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

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> CardButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UImage> ThumbImage;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> OwnerText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MetaText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> KindText;

private:
	void BuildDefaultLayout();

	UFUNCTION() void HandleClicked();

	FGFCSeed Seed;
};

/** Short label for a media kind ("3D model", "Image", ...). */
GFCCLIENT_API FText GFCMediaKindLabel(EGFCMediaKind Kind);

/** Requests Url from the image cache and shows it in Image when it arrives (if Image still exists). */
GFCCLIENT_API void GFCLoadImageInto(UUserWidget* Owner, UImage* Image, const FString& Url);
