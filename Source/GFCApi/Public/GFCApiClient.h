#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GFCApiTypes.h"
#include "GFCApiClient.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FGFCOnResult, const FGFCApiResult&, Result);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnAuth, const FGFCApiResult&, Result, const FGFCAuthResult&, Auth);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnProfile, const FGFCApiResult&, Result, const FGFCProfile&, Profile);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnWallet, const FGFCApiResult&, Result, const FGFCWallet&, Wallet);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnTransaction, const FGFCApiResult&, Result, const FGFCTransaction&, Transaction);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnHistory, const FGFCApiResult&, Result, const TArray<FGFCTransaction>&, Transactions);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnFeed, const FGFCApiResult&, Result, const TArray<FGFCSeed>&, Seeds);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnSeed, const FGFCApiResult&, Result, const FGFCSeed&, Seed);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnShareInfo, const FGFCApiResult&, Result, const FGFCShareInfo&, Share);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGFCOnBackup, const FGFCApiResult&, Result, const FString&, BackupJson);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGFCOnUnauthorizedDynamic);
DECLARE_MULTICAST_DELEGATE(FGFCOnSessionChanged);

/**
 * HTTP client for a GFC node's REST API.
 *
 * - Auth is sent only as "Authorization: Bearer <token>", never in URLs or bodies.
 * - Wallet private keys never reach the client: the node signs transfers itself.
 * - The token is never logged. Requests are logged as "VERB path -> status" only.
 *
 * Every call has a C++ form (TFunction callback) and a Blueprint form (dynamic
 * delegate, "Create Event" in the graph). Callbacks run on the game thread.
 */
UCLASS(BlueprintType)
class GFCAPI_API UGFCApiClient : public UObject
{
	GENERATED_BODY()

public:
	template <typename T>
	using TCallback = TFunction<void(const FGFCApiResult&, const T&)>;

	UGFCApiClient();

	// ---- configuration and session ----

	UFUNCTION(BlueprintCallable, Category = "GFC|Config")
	void SetBaseUrl(const FString& InBaseUrl);

	UFUNCTION(BlueprintPure, Category = "GFC|Config")
	FString GetBaseUrl() const { return BaseUrl; }

	UFUNCTION(BlueprintPure, Category = "GFC|Session")
	bool IsLoggedIn() const { return !Token.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "GFC|Session")
	FString GetUsername() const { return Username; }

	/** Installs a session (e.g. restored from storage). Fires OnSessionChanged. */
	void SetSession(const FString& InUsername, const FString& InToken);

	/** Forgets the session locally. Fires OnSessionChanged. */
	UFUNCTION(BlueprintCallable, Category = "GFC|Session")
	void ClearSession();

	/** For the session store only. Never display or log the result. */
	const FString& GetTokenForStorage() const { return Token; }

	float TimeoutSeconds = 30.f;

	/** Fired after login, register, restore, logout or a 401. */
	FGFCOnSessionChanged OnSessionChanged;

	/** Fired when an authenticated call gets 401: the token is no longer valid. */
	UPROPERTY(BlueprintAssignable, Category = "GFC|Session")
	FGFCOnUnauthorizedDynamic OnUnauthorized;

	// ---- C++ API ----

	void Register(const FString& InUsername, const FString& Password, const FString& Email, TCallback<FGFCAuthResult> Done);
	void Login(const FString& InUsername, const FString& Password, TCallback<FGFCAuthResult> Done);
	/** Revokes the current token on the node (DELETE /auth/tokens/{id}), then clears the session. */
	void Logout(TFunction<void(const FGFCApiResult&)> Done);
	void GetMe(TCallback<FGFCProfile> Done);

	void CreateWallet(const FString& Name, TCallback<FGFCWallet> Done);
	void GetWallet(const FString& WalletId, TCallback<FGFCWallet> Done);
	void GetHistory(const FString& WalletId, TCallback<TArray<FGFCTransaction>> Done);
	/** The node signs the transfer with the sender wallet's key. Sender must be the caller's wallet. */
	void CreateTransfer(const FString& SenderWalletId, const FString& RecipientWalletId, int64 AmountCents,
		const FString& Reason, TCallback<FGFCTransaction> Done);
	/**
	 * Passphrase-encrypted backup of the user's wallet keys (needs the password,
	 * and a passphrase of 12+ characters). The JSON is sensitive: save it to a file,
	 * never log it.
	 */
	void ExportWallets(const FString& Password, const FString& BackupPassphrase, TCallback<FString> Done);

	void GetFeed(TCallback<TArray<FGFCSeed>> Done);
	void GetSeed(const FString& SeedId, TCallback<FGFCSeed> Done);
	void GetShareInfo(const FString& SeedId, TCallback<FGFCShareInfo> Done);

	/** Downloads public media (thumbnails). Never sends the token. Bodies over MaxBytes fail. */
	void DownloadBytes(const FString& Url, int64 MaxBytes, TFunction<void(bool bOk, const TArray<uint8>& Bytes)> Done);

	// ---- Blueprint API ----

	UFUNCTION(BlueprintCallable, Category = "GFC|Auth", meta = (DisplayName = "Register"))
	void K2_Register(const FString& InUsername, const FString& Password, const FString& Email, const FGFCOnAuth& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Auth", meta = (DisplayName = "Login"))
	void K2_Login(const FString& InUsername, const FString& Password, const FGFCOnAuth& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Auth", meta = (DisplayName = "Logout"))
	void K2_Logout(const FGFCOnResult& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Auth", meta = (DisplayName = "Get My Profile"))
	void K2_GetMe(const FGFCOnProfile& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet", meta = (DisplayName = "Create Wallet"))
	void K2_CreateWallet(const FString& Name, const FGFCOnWallet& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet", meta = (DisplayName = "Get Wallet"))
	void K2_GetWallet(const FString& WalletId, const FGFCOnWallet& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet", meta = (DisplayName = "Get Transaction History"))
	void K2_GetHistory(const FString& WalletId, const FGFCOnHistory& OnComplete);

	/** Amount is typed GFC text such as "2.50"; invalid amounts fail without calling the node. */
	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet", meta = (DisplayName = "Send GFC"))
	void K2_CreateTransfer(const FString& SenderWalletId, const FString& RecipientWalletId, const FString& Amount,
		const FString& Reason, const FGFCOnTransaction& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Wallet", meta = (DisplayName = "Export Wallet Backup"))
	void K2_ExportWallets(const FString& Password, const FString& BackupPassphrase, const FGFCOnBackup& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Feed", meta = (DisplayName = "Get Feed"))
	void K2_GetFeed(const FGFCOnFeed& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Feed", meta = (DisplayName = "Get Seed"))
	void K2_GetSeed(const FString& SeedId, const FGFCOnSeed& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "GFC|Share", meta = (DisplayName = "Get Share Info"))
	void K2_GetShareInfo(const FString& SeedId, const FGFCOnShareInfo& OnComplete);

	/** {ShareBaseUrl}/p/{id}, built locally. */
	UFUNCTION(BlueprintPure, Category = "GFC|Share")
	static FString BuildShareUrl(const FString& SeedId);

	/** https://x.com/intent/post?text=...&url=..., built locally. */
	UFUNCTION(BlueprintPure, Category = "GFC|Share")
	static FString BuildXIntentUrl(const FString& Text, const FString& Url);

	/** Full URL for a node media path such as a seed's Thumb or Full. */
	UFUNCTION(BlueprintPure, Category = "GFC|Feed")
	FString ResolveMediaUrl(const FString& PathOrUrl) const;

private:
	/** Raw request. Status 0 means the node couldn't be reached. */
	void Send(const FString& Verb, const FString& PathAndQuery, const FString& ContentType, const FString& Body,
		bool bAuth, TFunction<void(int32 Status, const FString& Body)> OnDone);

	/** How a 401 is read: an expired session, or (for endpoints that re-check the password) a wrong password. */
	enum class EUnauthorized : uint8 { SessionExpired, WrongPassword };

	template <typename T>
	void Call(const FString& Verb, const FString& PathAndQuery, const FString& ContentType, const FString& Body,
		bool bAuth, TFunction<bool(const FString&, T&)> Parse, TCallback<T> Done,
		EUnauthorized On401 = EUnauthorized::SessionExpired);

	void HandleAuthResult(const FGFCApiResult& Result, const FGFCAuthResult& Auth);

	FString BaseUrl;
	FString Username;
	FString Token;
};
