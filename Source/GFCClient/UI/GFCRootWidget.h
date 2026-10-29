#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "GFCRootWidget.generated.h"

class UButton;
class UGFCCreateScreen;
class UGFCFeedScreen;
class UGFCLoginScreen;
class UGFCScreenBase;
class UGFCSeedDetailScreen;
class UGFCWalletScreen;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;

/**
 * App shell (portrait, phone-first): safe-area aware, a slim top bar, one status
 * line, the active screen, and a bottom navigation bar within thumb reach.
 * Screen classes are configurable so Blueprint screens can replace the
 * code-built ones without touching C++. See docs/ui-spec.md.
 *
 * Also routes deep links (FGFCDeepLink) to the seed detail screen, logged in or not.
 */
UCLASS()
class GFCCLIENT_API UGFCRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GFC|Screens") TSubclassOf<UGFCLoginScreen> LoginScreenClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GFC|Screens") TSubclassOf<UGFCFeedScreen> FeedScreenClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GFC|Screens") TSubclassOf<UGFCWalletScreen> WalletScreenClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GFC|Screens") TSubclassOf<UGFCSeedDetailScreen> SeedDetailScreenClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GFC|Screens") TSubclassOf<UGFCCreateScreen> CreateScreenClass;

	UGFCRootWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowLogin();
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowFeed();
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowWallet();
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowCreate();
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowSeed(const FGFCSeed& Seed);

	/** Opens a seed by id (deep links). Works without logging in: seeds are public. */
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void OpenSeedById(const FString& SeedId);

	/** Back from a seed: the feed when logged in, otherwise login. */
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void GoBack();

	/** One line under the top bar for progress and errors. */
	UFUNCTION(BlueprintCallable, Category = "GFC") void ShowStatus(const FText& Message, bool bError = false);

	/** After login or register. New accounts go to the wallet screen to create a first wallet. */
	void OnLoggedIn(bool bNewAccount);

	/** If the user's wallets went up since they last looked, plays the coin burst with the amount. */
	void AnnounceEarnings();

	/** The app's root (one per game instance). */
	static UGFCRootWidget* Find(const UObject* WorldContext);

	/** A post was reported or its creator blocked: off the feed, and off screen if it's open. */
	void OnPostHidden(const FString& SeedId);

	/** Opens the node's phone verification page in the browser; the balance is checked on return. */
	void StartPhoneVerification();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> ScreenSwitcher;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> NavBar;
	/** What's playing, above the bottom bar (UGFCMusicPlayer). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<class UGFCMiniPlayer> MiniPlayer;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> FeedNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> WalletNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CreateNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LogoutButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LoginNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> UserText;

private:
	void BuildDefaultLayout();
	bool OpenStartScreenFromCommandLine();
	void Activate(UGFCScreenBase* Screen);
	void RefreshNav();
	void HandleDeepLink(const FString& SeedId);

	UFUNCTION() void HandleLogout();
	UFUNCTION() void HandleSessionChanged();

	UPROPERTY(Transient) TObjectPtr<UGFCLoginScreen> LoginScreen;
	UPROPERTY(Transient) TObjectPtr<UGFCFeedScreen> FeedScreen;
	UPROPERTY(Transient) TObjectPtr<UGFCWalletScreen> WalletScreen;
	UPROPERTY(Transient) TObjectPtr<UGFCSeedDetailScreen> SeedDetailScreen;
	UPROPERTY(Transient) TObjectPtr<UGFCCreateScreen> CreateScreen;
	UPROPERTY(Transient) TObjectPtr<UGFCScreenBase> ActiveScreen;

	FDelegateHandle DeepLinkHandle;
	FDelegateHandle VerifiedLinkHandle;
	FDelegateHandle ForegroundHandle;
	bool bVerifyingPhone = false;
	void OnBackFromVerification();
	TSharedPtr<class FGFCBackKey> BackKey;

public:
	/** Android Back: one screen back (feed, or login when logged out). Returns false on the feed/login, where Back does nothing. */
	bool HandleBack();
private:
	bool bReturnToSeed = false;
};
