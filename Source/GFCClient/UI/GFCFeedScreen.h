#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCApiTypes.h"
#include "GFCFeedScreen.generated.h"

class UButton;
class UGFCSeedCard;
class UPanelWidget;
class UTextBlock;

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

	virtual void OnScreenHidden() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void BuildDefaultLayout() override;
	virtual void BindEvents() override;

	/** Any panel (a ScrollBox in the default layout); cards are added as children. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UPanelWidget> SeedList;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> RefreshButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;

private:
	UFUNCTION() void HandleRefresh();
	void Populate(const TArray<FGFCSeed>& Seeds);

	/** Releases cards that have been off screen longer than the grace period; restores cards coming back. */
	void UpdateCardVisibility(float DeltaTime);

	bool bLoading = false;
	TMap<TWeakObjectPtr<UGFCSeedCard>, float> OffscreenSeconds;
};
