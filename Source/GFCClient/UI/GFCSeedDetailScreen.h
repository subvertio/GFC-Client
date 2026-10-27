#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCApiTypes.h"
#include "GFCSeedDetailScreen.generated.h"

class UButton;
class UImage;
class UTextBlock;

/** One seed: preview, details and sharing. See docs/ui-spec.md#seed-detail. */
UCLASS()
class GFCCLIENT_API UGFCSeedDetailScreen : public UGFCScreenBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GFC")
	void SetSeed(const FGFCSeed& InSeed);

	virtual void OnScreenShown() override;

	/** The share page URL ({ShareBaseUrl}/p/{id}) for the current seed. */
	UFUNCTION(BlueprintPure, Category = "GFC")
	FString GetShareUrl() const;

protected:
	virtual void BuildDefaultLayout() override;
	virtual void BindEvents() override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> ShareButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CopyLinkButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> BackButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> OpenMediaButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UImage> PreviewImage;
	/** Shown over/under the preview, e.g. the 3D viewer placeholder. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PreviewNote;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OwnerText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MetaText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ShareLinkText;

private:
	UFUNCTION() void HandleShare();
	UFUNCTION() void HandleCopyLink();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleOpenMedia();

	void Render();

	FGFCSeed Seed;
};
