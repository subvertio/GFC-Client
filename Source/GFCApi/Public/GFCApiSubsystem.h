#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GFCSessionStore.h"
#include "GFCApiSubsystem.generated.h"

class UGFCApiClient;
class UGFCSessionSave;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGFCOnSessionChangedDynamic);

/**
 * Owns the app's API client and persists the session between launches.
 * Blueprint: Get Game Instance > Get Subsystem (GFC Api Subsystem) > Get Client.
 */
UCLASS()
class GFCAPI_API UGFCApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "GFC")
	UGFCApiClient* GetClient() const { return Client; }

	/** Points the client at another node and remembers it. Logs out if the node changes. */
	UFUNCTION(BlueprintCallable, Category = "GFC|Config")
	bool SetNode(const FString& BaseUrl);

	/** Wallets this device knows for the logged-in user on the current node. */
	UFUNCTION(BlueprintPure, Category = "GFC|Wallet")
	TArray<FGFCKnownWallet> GetKnownWallets() const;

	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet")
	void RememberWallet(const FString& WalletId, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet")
	void ForgetWallet(const FString& WalletId);

	/** Marks every known wallet of the user as backed up (an export covers all of them). */
	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet")
	void MarkWalletsBackedUp();

	/** True when the user has a wallet that isn't in a backup yet: show the backup prompt. */
	UFUNCTION(BlueprintPure, Category = "GFC|Wallet")
	bool NeedsBackup() const;

	/**
	 * Writes an export from /wallets/export to Saved/Backups (PC). Returns the file path, or empty on failure.
	 * TODO(android): hand the file to the share sheet / Storage Access Framework so it leaves the device.
	 */
	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet")
	FString SaveBackupFile(const FString& BackupJson) const;

	UPROPERTY(BlueprintAssignable, Category = "GFC|Session")
	FGFCOnSessionChangedDynamic OnSessionChanged;

	static const TCHAR* SlotName;

private:
	void HandleClientSessionChanged();
	void Persist();

	UPROPERTY()
	TObjectPtr<UGFCApiClient> Client;

	UPROPERTY()
	TObjectPtr<UGFCSessionSave> Save;

	bool bRestoring = false;
};
