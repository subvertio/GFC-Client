#include "GFCApiClient.h"

#include "GFCApi.h"
#include "GFCApiSettings.h"
#include "GFCJson.h"
#include "GFCUrl.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Containers/Ticker.h"

namespace
{
	const TCHAR* FormContentType = TEXT("application/x-www-form-urlencoded");
	const TCHAR* JsonContentType = TEXT("application/json");

	/** Strips the query so logs never carry parameters. */
	FString PathForLog(const FString& PathAndQuery)
	{
		FString Path, Query;
		return PathAndQuery.Split(TEXT("?"), &Path, &Query) ? Path : PathAndQuery;
	}

	template <typename T>
	void FireLater(TFunction<void(const FGFCApiResult&, const T&)> Done, const FGFCApiResult& Result)
	{
		if (Done)
		{
			Done(Result, T{});
		}
	}
}

UGFCApiClient::UGFCApiClient()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		BaseUrl = UGFCApiSettings::GetEffectiveNodeBaseUrl();
		TimeoutSeconds = GetDefault<UGFCApiSettings>()->RequestTimeoutSeconds;
	}
}

void UGFCApiClient::SetBaseUrl(const FString& InBaseUrl)
{
	const FString Normalized = FGFCUrl::NormalizeBaseUrl(InBaseUrl);
	if (!Normalized.IsEmpty())
	{
		BaseUrl = Normalized;
	}
}

void UGFCApiClient::SetSession(const FString& InUsername, const FString& InToken)
{
	Username = InUsername;
	Token = InToken;
	OnSessionChanged.Broadcast();
}

void UGFCApiClient::ClearSession()
{
	const bool bHad = !Token.IsEmpty() || !Username.IsEmpty();
	Username.Reset();
	Token.Reset();
	BlockedUsers.Reset();
	ReportedSeeds.Reset();
	if (bHad)
	{
		OnSessionChanged.Broadcast();
	}
}

FString UGFCApiClient::ResolveMediaUrl(const FString& PathOrUrl) const
{
	return FGFCUrl::ResolveMedia(BaseUrl, PathOrUrl);
}

FString UGFCApiClient::BuildShareUrl(const FString& SeedId)
{
	return FGFCUrl::ShareUrl(UGFCApiSettings::GetEffectiveShareBaseUrl(), SeedId);
}

FString UGFCApiClient::BuildXIntentUrl(const FString& Text, const FString& Url)
{
	return FGFCUrl::XIntentUrl(Text, Url);
}

// ---------------------------------------------------------------------------
// Transport
// ---------------------------------------------------------------------------

void UGFCApiClient::Send(const FString& Verb, const FString& PathAndQuery, const FString& ContentType, const FString& Body,
	bool bAuth, TFunction<void(int32 Status, const FString& Body)> OnDone)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FGFCUrl::Join(BaseUrl, PathAndQuery));
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Accept"), JsonContentType);
	Request->SetTimeout(TimeoutSeconds);
	if (bAuth && !Token.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
	}
	if (!ContentType.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), ContentType);
		Request->SetContentAsString(Body);
	}

	const FString Logged = PathForLog(PathAndQuery);
	const int32 TraceId = BeginTrace(Verb, Logged);
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, Verb, Logged, TraceId, OnDone = MoveTemp(OnDone)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			const int32 Status = (bConnected && Response.IsValid()) ? Response->GetResponseCode() : 0;
			const FString RespBody = Response.IsValid() ? Response->GetContentAsString() : FString();
			UE_LOG(LogGFCApi, Verbose, TEXT("%s %s -> %d"), *Verb, *Logged, Status);
			if (WeakThis.IsValid())
			{
				WeakThis->EndTrace(TraceId, Status);
			}
			if (WeakThis.IsValid() && OnDone)
			{
				OnDone(Status, RespBody);
			}
		});
	Request->ProcessRequest();
}

template <typename T>
void UGFCApiClient::Call(const FString& Verb, const FString& PathAndQuery, const FString& ContentType, const FString& Body,
	bool bAuth, TFunction<bool(const FString&, T&)> Parse, TCallback<T> Done, EUnauthorized On401)
{
	if (bAuth && Token.IsEmpty())
	{
		FireLater<T>(MoveTemp(Done), FGFCApiResult::Failure(401, TEXT("Please log in first.")));
		return;
	}

	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Send(Verb, PathAndQuery, ContentType, Body, bAuth,
		[WeakThis, bAuth, On401, Parse = MoveTemp(Parse), Done = MoveTemp(Done)](int32 Status, const FString& RespBody)
		{
			T Value{};
			FGFCApiResult Result;
			if (Status >= 200 && Status < 300)
			{
				Result = FGFCApiResult::Success(Status);
				if (Parse && !Parse(RespBody, Value))
				{
					Result = FGFCApiResult::Failure(Status, TEXT("Unexpected response from the node."));
					Value = T{};
				}
			}
			else
			{
				Result = FGFCApiResult::Failure(Status, FGFCJson::ErrorMessage(Status, RespBody));
				if (Status == 401 && On401 == EUnauthorized::WrongPassword && FGFCJson::ErrorMessage(Status, RespBody).Contains(TEXT("invalid credentials")))
				{
					// The token is fine; the password typed for this action isn't. Stay logged in.
					Result.Error = TEXT("Wrong password.");
				}
				else if (Status == 401 && bAuth && WeakThis.IsValid())
				{
					WeakThis->ClearSession();
					WeakThis->OnUnauthorized.Broadcast();
				}
			}
			if (Done)
			{
				Done(Result, Value);
			}
		});
}

// ---------------------------------------------------------------------------
// Auth
// ---------------------------------------------------------------------------

void UGFCApiClient::HandleAuthResult(const FGFCApiResult& Result, const FGFCAuthResult& Auth)
{
	if (Result.bOk)
	{
		SetSession(Auth.Username, Auth.Token);
	}
}

void UGFCApiClient::Register(const FString& InUsername, const FString& Password, const FString& Email, TCallback<FGFCAuthResult> Done)
{
	TArray<TPair<FString, FString>> Fields{{TEXT("username"), InUsername.TrimStartAndEnd()}, {TEXT("password"), Password}};
	if (!Email.IsEmpty())
	{
		Fields.Add({TEXT("email"), Email.TrimStartAndEnd()});
	}
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Call<FGFCAuthResult>(TEXT("POST"), TEXT("/auth/register"), FormContentType, FGFCUrl::FormBody(Fields), false,
		&FGFCJson::ParseAuth,
		[WeakThis, Done = MoveTemp(Done)](const FGFCApiResult& R, const FGFCAuthResult& A)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleAuthResult(R, A);
			}
			if (Done) Done(R, A);
		});
}

void UGFCApiClient::Login(const FString& InUsername, const FString& Password, TCallback<FGFCAuthResult> Done)
{
	const FString Body = FGFCUrl::FormBody({{TEXT("username"), InUsername.TrimStartAndEnd()}, {TEXT("password"), Password}});
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Call<FGFCAuthResult>(TEXT("POST"), TEXT("/auth/login"), FormContentType, Body, false,
		&FGFCJson::ParseAuth,
		[WeakThis, Done = MoveTemp(Done)](const FGFCApiResult& R, const FGFCAuthResult& A)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleAuthResult(R, A);
			}
			if (Done) Done(R, A);
		});
}

void UGFCApiClient::Logout(TFunction<void(const FGFCApiResult&)> Done)
{
	if (Token.IsEmpty())
	{
		if (Done) Done(FGFCApiResult::Success());
		return;
	}
	// Find this token's id, revoke it, and forget the session whatever the node says:
	// the user asked to log out, and an unreachable node shouldn't keep them logged in.
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Call<FString>(TEXT("GET"), TEXT("/auth/tokens"), FString(), FString(), true, &FGFCJson::ParseCurrentTokenId,
		[WeakThis, Done](const FGFCApiResult& R, const FString& TokenId)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (!R.bOk || TokenId.IsEmpty())
			{
				WeakThis->ClearSession();
				if (Done) Done(FGFCApiResult::Success());
				return;
			}
			WeakThis->Call<bool>(TEXT("DELETE"), TEXT("/auth/tokens/") + FGFCUrl::Encode(TokenId), FString(), FString(), true,
				nullptr,
				[WeakThis, Done](const FGFCApiResult&, const bool&)
				{
					if (WeakThis.IsValid())
					{
						WeakThis->ClearSession();
					}
					if (Done) Done(FGFCApiResult::Success());
				});
		});
}

void UGFCApiClient::GetMe(TCallback<FGFCProfile> Done)
{
	Call<FGFCProfile>(TEXT("GET"), TEXT("/users/me"), FString(), FString(), true, &FGFCJson::ParseProfile, MoveTemp(Done));
}

// ---------------------------------------------------------------------------
// Wallets
// ---------------------------------------------------------------------------

void UGFCApiClient::CreateWallet(const FString& Name, TCallback<FGFCWallet> Done)
{
	const FString WalletName = Name.TrimStartAndEnd().IsEmpty() ? FString(TEXT("Default Wallet")) : Name.TrimStartAndEnd();
	Call<FGFCWallet>(TEXT("POST"), TEXT("/wallets/create"), FormContentType, FGFCUrl::FormBody({{TEXT("name"), WalletName}}),
		true, &FGFCJson::ParseCreatedWallet, MoveTemp(Done));
}

void UGFCApiClient::GetWallet(const FString& WalletId, TCallback<FGFCWallet> Done)
{
	Call<FGFCWallet>(TEXT("GET"), TEXT("/wallets/") + FGFCUrl::Encode(WalletId), FString(), FString(), false,
		&FGFCJson::ParseWallet, MoveTemp(Done));
}

void UGFCApiClient::GetHistory(const FString& WalletId, TCallback<TArray<FGFCTransaction>> Done)
{
	Call<TArray<FGFCTransaction>>(TEXT("GET"), FGFCUrl::WithQuery(TEXT("/transactions/history"), {{TEXT("wallet_id"), WalletId}}),
		FString(), FString(), false, &FGFCJson::ParseHistory, MoveTemp(Done));
}

void UGFCApiClient::CreateTransfer(const FString& SenderWalletId, const FString& RecipientWalletId, int64 AmountCents,
	const FString& Reason, TCallback<FGFCTransaction> Done)
{
	if (AmountCents <= 0)
	{
		FireLater<FGFCTransaction>(MoveTemp(Done), FGFCApiResult::Failure(400, TEXT("Enter an amount greater than zero.")));
		return;
	}
	if (SenderWalletId.IsEmpty() || RecipientWalletId.TrimStartAndEnd().IsEmpty())
	{
		FireLater<FGFCTransaction>(MoveTemp(Done), FGFCApiResult::Failure(400, TEXT("Choose a wallet and a recipient.")));
		return;
	}

	// The node converts with int(amount * 100), which truncates values like 0.29
	// (0.29 * 100 = 28.999...). A quarter-cent nudge makes truncation and rounding
	// both land on the exact cent count.
	const double Amount = (static_cast<double>(AmountCents) + 0.25) / 100.0;

	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("sender"), SenderWalletId);
	Json->SetStringField(TEXT("recipient"), RecipientWalletId.TrimStartAndEnd());
	Json->SetNumberField(TEXT("amount"), Amount);
	Json->SetStringField(TEXT("reason"), Reason);
	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Json, Writer);

	Call<FGFCTransaction>(TEXT("POST"), TEXT("/transactions/create"), JsonContentType, Body, true,
		&FGFCJson::ParseTransaction, MoveTemp(Done));
}

void UGFCApiClient::ExportWallets(const FString& Password, const FString& BackupPassphrase, TCallback<FString> Done)
{
	if (BackupPassphrase.Len() < 12)
	{
		FireLater<FString>(MoveTemp(Done), FGFCApiResult::Failure(400, TEXT("Use a backup passphrase of at least 12 characters.")));
		return;
	}
	const FString Body = FGFCUrl::FormBody({{TEXT("password"), Password}, {TEXT("backup_passphrase"), BackupPassphrase}});
	Call<FString>(TEXT("POST"), TEXT("/wallets/export"), FormContentType, Body, true,
		[](const FString& RespBody, FString& Out)
		{
			// Keep the node's JSON verbatim: /wallets/import expects exactly this document.
			const TSharedPtr<FJsonObject> Obj = FGFCJson::ParseObject(RespBody);
			if (!Obj.IsValid() || !Obj->HasField(TEXT("sealed")))
			{
				return false;
			}
			Out = RespBody;
			return true;
		},
		MoveTemp(Done), EUnauthorized::WrongPassword);
}

// ---------------------------------------------------------------------------
// Feed and sharing
// ---------------------------------------------------------------------------

void UGFCApiClient::GetFeed(TCallback<TArray<FGFCSeed>> Done)
{
	// Public, but with a token the node adds liked/bookmarked for this user.
	Call<TArray<FGFCSeed>>(TEXT("GET"), TEXT("/feed"), FString(), FString(), !Token.IsEmpty(), &FGFCJson::ParseFeed, MoveTemp(Done));
}

void UGFCApiClient::GetSeed(const FString& SeedId, TCallback<FGFCSeed> Done)
{
	Call<FGFCSeed>(TEXT("GET"), TEXT("/seeds/") + FGFCUrl::Encode(SeedId), FString(), FString(), false,
		&FGFCJson::ParseSeed, MoveTemp(Done));
}

void UGFCApiClient::GetShareInfo(const FString& SeedId, TCallback<FGFCShareInfo> Done)
{
	Call<FGFCShareInfo>(TEXT("GET"), TEXT("/p/") + FGFCUrl::Encode(SeedId) + TEXT("/share"), FString(), FString(), false,
		&FGFCJson::ParseShareInfo, MoveTemp(Done));
}

void UGFCApiClient::GetVerifyStatus(TCallback<bool> Done)
{
	Call<bool>(TEXT("GET"), TEXT("/verify/status"), FString(), FString(), true, &FGFCJson::ParseVerifyStatus, MoveTemp(Done));
}

void UGFCApiClient::CreateVerifyLink(TCallback<FGFCVerifyLink> Done)
{
	Call<FGFCVerifyLink>(TEXT("POST"), TEXT("/verify/link"), FormContentType, FString(), true, &FGFCJson::ParseVerifyLink,
		MoveTemp(Done));
}

void UGFCApiClient::ReportSeed(const FString& SeedId, const FString& Reason, TFunction<void(const FGFCApiResult&)> Done)
{
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Call<bool>(TEXT("POST"), TEXT("/seeds/") + FGFCUrl::Encode(SeedId) + TEXT("/report"), FormContentType,
		FGFCUrl::FormBody({{TEXT("reason"), Reason}}), true, nullptr,
		[WeakThis, SeedId, Done](const FGFCApiResult& R, const bool&)
		{
			if (R.bOk && WeakThis.IsValid()) WeakThis->ReportedSeeds.Add(SeedId);
			if (Done) Done(R);
		});
}

void UGFCApiClient::BlockUser(const FString& Name, TFunction<void(const FGFCApiResult&)> Done)
{
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Call<bool>(TEXT("POST"), TEXT("/users/") + FGFCUrl::Encode(Name) + TEXT("/block"), FormContentType, FString(), true, nullptr,
		[WeakThis, Name, Done](const FGFCApiResult& R, const bool&)
		{
			if (R.bOk && WeakThis.IsValid()) WeakThis->BlockedUsers.Add(Name.ToLower());
			if (Done) Done(R);
		});
}

void UGFCApiClient::UnblockUser(const FString& Name, TFunction<void(const FGFCApiResult&)> Done)
{
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Call<bool>(TEXT("DELETE"), TEXT("/users/") + FGFCUrl::Encode(Name) + TEXT("/block"), FString(), FString(), true, nullptr,
		[WeakThis, Name, Done](const FGFCApiResult& R, const bool&)
		{
			if (R.bOk && WeakThis.IsValid()) WeakThis->BlockedUsers.Remove(Name.ToLower());
			if (Done) Done(R);
		});
}

void UGFCApiClient::RefreshBlocks(TFunction<void(const FGFCApiResult&)> Done)
{
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Call<TArray<FString>>(TEXT("GET"), TEXT("/users/me/blocks"), FString(), FString(), true, &FGFCJson::ParseBlocks,
		[WeakThis, Done](const FGFCApiResult& R, const TArray<FString>& Names)
		{
			if (R.bOk && WeakThis.IsValid())
			{
				WeakThis->BlockedUsers.Reset();
				for (const FString& N : Names) WeakThis->BlockedUsers.Add(N.ToLower());
			}
			if (Done) Done(R);
		});
}

bool UGFCApiClient::IsHiddenForMe(const FGFCSeed& Seed) const
{
	return ReportedSeeds.Contains(Seed.Id) || BlockedUsers.Contains(Seed.Owner.ToLower());
}

void UGFCApiClient::RecordPlay(const FString& SeedId, TCallback<FGFCPlayResult> Done)
{
	Call<FGFCPlayResult>(TEXT("POST"), TEXT("/seeds/") + FGFCUrl::Encode(SeedId) + TEXT("/played"), FormContentType, FString(), true,
		&FGFCJson::ParsePlayResult, MoveTemp(Done));
}

int32 UGFCApiClient::BeginTrace(const FString& Verb, const FString& Path)
{
	FGFCApiTrace T;
	T.Verb = Verb;
	T.Path = Path;
	T.StartedAt = FPlatformTime::Seconds();
	RecentCalls.Add(T);
	RecentCallIds.Add(NextTraceId);
	while (RecentCalls.Num() > 30)
	{
		RecentCalls.RemoveAt(0);
		RecentCallIds.RemoveAt(0);
	}
	return NextTraceId++;
}

void UGFCApiClient::EndTrace(int32 TraceId, int32 Status)
{
	const int32 Index = RecentCallIds.IndexOfByKey(TraceId);
	if (RecentCalls.IsValidIndex(Index))
	{
		RecentCalls[Index].Status = Status;
		RecentCalls[Index].Millis = float((FPlatformTime::Seconds() - RecentCalls[Index].StartedAt) * 1000.0);
	}
}

int32 UGFCApiClient::GetInFlightCount() const
{
	int32 N = 0;
	for (const FGFCApiTrace& T : RecentCalls)
	{
		N += T.Status < 0 ? 1 : 0;
	}
	return N;
}

TSharedRef<FGFCUploadJob> UGFCApiClient::Upload(const FGFCUploadRequest& Request, TFunction<void(int64, int64)> Progress,
	TCallback<FGFCUploadResult> Done)
{
	TSharedRef<FGFCUploadJob> Job = MakeShared<FGFCUploadJob>();
	TSharedRef<FGFCMultipart> Body = MakeShared<FGFCMultipart>();
	if (Token.IsEmpty())
	{
		if (Done) Done(FGFCApiResult::Failure(401, TEXT("Please log in first.")), FGFCUploadResult());
		return Job;
	}
	if (!FGFCMultipart::Build(Request, *Body))
	{
		if (Done) Done(FGFCApiResult::Failure(400, TEXT("The file couldn't be read.")), FGFCUploadResult());
		return Job;
	}
	if (Body->TotalSize() > MaxUploadBytes)
	{
		if (Done) Done(FGFCApiResult::Failure(413, FString::Printf(TEXT("This file is %.0f MB; uploads are limited to %lld MB."),
			Body->TotalSize() / 1048576.0, MaxUploadBytes / (1024 * 1024))), FGFCUploadResult());
		return Job;
	}
	StartUploadAttempt(Job, Body, MoveTemp(Progress), MoveTemp(Done));
	return Job;
}

void UGFCApiClient::StartUploadAttempt(TSharedRef<FGFCUploadJob> Job, TSharedRef<FGFCMultipart> Body,
	TFunction<void(int64, int64)> Progress, TCallback<FGFCUploadResult> Done)
{
	++Job->Attempt;
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FGFCUrl::Join(BaseUrl, TEXT("/seeds/upload")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Accept"), JsonContentType);
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
	Request->SetHeader(TEXT("Content-Type"), Body->ContentType());
	Request->SetContentFromStream(MakeShared<FGFCMultipartArchive, ESPMode::ThreadSafe>(*Body));
	// Slow phones on slow networks: no overall timeout, but give up if nothing moves for a minute.
	Request->SetActivityTimeout(60.f);
	Job->Request = Request;

	const int64 Total = Body->TotalSize();
	if (Progress)
	{
		Request->OnRequestProgress64().BindLambda([Progress, Total](FHttpRequestPtr, uint64 Sent, uint64)
		{
			Progress(int64(Sent), Total);
		});
	}
	const int32 TraceId = BeginTrace(TEXT("POST"), TEXT("/seeds/upload"));
	TWeakObjectPtr<UGFCApiClient> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, Job, Body, Progress, Done, TraceId](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			const int32 Status = (bConnected && Response.IsValid()) ? Response->GetResponseCode() : 0;
			const FString RespBody = Response.IsValid() ? Response->GetContentAsString() : FString();
			WeakThis->EndTrace(TraceId, Status);
			if (Job->bCancelled)
			{
				if (Done) Done(FGFCApiResult::Failure(0, TEXT("Upload cancelled.")), FGFCUploadResult());
				return;
			}
			if (Status >= 200 && Status < 300)
			{
				FGFCUploadResult Result;
				if (FGFCJson::ParseUploadResult(RespBody, Result))
				{
					if (Done) Done(FGFCApiResult::Success(Status), Result);
				}
				else if (Done)
				{
					Done(FGFCApiResult::Failure(Status, TEXT("Unexpected response from the node.")), FGFCUploadResult());
				}
				return;
			}
			// Flaky connection or a busy node: try again with backoff. 4xx answers are final.
			const bool bRetryable = Status == 0 || Status == 429 || Status >= 500;
			if (bRetryable && Job->Attempt < MaxUploadAttempts)
			{
				const float Delay = Job->Attempt == 1 ? 2.f : 6.f;
				UE_LOG(LogGFCApi, Log, TEXT("Upload attempt %d failed (%d); retrying in %.0f s."), Job->Attempt, Status, Delay);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Job, Body, Progress, Done](float)
				{
					if (WeakThis.IsValid() && !Job->bCancelled)
					{
						WeakThis->StartUploadAttempt(Job, Body, Progress, Done);
					}
					else if (Done)
					{
						Done(FGFCApiResult::Failure(0, TEXT("Upload cancelled.")), FGFCUploadResult());
					}
					return false;
				}), Delay);
				return;
			}
			FGFCApiResult R = FGFCApiResult::Failure(Status, FGFCJson::ErrorMessage(Status, RespBody));
			if (Status == 413)
			{
				R.Error = TEXT("The file is too big for the node.");
			}
			if (Status == 401)
			{
				WeakThis->ClearSession();
				WeakThis->OnUnauthorized.Broadcast();
			}
			if (Done) Done(R, FGFCUploadResult());
		});
	Request->ProcessRequest();
}

void UGFCApiClient::DownloadBytes(const FString& Url, int64 MaxBytes, TFunction<void(bool, const TArray<uint8>&)> Done)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(TimeoutSeconds);
	Request->OnProcessRequestComplete().BindLambda(
		[MaxBytes, Done = MoveTemp(Done)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			const bool bOk = bConnected && Response.IsValid() && Response->GetResponseCode() == 200
				&& Response->GetContent().Num() <= MaxBytes;
			if (Done)
			{
				Done(bOk, bOk ? Response->GetContent() : TArray<uint8>());
			}
		});
	Request->ProcessRequest();
}

// ---------------------------------------------------------------------------
// Blueprint wrappers
// ---------------------------------------------------------------------------

void UGFCApiClient::K2_Register(const FString& InUsername, const FString& Password, const FString& Email, const FGFCOnAuth& OnComplete)
{
	Register(InUsername, Password, Email, [OnComplete](const FGFCApiResult& R, const FGFCAuthResult& A) { OnComplete.ExecuteIfBound(R, A); });
}

void UGFCApiClient::K2_Login(const FString& InUsername, const FString& Password, const FGFCOnAuth& OnComplete)
{
	Login(InUsername, Password, [OnComplete](const FGFCApiResult& R, const FGFCAuthResult& A) { OnComplete.ExecuteIfBound(R, A); });
}

void UGFCApiClient::K2_Logout(const FGFCOnResult& OnComplete)
{
	Logout([OnComplete](const FGFCApiResult& R) { OnComplete.ExecuteIfBound(R); });
}

void UGFCApiClient::K2_GetMe(const FGFCOnProfile& OnComplete)
{
	GetMe([OnComplete](const FGFCApiResult& R, const FGFCProfile& P) { OnComplete.ExecuteIfBound(R, P); });
}

void UGFCApiClient::K2_CreateWallet(const FString& Name, const FGFCOnWallet& OnComplete)
{
	CreateWallet(Name, [OnComplete](const FGFCApiResult& R, const FGFCWallet& W) { OnComplete.ExecuteIfBound(R, W); });
}

void UGFCApiClient::K2_GetWallet(const FString& WalletId, const FGFCOnWallet& OnComplete)
{
	GetWallet(WalletId, [OnComplete](const FGFCApiResult& R, const FGFCWallet& W) { OnComplete.ExecuteIfBound(R, W); });
}

void UGFCApiClient::K2_GetHistory(const FString& WalletId, const FGFCOnHistory& OnComplete)
{
	GetHistory(WalletId, [OnComplete](const FGFCApiResult& R, const TArray<FGFCTransaction>& T) { OnComplete.ExecuteIfBound(R, T); });
}

void UGFCApiClient::K2_CreateTransfer(const FString& SenderWalletId, const FString& RecipientWalletId, const FString& Amount,
	const FString& Reason, const FGFCOnTransaction& OnComplete)
{
	int64 Cents = 0;
	if (!FGFCJson::ParseAmountToCents(Amount, Cents))
	{
		OnComplete.ExecuteIfBound(FGFCApiResult::Failure(400, TEXT("Enter an amount like 2.50 (up to two decimals).")), FGFCTransaction());
		return;
	}
	CreateTransfer(SenderWalletId, RecipientWalletId, Cents, Reason,
		[OnComplete](const FGFCApiResult& R, const FGFCTransaction& T) { OnComplete.ExecuteIfBound(R, T); });
}

void UGFCApiClient::K2_ExportWallets(const FString& Password, const FString& BackupPassphrase, const FGFCOnBackup& OnComplete)
{
	ExportWallets(Password, BackupPassphrase, [OnComplete](const FGFCApiResult& R, const FString& B) { OnComplete.ExecuteIfBound(R, B); });
}

void UGFCApiClient::K2_GetFeed(const FGFCOnFeed& OnComplete)
{
	GetFeed([OnComplete](const FGFCApiResult& R, const TArray<FGFCSeed>& S) { OnComplete.ExecuteIfBound(R, S); });
}

void UGFCApiClient::K2_GetSeed(const FString& SeedId, const FGFCOnSeed& OnComplete)
{
	GetSeed(SeedId, [OnComplete](const FGFCApiResult& R, const FGFCSeed& S) { OnComplete.ExecuteIfBound(R, S); });
}

void UGFCApiClient::K2_GetShareInfo(const FString& SeedId, const FGFCOnShareInfo& OnComplete)
{
	GetShareInfo(SeedId, [OnComplete](const FGFCApiResult& R, const FGFCShareInfo& S) { OnComplete.ExecuteIfBound(R, S); });
}
