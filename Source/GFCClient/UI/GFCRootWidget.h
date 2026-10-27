#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "GFCRootWidget.generated.h"

class UButton;
class UGFCFeedScreen;
class UGFCLoginScreen;
class UGFCScreenBase;
class UGFCSeedDetailScreen;
class UGFCWalletScreen;
class UNamedSlot;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;

/**
 * App shell: top bar, status line and a switcher holding one instance of each
 * screen. Screen classes are configurable so Blueprint screens can replace the
 * code-built ones without touching C++. See docs/ui-spec.md.
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

	UGFCRootWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowLogin();
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowFeed();
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowWallet();
	UFUNCTION(BlueprintCallable, Category = "GFC|Navigation") void ShowSeed(const FGFCSeed& Seed);

	/** One line under the top bar for progress and errors. */
	UFUNCTION(BlueprintCallable, Category = "GFC") void ShowStatus(const FText& Message, bool bError = false);

	/** After login or register. New accounts go to the wallet screen to create a first wallet. */
	void OnLoggedIn(bool bNewAccount);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> ScreenSwitcher;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> NavBar;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> FeedNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> WalletNavButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LogoutButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> UserText;

private:
	void BuildDefaultLayout();
	bool OpenStartScreenFromCommandLine();
	void Activate(UGFCScreenBase* Screen);
	void RefreshNav();

	UFUNCTION() void HandleLogout();
	UFUNCTION() void HandleSessionChanged();

	UPROPERTY(Transient) TObjectPtr<UGFCLoginScreen> LoginScreen;
	UPROPERTY(Transient) TObjectPtr<UGFCFeedScreen> FeedScreen;
	UPROPERTY(Transient) TObjectPtr<UGFCWalletScreen> WalletScreen;
	UPROPERTY(Transient) TObjectPtr<UGFCSeedDetailScreen> SeedDetailScreen;
};
