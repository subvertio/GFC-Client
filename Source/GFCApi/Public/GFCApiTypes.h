#pragma once

#include "CoreMinimal.h"
#include "GFCApiTypes.generated.h"

/** Outcome of an API call. On failure, Error is a message safe to show the user (never contains the token). */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCApiResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC")
	bool bOk = false;

	/** HTTP status, or 0 when the node could not be reached. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC")
	int32 StatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GFC")
	FString Error;

	/** 401: the token is missing, invalid, expired or revoked. Send the user back to login. */
	bool IsAuthError() const { return StatusCode == 401; }

	/** 403: the token lacks the needed scope, the spend limit was reached, or the wallet isn't the caller's. */
	bool IsForbidden() const { return StatusCode == 403; }

	static FGFCApiResult Success(int32 InStatus = 200)
	{
		FGFCApiResult R;
		R.bOk = true;
		R.StatusCode = InStatus;
		return R;
	}

	static FGFCApiResult Failure(int32 InStatus, const FString& InError)
	{
		FGFCApiResult R;
		R.bOk = false;
		R.StatusCode = InStatus;
		R.Error = InError;
		return R;
	}
};

/** POST /auth/login and /auth/register. The token is a credential: never log it. */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCAuthResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC")
	FString Username;

	FString Token;
};

/** GET /users/me */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Username;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString FirstName;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString LastName;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Bio;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Avatar;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString CreatedAt;
};

/** GET /wallets/{id} and POST /wallets/create. The ID is the wallet's Ed25519 public key. */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCWallet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Name;

	/** Exact balance in cents. Prefer this over Balance for arithmetic. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") int64 BalanceCents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GFC") double Balance = 0.0;
};

/** An entry from GET /transactions/history or the result of POST /transactions/create. */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCTransaction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Sender;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Recipient;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") int64 AmountCents = 0;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") double Amount = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Reason;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Timestamp;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString TxHash;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString TxType;

	/** -1 when the transaction isn't in a block yet. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") int64 BlockIndex = -1;

	/** Only reported by /transactions/create: false for legacy wallets without a key. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bSigned = false;
};

UENUM(BlueprintType)
enum class EGFCMediaKind : uint8
{
	Other,
	Image,
	Video,
	Audio,
	Model3D,
	/** A text post: a small .txt seed whose description holds the text. */
	Text,
};

/** "Repost of": a seed whose content matched an earlier original. Credit and earnings go to the original. */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCRepostInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString SeedId;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Owner;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Url;

	bool IsSet() const { return !SeedId.IsEmpty(); }
};

/** A seed from GET /feed or GET /seeds/{id}. Media paths (Full, Thumb) are relative to the node. */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCSeed
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Description;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") TArray<FString> Tags;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Full;
	/**
	 * A smaller copy for streaming, when the node makes one ("stream"; null until it exists): AAC for a big
	 * WAV, or for video an H.264 MP4 at most 720p with any rotation baked into the pixels. Players prefer it.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Stream;
	/** What to play: the streaming copy when there is one, else the original. */
	const FString& GetPlaybackPath() const { return Stream.IsEmpty() ? Full : Stream; }
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Thumb;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString ContentType;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") int64 FileSize = 0;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") double GfcEarned = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") int32 Likes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") int32 Comments = 0;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") int32 Views = 0;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString CreatedAt;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Owner;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString OwnerAvatar;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString RewardWallet;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bLiked = false;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bBookmarked = false;
	/** Set when this seed is a repost of someone else's original. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FGFCRepostInfo RepostOf;

	/** Classifies the media from its content type, falling back to the file extension. */
	EGFCMediaKind GetMediaKind() const;
};

/** GET /p/{id}/share */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCShareInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Url;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString XIntentUrl;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString AppLink;
};

/** POST /seeds/{id}/played. Not crediting is normal (own seed, daily limit, unverified phone...). */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCPlayResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bCredited = false;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") double Amount = 0.0;
	/** Why it didn't credit, e.g. "self-play" or "daily limit for this creator reached". */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Reason;
};

/** POST /seeds/upload */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCUploadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GFC") FGFCSeed Seed;
	/** True when the node signed the seed with a reward wallet, so it can earn. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bRewardsEnabled = false;
	/** Set when the node matched the upload to an earlier original: the original's creator gets the credit. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FGFCRepostInfo RepostOf;
	/** GFC paid for the account's first post ("first_post_bonus"), or 0. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") double FirstPostBonus = 0.0;
	/** The first-post bonus is waiting for the account's first wallet ("first_post_bonus_pending"). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bFirstPostBonusPending = false;
};

/** GET /verify/status: is the phone verified, what did verifying pay, and the account's wallets on the node. */
struct GFCAPI_API FGFCVerifyStatus
{
	bool bVerified = false;
	/** GFC paid for verifying: the welcome grant and the first post bonus (0 on older nodes). */
	double Grant = 0.0;
	double FirstPostBonus = 0.0;
	/** The account's wallet IDs on the node (empty on older nodes). */
	TArray<FString> Wallets;

	double Rewards() const { return Grant + FirstPostBonus; }
};

/** POST /verify/link: a one-time page to verify the account's phone in the browser. */
struct GFCAPI_API FGFCVerifyLink
{
	/** Already verified: nothing to open. */
	bool bVerified = false;
	FString Url;
};

/** POST /seeds/{id}/like and /bookmark: the new state (the node toggles), and the like count. */
struct GFCAPI_API FGFCToggleResult
{
	bool bOn = false;
	/** Likes after the toggle; -1 when the node didn't say (bookmarks). */
	int32 Count = -1;
};

/** One of the top earners in GET /podt/stats (by wallet: the node doesn't name owners). */
struct GFCAPI_API FGFCTopEarner
{
	FString Wallet;
	double EarnedGfc = 0.0;
	int32 Transfers = 0;
};

/** GET /podt/stats: what the network has paid creators for views, and who earned most. */
struct GFCAPI_API FGFCNetworkStats
{
	double TotalRewardGfc = 0.0;
	int64 TotalTransfers = 0;
	TArray<FGFCTopEarner> TopEarners;
};

/** One chain from GET /bridge/info: where wrapped GFC lives, and the ledger escrow that backs it. */
struct GFCAPI_API FGFCBridgeChain
{
	int64 ChainId = 0;
	FString Name;
	/** The ledger wallet deposits go to (the node signs them; never shown as a send target). */
	FString EscrowWallet;
	/** The ERC-20 contract, or empty until the node operator publishes it: don't bridge before then. */
	FString TokenAddress;
};

/** GET /bridge/info: the chains this node bridges to, and the deposit rules. */
struct GFCAPI_API FGFCBridgeInfo
{
	bool bEnabled = false;
	TArray<FGFCBridgeChain> Chains;
	int64 MinDepositCents = 0;
	/** Taken out of the minted amount. */
	int64 RelayFeeCents = 0;
	/** Free credit (welcome grant, invite and post bonuses) can be bridged this many days after it arrives. */
	double FaucetHoldDays = 0.0;
	/** Shown next to the button: not an investment, unaudited, start small. */
	FString Notice;
};

/** GET /bridge/quote: how much of a wallet's balance can be bridged now. */
struct GFCAPI_API FGFCBridgeQuote
{
	int64 BalanceCents = 0;
	int64 BridgeableCents = 0;
	/** Fresh free credit that has to age first. */
	int64 HeldCents = 0;
};

/** POST /bridge/deposit: the signed escrow deposit. Tokens are minted later, after the finality wait. */
struct GFCAPI_API FGFCBridgeDeposit
{
	FString TxId;
	FString TxHash;
	int64 ChainId = 0;
	FString EvmAddress;
	int64 AmountCents = 0;
	int64 MintCents = 0;
	FString Status;
};

/** One request, for the debug overlay. Never holds the token, bodies or query strings. */
struct GFCAPI_API FGFCApiTrace
{
	FString Verb;
	FString Path;
	int32 Status = -1; // -1 while in flight, 0 if the node couldn't be reached
	double StartedAt = 0.0;
	float Millis = 0.f;
};
