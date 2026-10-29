#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GFCSessionStore.generated.h"

/** A wallet this device knows belongs to the user (the node has no "list my wallets" endpoint yet). */
USTRUCT(BlueprintType)
struct GFCAPI_API FGFCKnownWallet
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "GFC") FString Owner;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "GFC") FString NodeBaseUrl;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "GFC") FString WalletId;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "GFC") FString Name;

	/** Set once the user exported a backup that includes this wallet. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "GFC") bool bBackedUp = false;

	/** The balance the user last saw, in cents; -1 until the app has seen it once. Drives "you earned" on login. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "GFC") int64 LastSeenCents = -1;
};

/**
 * Local session data, saved to the "GFCSession" SaveGame slot.
 * The token is stored only in protected form (see FGFCSecureStore).
 */
UCLASS()
class GFCAPI_API UGFCSessionSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame) FString Username;
	UPROPERTY(SaveGame) FString NodeBaseUrl;
	UPROPERTY(SaveGame) TArray<uint8> ProtectedToken;

	/** The node the user chose on the login screen, if different from the project default. */
	UPROPERTY(SaveGame) FString NodeOverride;

	UPROPERTY(SaveGame) TArray<FGFCKnownWallet> KnownWallets;
};

/**
 * Protects secrets at rest as well as the platform allows.
 *
 * - Windows: DPAPI (CryptProtectData), bound to the current Windows user.
 * - Android: AES-256-GCM with a key generated inside the Android Keystore that never
 *   leaves it (GFCApi_UPL.xml). A copied save file (backup, device transfer, rooted
 *   phone) can't be decrypted elsewhere; the user just logs in again.
 * - Other platforms: plain bytes, with a warning.
 */
struct GFCAPI_API FGFCSecureStore
{
	static bool Protect(const FString& Secret, TArray<uint8>& OutBlob);
	static bool Unprotect(const TArray<uint8>& Blob, FString& OutSecret);

	/** True for a blob saved in plain form by an older build, which should be saved again protected. */
	static bool NeedsUpgrade(const TArray<uint8>& Blob);

	/** True when Protect uses real OS protection rather than plain storage. */
	static bool IsHardwareOrOsBacked();
};
