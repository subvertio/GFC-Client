#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FGFCOnSeedLink, const FString& /*SeedId*/);

/**
 * Links that open a seed in the app:
 *   https://{app link host}/p/{id}   (Android App Link, what people share on X)
 *   gfc://seed/{id}                  (custom scheme fallback)
 *
 * Where they come from:
 *   Android: the intent that launched the app or onNewIntent (GFCApi_UPL.xml).
 *   PC: a command-line argument, so links can be tested without a phone:
 *     UnrealEditor.exe GFCClient.uproject -game "gfc://seed/abc123"
 *     ... -GFCLink=https://node.galacticfederation.tech/p/abc123
 */
struct GFCAPI_API FGFCDeepLink
{
	/** Seed ids are short tokens: 1-128 of A-Z a-z 0-9 - _. Anything else is rejected. */
	static bool IsValidSeedId(const FString& Id);

	/** Extracts the seed id from a link, or returns false for anything that isn't a valid seed link. */
	static bool ParseSeedId(const FString& Url, const TArray<FString>& AllowedHosts, const FString& Scheme, FString& OutSeedId);

	/** Same, with hosts and scheme from UGFCApiSettings. */
	static bool ParseSeedId(const FString& Url, FString& OutSeedId);

	/** The first seed link on the PC command line, if any. */
	static bool FindInCommandLine(const TCHAR* CommandLine, FString& OutSeedId);

	/** At startup: queues the link the app was launched with (Android intent, or PC command line). */
	static void CollectLaunchLink();

	/** Queues a link from the platform (any thread). Invalid links are ignored. */
	static void Receive(const FString& Url);

	/** Takes the link that launched the app or arrived since the last call. Game thread. */
	static bool TakePending(FString& OutSeedId);

	/** Fired on the game thread when a link arrives while the app is running. */
	static FGFCOnSeedLink OnSeedLink;
};
