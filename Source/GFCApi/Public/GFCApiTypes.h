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

	/** Registration only: the account's recovery code, shown to the user once (never stored by the app). */
	FString RecoveryCode;
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
	/** When the poster last edited the title, text or tags ("edited_at"); empty if never. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString EditedAt;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Owner;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString OwnerAvatar;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString RewardWallet;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bLiked = false;
	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bBookmarked = false;
	/** Set when this seed is a repost of someone else's original. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FGFCRepostInfo RepostOf;
	/**
	 * A GFC Exclusive ("exclusive"): only real GFC accounts see all of it (GET /seeds/{id}/play). For a
	 * video, Full is the public 5-second Teaser and Stream is empty; for a text post the title is public,
	 * Description is empty and Full is empty.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") bool bExclusive = false;
	/** An exclusive video's public 5-second teaser ("/teasers/<id>.mp4"); empty for text or if the cut failed. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC") FString Teaser;

	/** Classifies the media from its content type, falling back to the file extension. */
	EGFCMediaKind GetMediaKind() const;

	/** Only videos and text posts can be exclusive to the GFC Network (the node refuses others). */
	static bool CanBeExclusive(EGFCMediaKind Kind) { return Kind == EGFCMediaKind::Video || Kind == EGFCMediaKind::Text; }
};

/** GET /seeds/{id}/play: links to a post's full media. For exclusives they're signed and expire. */
struct GFCAPI_API FGFCPlayLinks
{
	bool bExclusive = false;
	FString Full;
	/** The streaming copy, when there is one. */
	FString Stream;
	FString Teaser;
	/** An exclusive text post's whole body. */
	FString Text;
	/** Seconds the signed links work for (0: they don't expire). */
	int32 ExpiresIn = 0;

	const FString& GetPlaybackPath() const { return Stream.IsEmpty() ? Full : Stream; }
};

/** May this account see a GFC Exclusive in full? */
enum class EGFCExclusiveAccess : uint8
{
	/** Not exclusive, the poster, or a real account: GET /seeds/{id}/play gives the full post. */
	Full,
	/** Logged in, but the app doesn't know yet (ask /play). */
	Unknown,
	NeedsLogin,
	/** The real network wants a verified phone (/play said 403). */
	NeedsVerify,
};

/** POST /seeds/upload "x_post": a ready-made X post for an exclusive's teaser. */
struct GFCAPI_API FGFCXPost
{
	FString Text;
	FString Url;
	/** https://x.com/intent/post?text=...&url=... */
	FString Intent;

	bool IsSet() const { return !Intent.IsEmpty() || !Url.IsEmpty(); }
	/** What to put next to the attached teaser: the text and the link. */
	FString ShareText() const { return Url.IsEmpty() ? Text : Text + TEXT(" ") + Url; }
};

/**
 * POST /seeds/{id}/edit: the poster's changes. Unset fields stay as they are; media can't change.
 * The node's limits: title 300 characters, description 20000, tags 1000 (comma-separated), 50 edits a post.
 */
struct GFCAPI_API FGFCSeedEditRequest
{
	TOptional<FString> Title;
	TOptional<FString> Description;
	TOptional<TArray<FString>> Tags;

	static constexpr int32 MaxTitle = 300;
	static constexpr int32 MaxDescription = 20000;
	static constexpr int32 MaxTags = 1000;
};

/** One version of a post: the edit's result, or an entry in GET /seeds/{id}/edits (newest first). */
struct GFCAPI_API FGFCSeedEdit
{
	FString Id;
	FString Title;
	FString Description;
	TArray<FString> Tags;
	FString EditedAt;
	/** Edit result only: false for an older unsigned post, where the edit shows only on this node. */
	bool bSynced = true;
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
	/** An exclusive post: the ready-made X post for its teaser (Seed.Teaser is the clip to attach). */
	FGFCXPost XPost;
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
/** GET /account/security: what the account can recover with (account_recovery.py on the node). */
struct GFCAPI_API FGFCAccountSecurity
{
	bool bRecoveryCode = false;
	bool bEmailVerified = false;
	/** Masked, e.g. a***@example.com. */
	FString Email;
	bool bPhoneVerified = false;
};

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

/** GET /health: which network the node is on and what it asks of posters (older nodes say neither). */
struct GFCAPI_API FGFCNodeHealth
{
	/** "mainnet" or "testnet"; empty on older nodes. */
	FString Network;
	/** The real network: uploads need a verified phone (403 otherwise). Test nets let anyone post. */
	bool bPhoneRequiredToPost = false;
};

/** One of the top earners in GET /podt/stats. */
struct GFCAPI_API FGFCTopEarner
{
	FString Wallet;
	/** The account name, when the node can confirm whose wallet it is ("owner"); else empty. */
	FString Owner;
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

/** One request, for the debug overlay. Never holds the token, bodies or query strings. */
struct GFCAPI_API FGFCApiTrace
{
	FString Verb;
	FString Path;
	int32 Status = -1; // -1 while in flight, 0 if the node couldn't be reached
	double StartedAt = 0.0;
	float Millis = 0.f;
};
