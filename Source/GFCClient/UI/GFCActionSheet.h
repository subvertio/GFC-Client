#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "GFCActionSheet.generated.h"

class UButton;
class UGFCActionSheet;
class UVerticalBox;

/** One option's click handler (dynamic delegates can't carry the option's index). */
UCLASS()
class UGFCSheetChoice : public UObject
{
	GENERATED_BODY()
public:
	int32 Index = -1;
	TWeakObjectPtr<UGFCActionSheet> Sheet;
	UFUNCTION() void Clicked();
};

/**
 * A phone-style action sheet: a list of choices at the bottom of the screen over a dimmed
 * background. Tapping outside, Cancel, or the Android Back key closes it (choice -1).
 * One at a time; showing another replaces the open one.
 */
UCLASS()
class GFCCLIENT_API UGFCActionSheet : public UUserWidget
{
	GENERATED_BODY()

public:
	using FOnPick = TFunction<void(int32 Index)>;

	/** Shows the sheet over everything. Options are drawn in order; bDanger marks the last one red. */
	static UGFCActionSheet* Show(UObject* WorldContext, const FText& Title, const TArray<FText>& Options, FOnPick OnPick,
		bool bLastIsDanger = false);

	/** Closes the open sheet as if cancelled. Returns false when none is open (for the Back key). */
	static bool CloseOpen();

	void Finish(int32 Index);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION() void HandleCancel();
	void Build(const FText& Title, const TArray<FText>& Options, bool bLastIsDanger);

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> List;
	UPROPERTY(Transient) TArray<TObjectPtr<UGFCSheetChoice>> Choices;
	UPROPERTY(Transient) TObjectPtr<UButton> Dim;
	FOnPick OnPick;
	bool bDone = false;
};

/**
 * Report / block for a post, from the feed or the post screen: the "•••" menu.
 * OnHidden runs once the post should disappear (reported, or its creator blocked).
 */
GFCCLIENT_API void GFCShowPostActions(UObject* WorldContext, const FGFCSeed& Seed, TFunction<void()> OnHidden);
