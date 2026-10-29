#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "GFCMediaCapture.h"
#include "GFCRootWidget.generated.h"

class UBorder;
class UButton;
class UGFCCreateScreen;
class UGFCLeftRail;
class UGFCRightRail;
class UHorizontalBox;
class UOverlay;
class USizeBox;
class UGFCFeedScreen;
class UGFCLoginScreen;
class UGFCScreenBase;
class UGFCSeedDetailScreen;
class UGFCWalletScreen;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;

/**
 * App shell. Phone-first (portrait): safe-area aware, a slim top bar, one status
 * line, the active screen, and a bottom navigation bar within thumb reach.
 * In a window at least GFCUI::Desktop::Breakpoint dp wide it switches, live, to the
 * desktop layout: a navigation rail on the left, the active screen in a 600 dp centre
 * column, and a right rail of node data (search, balance, trending, top earners).
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
	/** The For you feed (desktop Home; the phone's Feed button keeps the feed's current page). */
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowHome();
	/** Search results for Query, or the top posts when it's empty. */
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowExplore(const FString& Query);
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowBookmarks();
	/** The user's own posts. */
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowProfile();
	/** Another account's posts (e.g. a tapped top earner); your own name opens your profile. Shows at once, then fills in. */
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowUser(const FString& Username);
	UFUNCTION(BlueprintCallable, Category = "GFC") void Logout();

	/** From the desktop composer: opens the create screen with Body, then the picker or the upload. */
	void ComposeFromFeed(const FString& PostBody, TOptional<EGFCCaptureSource> Pick, bool bPostNow);

	/** True while the window shows the desktop (three-column) layout. */
	UFUNCTION(BlueprintPure, Category = "GFC") bool IsWideLayout() const { return Layout != ELayout::Phone; }

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
	/** The poster edited a post: its feed cards and the open post screen show the new text in place. */
	void OnPostEdited(const FGFCSeed& Updated);

	/** Opens the node's phone verification page in the browser; the balance is checked on return. */
	void StartPhoneVerification();
	/**
	 * Runs Then when the user may post. On the real network (GET /health phone_required_to_post) an
	 * unverified account is offered phone verification instead, before writing anything.
	 */
	void WhenAllowedToPost(TFunction<void()> Then);
	/** "Verify my phone" sheet with Why above it; opens the verification page. */
	void OfferPhoneVerification(const FText& Why);

	/**
	 * A recovery code the user hasn't confirmed saving yet (from registering, or a new one from the
	 * wallet screen). Kept in memory only: the wallet screen shows it until they tap "I've written it down".
	 */
	FString PendingRecoveryCode;

	/** Opens a node page (e.g. "/reset") in the system browser. False if no browser opened. */
	bool OpenNodePage(const FString& Path);

	/** Reloads the right rail (balance, top earners) when it's showing, e.g. after this device learns a wallet. */
	void RefreshRightRail();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> ScreenSwitcher;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> NavBar;
	/** What's playing, above the bottom bar (UGFCMusicPlayer). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<class UGFCMiniPlayer> MiniPlayer;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> FeedNavButton;
	/** The GFC logo in the top bar: back to the full feed. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> BrandButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> WalletNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CreateNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LogoutButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LoginNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> UserText;
	/** Desktop rails (collapsed in the phone layout). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UGFCLeftRail> LeftRail;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UGFCRightRail> RightRail;

private:
	enum class ELayout : uint8 { Phone, Wide, WideWithRightRail };
	ELayout Layout = ELayout::Phone;
	/** Applies a layout: which chrome shows, the centre column's width, the screens' wide mode. */
	void ApplyLayout(ELayout NewLayout);
	void UpdateDesktopNav();
	void ShowFeedPage(uint8 Source, const FString& Query);

	/** The layout parts that change between phone and desktop. */
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> Shell;
	UPROPERTY(Transient) TObjectPtr<USizeBox> CenterBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> Body;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> PhoneChrome;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> DesktopChrome;
	/** Desktop status messages: a pill at the bottom that fades after a few seconds. */
	UPROPERTY(Transient) TObjectPtr<UWidget> Toast;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ToastText;
	float ToastSeconds = 0.f;

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
