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
