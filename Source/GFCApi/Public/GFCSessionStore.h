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
 * - Android: TODO(android-keystore): wrap with an AES key held in the Android
 *   Keystore (via JNI / UPL, KeyGenParameterSpec with PURPOSE_ENCRYPT|DECRYPT),
 *   or use EncryptedSharedPreferences. Until then the token is stored as plain
 *   bytes in the app's private storage, which other apps can't read on a
 *   non-rooted device, and a warning is logged.
 * - Other platforms: plain bytes, with the same warning.
 */
struct GFCAPI_API FGFCSecureStore
{
	static bool Protect(const FString& Secret, TArray<uint8>& OutBlob);
	static bool Unprotect(const TArray<uint8>& Blob, FString& OutSecret);

	/** True when Protect uses real OS protection rather than plain storage. */
	static bool IsHardwareOrOsBacked();
};
