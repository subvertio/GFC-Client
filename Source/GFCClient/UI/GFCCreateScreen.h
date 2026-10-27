#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCApiTypes.h"
#include "GFCMediaCapture.h"
#include "GFCCreateScreen.generated.h"

class FGFCUploadJob;
class UButton;
class UEditableTextBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;

/**
 * Post something: take a photo, record a video, or choose from the gallery,
 * add a title and description, and upload with progress and cancel.
 * Captures are saved to the phone's gallery and stay the user's; the app's
 * temporary copy is deleted after posting. See docs/ui-spec.md#create.
 */
UCLASS()
class GFCCLIENT_API UGFCCreateScreen : public UGFCScreenBase
{
	GENERATED_BODY()

public:
	virtual void OnScreenShown() override;

	/** Picks or captures media (what the three source buttons do). */
	void Choose(EGFCCaptureSource Source);

	/** For tests and the PC -GFCPost=<file> shortcut: use a file already on disk. */
	void UseFile(const FString& Path);

protected:
	virtual void NativeDestruct() override;
	virtual void BuildDefaultLayout() override;
	virtual void BindEvents() override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> PhotoButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> VideoButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> GalleryButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> TitleBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> PostButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> DescriptionBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CancelButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> UploadProgress;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ProgressText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UImage> PreviewImage;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FileText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ViewPostButton;
	/** The form below the source buttons; hidden until something is chosen. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> FormPanel;

private:
	UFUNCTION() void HandlePhoto();
	UFUNCTION() void HandleVideo();
	UFUNCTION() void HandleGallery();
	UFUNCTION() void HandlePost();
	UFUNCTION() void HandleCancel();
	UFUNCTION() void HandleViewPost();

	void OnChosen(const FGFCCapturedMedia& Media);
	void SetUploading(bool bUploading);
	void DiscardMedia();

	FGFCCapturedMedia Chosen;
	FGFCSeed Posted;
	TSharedPtr<FGFCUploadJob> Job;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> PreviewTexture;
};
