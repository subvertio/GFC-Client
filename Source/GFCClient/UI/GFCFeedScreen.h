#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCApiTypes.h"
#include "Types/SlateEnums.h"
#include "GFCFeedScreen.generated.h"

class UButton;
class UEditableTextBox;
class UGFCSeedCard;
class UMultiLineEditableTextBox;
class UPanelWidget;
class USizeBox;
class UTextBlock;

/** What the feed lists. The phone layout only shows ForYou; the desktop rail reaches the rest. */
UENUM(BlueprintType)
enum class EGFCFeedSource : uint8
{
	ForYou,     // GET /feed, newest first
	Explore,    // GET /search?q=..., or the most liked posts when there's no query
	Bookmarks,  // GET /bookmarks
	Mine,       // GET /seeds/mine (the profile)
};

/** Seed list, newest first. See docs/ui-spec.md#feed. */
UCLASS()
class GFCCLIENT_API UGFCFeedScreen : public UGFCScreenBase
{
	GENERATED_BODY()

public:
	virtual void OnScreenShown() override;

	UFUNCTION(BlueprintCallable, Category = "GFC")
	void Refresh();

	/** Card class for each seed; a Blueprint subclass of UGFCSeedCard can replace it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GFC")
	TSubclassOf<UGFCSeedCard> SeedCardClass;

	UGFCFeedScreen(const FObjectInitializer& ObjectInitializer);

	/** Switches what the feed lists (refreshes when the screen is shown). Query is for Explore. */
	void SetSource(EGFCFeedSource InSource, const FString& InQuery = FString());
	EGFCFeedSource GetSource() const { return Source; }

	/** Desktop: a sticky header (tab or title), the inline composer on For you, and wide cards. */
	void SetWideLayout(bool bInWide);

	/** Called with every For you load (the desktop right rail's trending card reads it). */
	TFunction<void(const TArray<FGFCSeed>&)> OnForYouLoaded;

	virtual void OnScreenHidden() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void BuildDefaultLayout() override;
	virtual void BindEvents() override;

	/** Any panel (a ScrollBox in the default layout); cards are added as children. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UPanelWidget> SeedList;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> RefreshButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;
	/** The phone's header (title and Refresh); hidden on desktop. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> PhoneHeader;
	/** Desktop: the header above the list, with the For you tab or the page title. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> DesktopHeader;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> TabBar;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ForYouTab;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> TitleBar;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PageTitle;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PageSubtitle;
	/** Explore: the search box at the top of the page. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> SearchBox;
	/** Desktop For you: "What's happening?" with media/3D buttons and Post. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Composer;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UMultiLineEditableTextBox> ComposerBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ComposerMediaButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ComposerFileButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ComposerPostButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> ComposerAvatar;

private:
	UFUNCTION() void HandleRefresh();
	UFUNCTION() void HandleComposerChanged(const FText& NewText);
	UFUNCTION() void HandleComposerMedia();
	UFUNCTION() void HandleComposerFile();
	UFUNCTION() void HandleComposerPost();
	UFUNCTION() void HandleSearchCommitted(const FText& InText, ETextCommit::Type Commit);
	void BuildDesktopHeader(class UVerticalBox* Col);
	void Compose(TOptional<uint8> Pick, bool bPostNow);
	void UpdateHeader();
	void OnLoaded(const FGFCApiResult& R, const TArray<FGFCSeed>& Seeds, EGFCFeedSource For);
	void Populate(const TArray<FGFCSeed>& Seeds);

public:
	/** Drops posts the user just reported or whose creator they blocked, without reloading. */
	void RemoveHidden();
	/** Shows an edited post's new title, text and tags on its cards, without reloading. */
	void ApplyEdit(const FGFCSeed& Updated);
private:

	/** Releases cards that have been off screen longer than the grace period; restores cards coming back. */
	void UpdateCardVisibility(float DeltaTime);

	bool bLoading = false;
	bool bWide = false;
	EGFCFeedSource Source = EGFCFeedSource::ForYou;
	FString Query;
	/** Bumped on every source change, so a late answer for the previous page is dropped. */
	uint32 LoadSerial = 0;
	TMap<TWeakObjectPtr<UGFCSeedCard>, float> OffscreenSeconds;
};
