#include "GFCDeepLink.h"

#include "Async/Async.h"
#include "GFCApi.h"
#include "GFCApiSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"

FGFCOnSeedLink FGFCDeepLink::OnSeedLink;
FGFCOnVerifiedLink FGFCDeepLink::OnVerifiedLink;

#if PLATFORM_ANDROID
namespace GFCAndroid { void ConsumeLaunchLink(); }
#endif

namespace
{
	FCriticalSection GPendingLock;
	FString GPendingSeedId;

	/** Drops "?query" and "#fragment". */
	FString StripQuery(const FString& S)
	{
		int32 Cut = S.Len();
		int32 Q, H;
		if (S.FindChar(TEXT('?'), Q)) Cut = FMath::Min(Cut, Q);
		if (S.FindChar(TEXT('#'), H)) Cut = FMath::Min(Cut, H);
		return S.Left(Cut);
	}
}

bool FGFCDeepLink::IsValidSeedId(const FString& Id)
{
	if (Id.IsEmpty() || Id.Len() > 128)
	{
		return false;
	}
	for (const TCHAR C : Id)
	{
		const bool bOk = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '-' || C == '_';
		if (!bOk)
		{
			return false;
		}
	}
	return true;
}

bool FGFCDeepLink::ParseSeedId(const FString& Url, const TArray<FString>& AllowedHosts, const FString& Scheme, FString& OutSeedId)
{
	const FString Trimmed = Url.TrimStartAndEnd().TrimQuotes();
	if (Trimmed.IsEmpty() || Trimmed.Len() > 2048)
	{
		return false;
	}
	FString UrlScheme, Rest;
	if (!Trimmed.Split(TEXT("://"), &UrlScheme, &Rest))
	{
		return false;
	}
	Rest = StripQuery(Rest);

	FString Authority, Path;
	if (!Rest.Split(TEXT("/"), &Authority, &Path))
	{
		return false;
	}
	while (Path.EndsWith(TEXT("/")))
	{
		Path.LeftChopInline(1);
	}

	FString Candidate;
	if (UrlScheme.Equals(TEXT("https"), ESearchCase::IgnoreCase))
	{
		// No user info or ports: only the exact app link hosts.
		if (Authority.Contains(TEXT("@")) || Authority.Contains(TEXT(":")))
		{
			return false;
		}
		const bool bHostOk = AllowedHosts.ContainsByPredicate([&](const FString& H) { return H.Equals(Authority, ESearchCase::IgnoreCase); });
		if (!bHostOk || !Path.StartsWith(TEXT("p/")))
		{
			return false;
		}
		Candidate = Path.RightChop(2);
	}
	else if (!Scheme.IsEmpty() && UrlScheme.Equals(Scheme, ESearchCase::IgnoreCase))
	{
		if (!Authority.Equals(TEXT("seed"), ESearchCase::IgnoreCase))
		{
			return false;
		}
		Candidate = Path;
	}
	else
	{
		return false;
	}

	// One path segment only, and nothing encoded or exotic.
	if (!IsValidSeedId(Candidate))
	{
		return false;
	}
	OutSeedId = Candidate;
	return true;
}

bool FGFCDeepLink::ParseSeedId(const FString& Url, FString& OutSeedId)
{
	const UGFCApiSettings* S = GetDefault<UGFCApiSettings>();
	return ParseSeedId(Url, S->AppLinkHosts, S->AppLinkScheme, OutSeedId);
}

bool FGFCDeepLink::FindInCommandLine(const TCHAR* CommandLine, FString& OutSeedId)
{
	FString Value;
	if (FParse::Value(CommandLine, TEXT("GFCLink="), Value) && ParseSeedId(Value, OutSeedId))
	{
		return true;
	}
	const TCHAR* Cursor = CommandLine;
	FString Token;
	while (FParse::Token(Cursor, Token, false))
	{
		if (ParseSeedId(Token, OutSeedId))
		{
			return true;
		}
	}
	return false;
}

void FGFCDeepLink::CollectLaunchLink()
{
#if PLATFORM_ANDROID
	GFCAndroid::ConsumeLaunchLink();
#else
	FString SeedId;
	if (FindInCommandLine(FCommandLine::Get(), SeedId))
	{
		FScopeLock Lock(&GPendingLock);
		GPendingSeedId = SeedId;
	}
#endif
}

bool FGFCDeepLink::IsVerifiedLink(const FString& Url, const FString& Scheme)
{
	FString UrlScheme, Rest;
	if (Scheme.IsEmpty() || !Url.TrimStartAndEnd().Split(TEXT("://"), &UrlScheme, &Rest) || !UrlScheme.Equals(Scheme, ESearchCase::IgnoreCase))
	{
		return false;
	}
	Rest = StripQuery(Rest);
	FString Host = Rest, Path;
	Rest.Split(TEXT("/"), &Host, &Path);
	return Host.Equals(TEXT("verified"), ESearchCase::IgnoreCase);
}

void FGFCDeepLink::Receive(const FString& Url)
{
	if (IsVerifiedLink(Url, GetDefault<UGFCApiSettings>()->AppLinkScheme))
	{
		// Arrives while the app runs (the browser hands back): the root refreshes the balance.
		AsyncTask(ENamedThreads::GameThread, []() { OnVerifiedLink.Broadcast(); });
		return;
	}
	FString SeedId;
	if (!ParseSeedId(Url, SeedId))
	{
		UE_LOG(LogGFCApi, Log, TEXT("Ignored a link that isn't a seed link."));
		return;
	}
	{
		FScopeLock Lock(&GPendingLock);
		GPendingSeedId = SeedId;
	}
	AsyncTask(ENamedThreads::GameThread, []()
	{
		if (OnSeedLink.IsBound())
		{
			FString Id;
			if (TakePending(Id))
			{
				OnSeedLink.Broadcast(Id);
			}
		}
	});
}

bool FGFCDeepLink::TakePending(FString& OutSeedId)
{
	FScopeLock Lock(&GPendingLock);
	if (GPendingSeedId.IsEmpty())
	{
		return false;
	}
	OutSeedId = MoveTemp(GPendingSeedId);
	GPendingSeedId.Reset();
	return true;
}
