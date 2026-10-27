#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCApiTypes.h"
#include "GFCMediaCapture.h"
#include "GFCCreateScreen.generated.h"

class FGFCUploadJob;
class UButton;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;

/**
 * Post something, top to bottom: write (a post can be just text), optionally add
 * a photo, video, gallery item or file (3D model, audio), title it, and upload
 * with progress and cancel. A text-only post is uploaded as a small .txt seed.
 * Captures are saved to the phone's gallery and stay the user's; the app's
 * temporary copy is deleted after posting. See docs/ui-spec.md#create.
 */
UCLASS()
class GFCCLIENT_API UGFCCreateScreen : public UGFCScreenBase
{
	GENERATED_BODY()

public:
	virtual void OnScreenShown() override;

	/** Picks or captures media (what the source buttons do). */
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
	/** What the post says: the whole post for a text post, the caption otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UMultiLineEditableTextBox> BodyBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> FileButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> RemoveMediaButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CancelButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> UploadProgress;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ProgressText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UImage> PreviewImage;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FileText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ViewPostButton;
	/** The chosen media's preview and details; hidden for a text post. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> MediaPanel;

private:
	UFUNCTION() void HandlePhoto();
	UFUNCTION() void HandleVideo();
	UFUNCTION() void HandleGallery();
	UFUNCTION() void HandleFile();
	UFUNCTION() void HandleRemoveMedia();
	UFUNCTION() void HandleBodyChanged(const FText& NewText);
	UFUNCTION() void HandlePost();
	UFUNCTION() void HandleCancel();
	UFUNCTION() void HandleViewPost();

	void OnChosen(const FGFCCapturedMedia& Media);
	void SetUploading(bool bUploading);
	void DiscardMedia();
	void UpdatePostEnabled();
	void ShowMediaPanel();

public:
	/** Title for a text post with no title: its first line, at most 80 characters. */
	static FString TitleFromBody(const FString& Body);
	/** The text post's file: <cache>/outbox/text-<guid>.txt holding Body. Empty on failure. */
	static FString WriteTextSeed(const FString& Body);
	/** Longest text post (the node keeps descriptions as plain text; this keeps posts readable). */
	static constexpr int32 MaxBodyChars = 5000;
private:
	bool bTextFile = false; // Chosen is a text seed the app wrote

	FGFCCapturedMedia Chosen;
	FGFCSeed Posted;
	TSharedPtr<FGFCUploadJob> Job;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> PreviewTexture;
};
