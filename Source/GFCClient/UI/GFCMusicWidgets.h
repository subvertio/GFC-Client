#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "GFCMusicWidgets.generated.h"

class UButton;
class UGFCMusicPlayer;
class UImage;
class UProgressBar;
class USlider;
class UTextBlock;

/**
 * The full music player on an audio post's screen: title, seek bar with elapsed and total time,
 * -15 s, play/pause, +15 s and loop. It drives the shared UGFCMusicPlayer, so the music keeps
 * playing after the user leaves the post (the mini player takes over).
 */
UCLASS()
class GFCCLIENT_API UGFCMusicPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The audio post this panel plays. */
	void SetSeed(const FGFCSeed& InSeed);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TrackText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<USlider> SeekSlider;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ElapsedText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TotalText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> BackButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> PlayButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PlayLabel;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ForwardButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LoopButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LoopLabel;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION() void HandlePlay();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleForward();
	UFUNCTION() void HandleLoop();
	UFUNCTION() void HandleSeekStarted();
	UFUNCTION() void HandleSeekEnded();
	void Refresh();
	UGFCMusicPlayer* Music() const;

	FGFCSeed Seed;
	bool bDragging = false;
	FDelegateHandle ChangedHandle;
};

/**
 * The mini player above the bottom bar: what's playing, a thin progress line, play/pause and close.
 * Tapping the title opens the post. Hidden while nothing plays.
 */
UCLASS()
class GFCCLIENT_API UGFCMiniPlayer : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Opens the playing post (the root sets this). */
	TFunction<void(const FGFCSeed&)> OnOpen;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> Progress;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> OpenButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OwnerText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> PlayButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PlayLabel;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

private:
	UFUNCTION() void HandleOpen();
	UFUNCTION() void HandlePlay();
	UFUNCTION() void HandleClose();
	void Refresh();
	UGFCMusicPlayer* Music() const;

	FDelegateHandle ChangedHandle;
};
