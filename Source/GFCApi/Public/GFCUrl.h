#pragma once

#include "CoreMinimal.h"

/** URL helpers for the GFC API. Pure functions, unit-tested. */
struct GFCAPI_API FGFCUrl
{
	/** RFC 3986 percent-encoding: everything except A-Z a-z 0-9 - . _ ~ is encoded (UTF-8). */
	static FString Encode(const FString& Value);

	/** Joins a base URL and a path with exactly one slash: ("http://n/", "/feed") -> "http://n/feed". */
	static FString Join(const FString& BaseUrl, const FString& Path);

	/** Media paths from the node ("/thumbs/x.jpg") resolve against the node; absolute URLs pass through. */
	static FString ResolveMedia(const FString& BaseUrl, const FString& PathOrUrl);

	/** application/x-www-form-urlencoded body from ordered key/value pairs. */
	static FString FormBody(const TArray<TPair<FString, FString>>& Fields);

	/** Builds "path?k=v&k2=v2", skipping empty values. */
	static FString WithQuery(const FString& Path, const TArray<TPair<FString, FString>>& Query);

	/** Public share page: {ShareBaseUrl}/p/{seed id}. */
	static FString ShareUrl(const FString& ShareBaseUrl, const FString& SeedId);

	/** X web intent: https://x.com/intent/post?text=<text>&url=<url>, both encoded. */
	static FString XIntentUrl(const FString& Text, const FString& Url);

	/** Suggested share text, matching the node's: "<title> by <owner> on GFC". */
	static FString ShareText(const FString& Title, const FString& Owner);

	/** Normalises a user-entered node URL: trims, drops trailing slashes. Empty if not http(s). */
	static FString NormalizeBaseUrl(const FString& Url);
};
