#include "GFCUrl.h"

FString FGFCUrl::Encode(const FString& Value)
{
	const FTCHARToUTF8 Utf8(*Value);
	FString Out;
	Out.Reserve(Utf8.Length() * 3);
	for (int32 i = 0; i < Utf8.Length(); ++i)
	{
		const uint8 C = static_cast<uint8>(Utf8.Get()[i]);
		const bool bUnreserved = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9')
			|| C == '-' || C == '.' || C == '_' || C == '~';
		if (bUnreserved)
		{
			Out.AppendChar(static_cast<TCHAR>(C));
		}
		else
		{
			Out += FString::Printf(TEXT("%%%02X"), C);
		}
	}
	return Out;
}

FString FGFCUrl::Join(const FString& BaseUrl, const FString& Path)
{
	FString Base = BaseUrl;
	while (Base.EndsWith(TEXT("/")))
	{
		Base.LeftChopInline(1);
	}
	if (Path.IsEmpty())
	{
		return Base;
	}
	return Path.StartsWith(TEXT("/")) ? Base + Path : Base + TEXT("/") + Path;
}

FString FGFCUrl::ResolveMedia(const FString& BaseUrl, const FString& PathOrUrl)
{
	if (PathOrUrl.IsEmpty())
	{
		return FString();
	}
	if (PathOrUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) || PathOrUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
	{
		return PathOrUrl;
	}
	return Join(BaseUrl, PathOrUrl);
}

FString FGFCUrl::FormBody(const TArray<TPair<FString, FString>>& Fields)
{
	TArray<FString> Parts;
	Parts.Reserve(Fields.Num());
	for (const TPair<FString, FString>& F : Fields)
	{
		Parts.Add(Encode(F.Key) + TEXT("=") + Encode(F.Value));
	}
	return FString::Join(Parts, TEXT("&"));
}

FString FGFCUrl::WithQuery(const FString& Path, const TArray<TPair<FString, FString>>& Query)
{
	TArray<FString> Parts;
	for (const TPair<FString, FString>& Q : Query)
	{
		if (!Q.Value.IsEmpty())
		{
			Parts.Add(Encode(Q.Key) + TEXT("=") + Encode(Q.Value));
		}
	}
	return Parts.Num() ? Path + TEXT("?") + FString::Join(Parts, TEXT("&")) : Path;
}

FString FGFCUrl::ShareUrl(const FString& ShareBaseUrl, const FString& SeedId)
{
	return Join(ShareBaseUrl, TEXT("/p/") + Encode(SeedId));
}

FString FGFCUrl::XIntentUrl(const FString& Text, const FString& Url)
{
	return FString::Printf(TEXT("https://x.com/intent/post?text=%s&url=%s"), *Encode(Text), *Encode(Url));
}

FString FGFCUrl::ShareText(const FString& Title, const FString& Owner)
{
	const FString T = Title.IsEmpty() ? FString(TEXT("a seed")) : Title;
	return Owner.IsEmpty() ? FString::Printf(TEXT("%s on GFC"), *T) : FString::Printf(TEXT("%s by %s on GFC"), *T, *Owner);
}

FString FGFCUrl::NormalizeBaseUrl(const FString& Url)
{
	FString U = Url.TrimStartAndEnd();
	if (!U.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) && !U.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
	{
		return FString();
	}
	while (U.EndsWith(TEXT("/")))
	{
		U.LeftChopInline(1);
	}
	// Scheme with no host ("https:/") isn't a URL.
	return U.Contains(TEXT("://")) && U.Len() > U.Find(TEXT("://")) + 3 ? U : FString();
}
