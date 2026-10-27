#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCApiTypes.h"
#include "GFCViewTracker.h"
#include "GFCSeedDetailScreen.generated.h"

class AGFCModelViewer;
class UButton;
class UGFCMediaSession;
class UGFCModelView;
class UImage;
class UTextBlock;
class UWidget;
struct FGFCModelData;

UENUM()
enum class EGFCDetailState : uint8
{
	Thumbnail,
	ConfirmCellular,
	Downloading,
	Preparing,
	Ready,
	Failed,
};

/**
 * One seed: thumbnail first, then on tap the full content in the app: a 3D
 * model (runtime .glb/.gltf with orbit, pinch-zoom and pan), a full-size image,
 * or video/audio playback. See docs/ui-spec.md#seed-detail.
 *
 * Content lifecycle (docs/cache-policy.md): the seed is the one live heavy item;
 * leaving the screen releases it (actor, textures, player, temp file).
 * Qualified views are reported once with POST /seeds/{id}/played.
 */
UCLASS()
class GFCCLIENT_API UGFCSeedDetailScreen : public UGFCScreenBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GFC")
	void SetSeed(const FGFCSeed& InSeed);

	UFUNCTION(BlueprintPure, Category = "GFC")
	const FGFCSeed& GetSeed() const { return Seed; }

	virtual void OnScreenShown() override;
	virtual void OnScreenHidden() override;

	/** The share page URL ({ShareBaseUrl}/p/{id}) for the current seed. */
	UFUNCTION(BlueprintPure, Category = "GFC")
	FString GetShareUrl() const;

	/** Starts loading the full content (what the primary button does). */
	UFUNCTION(BlueprintCallable, Category = "GFC")
	void LoadFullContent();

	/** Frees the model, image, player and temp file. */
	UFUNCTION(BlueprintCallable, Category = "GFC")
	void ReleaseContent();

	EGFCDetailState GetState() const { return State; }

	/**
	 * Future opt-in: keep a bookmarked seed's file for offline viewing (tied to
	 * POST /seeds/{id}/bookmark). Not built: everything is session-only for now.
	 */
	bool ShouldKeepOffline() const { return false; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void BuildDefaultLayout() override;
	virtual void BindEvents() override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> ShareButton;
	/** "View in 3D" / "View full image" / "Play": loads the full content. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> PrimaryButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PrimaryLabel;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CopyLinkButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> BackButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> OpenMediaButton;
	/** Thumbnail, then the full image or video. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UImage> PreviewImage;
	/** "•••": report the post or block its creator. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> MoreButton;
	/** The preview area and its button; collapsed for text posts, which are all text. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> PreviewFrame;
	/** The 3D view; created in code if a Blueprint doesn't provide one. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UGFCModelView> ModelView;
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
	UFUNCTION() void HandlePrimary();
	UFUNCTION() void HandleMore();

	void Render();
	void SetState(EGFCDetailState NewState, const FText& Note = FText::GetEmpty());
	void StartDownload();
	void OnDownloaded(const FString& Path);
	void ShowModel(TSharedPtr<FGFCModelData> Model);
	void OnQualifiedView();
	void HandleOwnerReleased(const FString& Owner);
	FString OwnerKey() const { return TEXT("seed:") + Seed.Id; }
	int64 MaxBytesForKind() const;
	void LogResourceUsage(const TCHAR* When) const;

	FGFCSeed Seed;
	EGFCDetailState State = EGFCDetailState::Thumbnail;
	FGFCViewTracker Tracker;
	uint32 LoadSerial = 0;
	double LoadStarted = 0.0;
	float ReadySeconds = 0.f;
	bool bReleasing = false;
	FDelegateHandle ReleasedHandle, BackgroundHandle, ForegroundHandle;
	bool bAppInBackground = false;

	UPROPERTY(Transient) TObjectPtr<AGFCModelViewer> Viewer;
	UPROPERTY(Transient) TObjectPtr<UGFCMediaSession> Media;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> FullImage;
};
