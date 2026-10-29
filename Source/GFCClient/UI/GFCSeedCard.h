#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCApiTypes.h"
#include "GFCViewTracker.h"
#include "GFCSeedCard.generated.h"

class UButton;
class UGFCMediaSession;
class UImage;
class UScaleBox;
class USizeBox;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FGFCOnSeedSelected, const FGFCSeed&);

/**
 * One feed post: avatar, name, @handle · time, title, media, stats.
 *
 * Tapping the post opens the seed. Video and audio play **inline, inside the
 * card, only when tapped**: video starts muted (a Sound button turns it on),
 * audio plays because the user asked for it. Nothing auto-plays, nothing goes
 * full screen. Scrolling away releases the player and the downloaded file.
 */
UCLASS()
class GFCCLIENT_API UGFCSeedCard : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSeed(const FGFCSeed& InSeed);

	FGFCOnSeedSelected OnSelected;
	/** "•••": report or block (GFCShowPostActions). */
	FGFCOnSeedSelected OnMore;

	UFUNCTION(BlueprintPure, Category = "GFC")
	const FGFCSeed& GetSeed() const { return Seed; }

	/** Scrolled out of view: let go of the thumbnail, the inline player and anything the card owns in the cache. */
	void ReleaseContent();
	/** Back in view: show the thumbnail again (from memory if still cached, else re-fetched). */
	void RestoreContent();
	bool IsContentLoaded() const { return bContentLoaded; }

	/** The cache owner key for this card: "card:<seed id>". */
	FString OwnerKey() const { return TEXT("card:") + Seed.Id; }
	FString ThumbUrl() const;
public:
	/**
	 * The desktop layout: an action row (reply, like, views, GFC earned, bookmark, share)
	 * instead of the stats line, wider padding and taller media. Off by default (phone).
	 */
	void SetWideLayout(bool bInWide);

	/** The card's text: Description trimmed and cut at MaxChars with an ellipsis (tap for the rest). */
	static FString FeedBody(const FString& Description, int32 MaxChars);
private:

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> CardButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UImage> ThumbImage;
	/** The post's text: a text post's whole body, or a caption (shortened in the feed). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BodyText;
	/** The media frame around ThumbImage; collapsed for text posts. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> MediaFrame;
	/** Around ThumbImage: fills the frame for thumbnails, fits (letterboxed) while a video plays. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UScaleBox> ThumbScale;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> OwnerText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MetaText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> KindText;
	/** "@owner · 3h" */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HandleText;
	/** Holds the avatar (an initial in a circle until profile images are loaded). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<USizeBox> AvatarSlot;
	/** "Repost of @original": shown when the node matched this post to an earlier original. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RepostText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> MoreButton;
	/** Over the media: plays video/audio inline (or opens the seed for other kinds). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> MediaButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MediaLabel;
	/** Inline video: turns sound on/off. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> SoundButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SoundLabel;
	/** The phone's stats line (kind, views, likes, comments). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> StatsRow;
	/** Desktop: the action row and its parts. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> ActionRow;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ReplyButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ReplyText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LikeButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LikeText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ViewsText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EarnedText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> BookmarkButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BookmarkText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ShareButton;

private:
	void BuildDefaultLayout();
	void StopInline();
	void UpdateMediaControls();
	void HandleOwnerReleased(const FString& Owner);

	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleMediaClicked();
	UFUNCTION() void HandleSoundClicked();
	UFUNCTION() void HandleMoreClicked();
	UFUNCTION() void HandleLike();
	UFUNCTION() void HandleBookmark();
	UFUNCTION() void HandleShare();
	void UpdateActions();
	bool bWide = false;
	bool bTogglingLike = false;
	bool bTogglingBookmark = false;

	FGFCSeed Seed;
	bool bContentLoaded = false;
	bool bLoadingInline = false;
	float UiTimer = 0.f;
	FGFCViewTracker Tracker;
	FDelegateHandle ReleasedHandle;

	UPROPERTY(Transient) TObjectPtr<UGFCMediaSession> Inline;
};

/** Short label for a media kind ("3D model", "Image", ...). */
GFCCLIENT_API FText GFCMediaKindLabel(EGFCMediaKind Kind);

/** Requests Url from the image cache and shows it in Image when it arrives (if Image still exists). */
GFCCLIENT_API void GFCLoadImageInto(UUserWidget* Owner, UImage* Image, const FString& Url);
