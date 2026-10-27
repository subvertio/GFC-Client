#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "GFCSeedCard.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FGFCOnSeedSelected, const FGFCSeed&);

/** One feed row: thumbnail, title, creator, stats. Click opens the seed. */
UCLASS()
class GFCCLIENT_API UGFCSeedCard : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSeed(const FGFCSeed& InSeed);

	FGFCOnSeedSelected OnSelected;

	UFUNCTION(BlueprintPure, Category = "GFC")
	const FGFCSeed& GetSeed() const { return Seed; }

	/** Scrolled out of view: let go of the thumbnail and anything the card owns in the cache. */
	void ReleaseContent();
	/** Back in view: show the thumbnail again (from memory if still cached, else re-fetched). */
	void RestoreContent();
	bool IsContentLoaded() const { return bContentLoaded; }

	/** The cache owner key for this card: "card:<seed id>". */
	FString OwnerKey() const { return TEXT("card:") + Seed.Id; }
	FString ThumbUrl() const;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> CardButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UImage> ThumbImage;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> OwnerText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MetaText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> KindText;
	/** "@owner · 3h" */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HandleText;
	/** Holds the avatar (an initial in a circle until profile images are loaded). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<class USizeBox> AvatarSlot;

private:
	void BuildDefaultLayout();

	UFUNCTION() void HandleClicked();

	FGFCSeed Seed;
	bool bContentLoaded = false;
};

/** Short label for a media kind ("3D model", "Image", ...). */
GFCCLIENT_API FText GFCMediaKindLabel(EGFCMediaKind Kind);

/** Requests Url from the image cache and shows it in Image when it arrives (if Image still exists). */
GFCCLIENT_API void GFCLoadImageInto(UUserWidget* Owner, UImage* Image, const FString& Url);
