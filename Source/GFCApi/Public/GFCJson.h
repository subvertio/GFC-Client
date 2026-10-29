#pragma once

#include "CoreMinimal.h"
#include "GFCApiTypes.h"

class FJsonObject;

/**
 * Pure parsing of GFC node responses. No networking, so it's unit-tested directly
 * (see Private/Tests). Each Parse* returns false if the body isn't the expected shape.
 */
struct GFCAPI_API FGFCJson
{
	static TSharedPtr<FJsonObject> ParseObject(const FString& Body);

	static bool ParseAuth(const FString& Body, FGFCAuthResult& Out);
	static bool ParseProfile(const FString& Body, FGFCProfile& Out);
	static bool ParseWallet(const FString& Body, FGFCWallet& Out);
	/** POST /wallets/create returns {id, name, gfc_balance} instead of the GET shape. */
	static bool ParseCreatedWallet(const FString& Body, FGFCWallet& Out);
	static bool ParseTransaction(const FString& Body, FGFCTransaction& Out);
	static bool ParseHistory(const FString& Body, TArray<FGFCTransaction>& Out);
	static bool ParseSeed(const FString& Body, FGFCSeed& Out);
	/** GET /feed, /seeds and /seeds/mine: {"items": [...]} */
	static bool ParseFeed(const FString& Body, TArray<FGFCSeed>& Out);
	static bool ParseShareInfo(const FString& Body, FGFCShareInfo& Out);
	/** GET /auth/tokens: the id of the token marked current, for logging out. */
	static bool ParseCurrentTokenId(const FString& Body, FString& OutId);
	static bool ParsePlayResult(const FString& Body, FGFCPlayResult& Out);
	/** POST /seeds/upload, including repost_of when the node recognises a repost. */
	static bool ParseUploadResult(const FString& Body, FGFCUploadResult& Out);
	static bool ParseVerifyStatus(const FString& Body, FGFCVerifyStatus& Out);
	static bool ParseVerifyLink(const FString& Body, FGFCVerifyLink& Out);
	static bool ParseEmailLink(const FString& Body, FGFCVerifyLink& Out);
	static bool ParseAccountSecurity(const FString& Body, FGFCAccountSecurity& Out);
	static bool ParseRecoveryCode(const FString& Body, FString& Out);
	static bool ParseBlocks(const FString& Body, TArray<FString>& Out);
	/** GET /search: {"seeds": [...], "transactions": [...]}; only the seeds are read. */
	static bool ParseSearchSeeds(const FString& Body, TArray<FGFCSeed>& Out);
	/** POST /seeds/{id}/like ({"liked", "like_count"}) and /bookmark ({"bookmarked"}). */
	static bool ParseToggle(const FString& Body, FGFCToggleResult& Out);
	static bool ParseNetworkStats(const FString& Body, FGFCNetworkStats& Out);
	static void ReadRepost(const TSharedPtr<FJsonObject>& Obj, FGFCRepostInfo& Out);

	static void ReadSeed(const TSharedPtr<FJsonObject>& Obj, FGFCSeed& Out);
	/** An array of seeds under Field; seeds without an id are dropped. */
	static bool ReadSeeds(const FString& Body, const TCHAR* Field, TArray<FGFCSeed>& Out);
	static void ReadTransaction(const TSharedPtr<FJsonObject>& Obj, FGFCTransaction& Out);

	/**
	 * A user-facing message for a failed response. Understands FastAPI's
	 * {"detail": "..."} and validation errors {"detail": [{"loc": [...], "msg": "..."}]},
	 * and falls back to a generic message by status code.
	 */
	static FString ErrorMessage(int32 StatusCode, const FString& Body);

	/**
	 * Parses a user-typed GFC amount ("2", "2.5", "2.50") into cents. Rejects
	 * negatives, more than two decimals, non-numbers and zero.
	 */
	static bool ParseAmountToCents(const FString& Text, int64& OutCents);

	/** Formats cents as "12.34". */
	static FString FormatCents(int64 Cents);
};
