#include "UI/GFCSeedCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "GFCApi.h"
#include "GFCApiSettings.h"
#include "GFCCacheSubsystem.h"
#include "MediaTexture.h"
#include "Viewer/GFCMediaSession.h"
#include "Viewer/GFCMusicPlayer.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UI/GFCCreateScreen.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCSeedCard"

using namespace GFCUI;

FText GFCMediaKindLabel(EGFCMediaKind Kind)
{
	switch (Kind)
	{
	case EGFCMediaKind::Model3D: return LOCTEXT("Model", "3D model");
	case EGFCMediaKind::Image: return LOCTEXT("Image", "Image");
	case EGFCMediaKind::Video: return LOCTEXT("Video", "Video");
	case EGFCMediaKind::Audio: return LOCTEXT("Audio", "Audio");
	case EGFCMediaKind::Text: return LOCTEXT("Text", "Text");
	default: return LOCTEXT("Other", "File");
	}
}

void GFCLoadImageInto(UUserWidget* Owner, UImage* Image, const FString& Url)
{
	if (!Owner || !Image)
	{
		return;
	}
	// Which URL each image is currently meant to show, so a late download for a
	// previous seed doesn't overwrite a reused widget.
	static TMap<TWeakObjectPtr<UImage>, FString> Wanted;
	for (auto It = Wanted.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	Wanted.Add(Image, Url);

	Image->SetBrushResourceObject(nullptr);
	Image->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f)); // hidden until loaded
	UGFCCacheSubsystem* Cache = Owner->GetGameInstance() ? Owner->GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>() : nullptr;
	if (!Cache || Url.IsEmpty())
	{
		return;
	}
	TWeakObjectPtr<UImage> WeakImage(Image);
	Cache->RequestThumbnail(Url, [WeakImage, Url](UTexture2D* Tex)
	{
		const FString* Expected = Wanted.Find(WeakImage);
		if (WeakImage.IsValid() && Tex && Expected && *Expected == Url)
		{
			WeakImage->SetBrushFromTexture(Tex, true);
			WeakImage->SetColorAndOpacity(FLinearColor::White);
		}
	});
}

void UGFCSeedCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	if (CardButton) CardButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	if (MediaButton) MediaButton->OnClicked.AddDynamic(this, &ThisClass::HandleMediaClicked);
	if (SoundButton) SoundButton->OnClicked.AddDynamic(this, &ThisClass::HandleSoundClicked);
	if (MoreButton) MoreButton->OnClicked.AddDynamic(this, &ThisClass::HandleMoreClicked);
	if (ReplyButton) ReplyButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked); // comments live on the post's screen
	if (LikeButton) LikeButton->OnClicked.AddDynamic(this, &ThisClass::HandleLike);
	if (BookmarkButton) BookmarkButton->OnClicked.AddDynamic(this, &ThisClass::HandleBookmark);
	if (ShareButton) ShareButton->OnClicked.AddDynamic(this, &ThisClass::HandleShare);
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		// Another card (or the detail screen) became the live heavy item: stop playing here.
		ReleasedHandle = Cache->OnOwnerReleased.AddUObject(this, &UGFCSeedCard::HandleOwnerReleased);
	}
	Tracker.Rules = FGFCViewRules::FromSettings();
}

void UGFCSeedCard::NativeDestruct()
{
	StopInline();
	if (UGFCCacheSubsystem* Cache = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>() : nullptr)
	{
		Cache->OnOwnerReleased.Remove(ReleasedHandle);
	}
	Super::NativeDestruct();
}

void UGFCSeedCard::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;

	// A social-feed post: avatar | name @handle · time / repost / title / media / stats.
	CardButton = T->ConstructWidget<UButton>();
	CardButton->SetStyle(FlatButtonStyle());
	CardButton->SetTouchMethod(EButtonTouchMethod::PreciseTap); // swipes scroll the feed
	CardButton->SetClickMethod(EButtonClickMethod::PreciseClick);
	T->RootWidget = CardButton;

	UHorizontalBox* Post = MakeRow(T);
	CardButton->AddChild(Post);
	if (UButtonSlot* S = Cast<UButtonSlot>(Post->Slot))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(4.f, 12.f));
	}

	AvatarSlot = T->ConstructWidget<USizeBox>();
	AvatarSlot->SetWidthOverride(40.f);
	AddH(Post, AvatarSlot, 12.f);
	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(AvatarSlot->Slot))
	{
		S->SetVerticalAlignment(VAlign_Top);
	}

	UVerticalBox* Body = MakeColumn(T);
	AddH(Post, Body, 0.f, true);

	UHorizontalBox* Header = MakeRow(T);
	OwnerText = MakeText(T, FText::GetEmpty(), 15, GFCUI::Text, true);
	OwnerText->SetAutoWrapText(false);
	HandleText = MakeText(T, FText::GetEmpty(), 14, TextDim);
	HandleText->SetAutoWrapText(false);
	AddH(Header, OwnerText, 6.f);
	AddH(Header, HandleText, 0.f, true);
	// Report / block: a small target of its own, so it never opens the post by accident.
	MoreButton = T->ConstructWidget<UButton>();
	MoreButton->SetStyle(FlatButtonStyle());
	MoreButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
	UTextBlock* Dots = MakeText(T, FText::FromString(TEXT("\u2022\u2022\u2022")), 15, TextDim, true);
	Dots->SetAutoWrapText(false);
	MoreButton->AddChild(Dots);
	if (UButtonSlot* S = Cast<UButtonSlot>(Dots->Slot)) S->SetPadding(FMargin(10.f, 0.f, 4.f, 6.f));
	AddH(Header, MoreButton, 0.f);
	AddV(Body, Header, 2.f);

	RepostText = MakeText(T, FText::GetEmpty(), 13, TextDim);
	AddV(Body, RepostText, 2.f);

	TitleText = MakeText(T, FText::GetEmpty(), 15, GFCUI::Text);
	AddV(Body, TitleText, 6.f);
	BodyText = MakeText(T, FText::GetEmpty(), 15, GFCUI::Text);
	AddV(Body, BodyText, 10.f);

	// Media frame: the thumbnail (or inline video), a tap target over it, and a sound toggle.
	USizeBox* ThumbBox = T->ConstructWidget<USizeBox>();
	ThumbBox->SetHeightOverride(220.f);
	ThumbBox->SetClipping(EWidgetClipping::ClipToBounds);
	UBorder* ThumbBg = MakeCard(T, FMargin(0.f));
	UOverlay* Over = T->ConstructWidget<UOverlay>();
	UScaleBox* Scale = T->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFill);
	ThumbScale = Scale;
	ThumbImage = T->ConstructWidget<UImage>();
	Scale->AddChild(ThumbImage);

	MediaButton = T->ConstructWidget<UButton>();
	FButtonStyle Clear = FlatButtonStyle();
	Clear.Normal.TintColor = FSlateColor(FLinearColor::Transparent);
	Clear.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.04f));
	Clear.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f));
	MediaButton->SetStyle(Clear);
	MediaButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
	UBorder* LabelBg = MakePanel(T, FLinearColor(0.f, 0.f, 0.f, 0.7f), FMargin(12.f, 6.f));
	MediaLabel = MakeText(T, FText::GetEmpty(), 14, GFCUI::Text, true);
	MediaLabel->SetAutoWrapText(false);
	LabelBg->AddChild(MediaLabel);
	MediaButton->AddChild(LabelBg);
	if (UButtonSlot* S = Cast<UButtonSlot>(LabelBg->Slot))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* Sl = nullptr;
	SoundButton = MakeButton(T, LOCTEXT("SoundOn", "Sound on"), PanelRaised, &Sl);
	SoundLabel = Sl;

	for (UWidget* W : TArray<UWidget*>{Scale, MediaButton, SoundButton})
	{
		if (UOverlaySlot* S = Over->AddChildToOverlay(W))
		{
			const bool bSound = W == SoundButton;
			S->SetHorizontalAlignment(bSound ? HAlign_Right : HAlign_Fill);
			S->SetVerticalAlignment(bSound ? VAlign_Bottom : VAlign_Fill);
			S->SetPadding(bSound ? FMargin(8.f) : FMargin(0.f));
		}
	}
	ThumbBg->AddChild(Over);
	ThumbBox->AddChild(ThumbBg);
	AddV(Body, ThumbBox, 10.f);
	MediaFrame = ThumbBox;

	UHorizontalBox* Stats = MakeRow(T);
	KindText = MakeText(T, FText::GetEmpty(), 13, AccentText, true);
	KindText->SetAutoWrapText(false);
	MetaText = MakeText(T, FText::GetEmpty(), 13, TextDim);
	AddH(Stats, KindText, 14.f);
	AddH(Stats, MetaText, 0.f, true);
	AddV(Body, Stats, 0.f);
	StatsRow = Stats;

	// Desktop action row: each action is its own small target, so it never opens the post by accident.
	const FGFCTheme& Th = FGFCTheme::Get();
	UHorizontalBox* Actions = MakeRow(T);
	auto Action = [T, &Th](UTextBlock*& OutText, const FLinearColor& Hover) -> UButton*
	{
		UButton* B = T->ConstructWidget<UButton>();
		B->SetStyle(GhostButtonStyle(Hover, 17.f));
		B->SetClickMethod(EButtonClickMethod::PreciseClick);
		B->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		OutText = MakeText(T, FText::GetEmpty(), 13, Th.TextSecondary);
		OutText->SetAutoWrapText(false);
		B->AddChild(OutText);
		if (UButtonSlot* S = Cast<UButtonSlot>(OutText->Slot)) S->SetPadding(FMargin(8.f, 6.f));
		return B;
	};
	UTextBlock* Tx = nullptr;
	ReplyButton = Action(Tx, Th.AccentTint);
	ReplyText = Tx;
	LikeButton = Action(Tx, Th.Like.CopyWithNewOpacity(0.1f));
	LikeText = Tx;
	ViewsText = MakeText(T, FText::GetEmpty(), 13, Th.TextSecondary);
	ViewsText->SetAutoWrapText(false);
	EarnedText = MakeText(T, FText::GetEmpty(), 13, Th.Gold);
	EarnedText->SetAutoWrapText(false);
	BookmarkButton = Action(Tx, Th.AccentTint);
	BookmarkText = Tx;
	UTextBlock* ShareGlyph = nullptr;
	ShareButton = Action(ShareGlyph, Th.AccentTint);
	ShareGlyph->SetText(FText::FromString(Icon::Share));
	AddH(Actions, ReplyButton, 0.f, true);
	AddH(Actions, LikeButton, 0.f, true);
	AddH(Actions, ViewsText, 0.f, true);
	AddH(Actions, EarnedText, 0.f, true);
	AddH(Actions, BookmarkButton, 4.f);
	AddH(Actions, ShareButton, 0.f);
	for (UWidget* W : TArray<UWidget*>{ReplyButton, LikeButton, ViewsText, EarnedText})
	{
		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(W->Slot)) S->SetHorizontalAlignment(HAlign_Left);
	}
	ReplyButton->SetToolTipText(LOCTEXT("ReplyTip", "Comments"));
	LikeButton->SetToolTipText(LOCTEXT("LikeTip", "Like"));
	ViewsText->SetToolTipText(LOCTEXT("ViewsTip", "Views"));
	EarnedText->SetToolTipText(LOCTEXT("EarnedTip", "GFC this post has earned its creator"));
	BookmarkButton->SetToolTipText(LOCTEXT("BookmarkTip", "Bookmark"));
	ShareButton->SetToolTipText(LOCTEXT("ShareTip", "Copy link"));
	AddV(Body, Actions, 0.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Actions->Slot)) S->SetPadding(FMargin(-8.f, -2.f, 0.f, -6.f)); // icons line up with the text
	ActionRow = Actions;
	ActionRow->SetVisibility(ESlateVisibility::Collapsed);
}

void UGFCSeedCard::SetWideLayout(bool bInWide)
{
	bWide = bInWide;
	if (StatsRow) StatsRow->SetVisibility(bWide ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (ActionRow) ActionRow->SetVisibility(bWide ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (USizeBox* Frame = Cast<USizeBox>(MediaFrame)) Frame->SetHeightOverride(bWide ? 320.f : 220.f);
	if (CardButton && CardButton->GetChildrenCount() > 0)
	{
		if (UButtonSlot* S = Cast<UButtonSlot>(CardButton->GetChildAt(0)->Slot))
		{
			S->SetPadding(bWide ? FMargin(16.f, 12.f) : FMargin(4.f, 12.f));
		}
	}
	UpdateActions();
}

void UGFCSeedCard::UpdateActions()
{
	if (!ActionRow)
	{
		return;
	}
	const FGFCTheme& Th = FGFCTheme::Get();
	auto Count = [](int32 N) { return N > 0 ? FText::AsNumber(N) : FText::GetEmpty(); };
	if (ReplyText) ReplyText->SetText(FText::Format(LOCTEXT("ReplyAction", "{0}  {1}"), FText::FromString(Icon::Reply), Count(Seed.Comments)));
	if (LikeText)
	{
		LikeText->SetText(FText::Format(LOCTEXT("LikeAction", "{0}  {1}"), FText::FromString(Seed.bLiked ? Icon::Liked : Icon::Like), Count(Seed.Likes)));
		LikeText->SetColorAndOpacity(FSlateColor(Seed.bLiked ? Th.Like : Th.TextSecondary));
	}
	if (ViewsText) ViewsText->SetText(FText::Format(LOCTEXT("ViewsAction", "{0}  {1}"), FText::FromString(Icon::Views), Count(Seed.Views)));
	if (EarnedText)
	{
		FNumberFormattingOptions Two;
		Two.MinimumFractionalDigits = 2;
		Two.MaximumFractionalDigits = 2;
		EarnedText->SetText(FText::Format(LOCTEXT("EarnedAction", "{0}  {1} GFC"), FText::FromString(Icon::Wallet), FText::AsNumber(Seed.GfcEarned, &Two)));
		EarnedText->SetColorAndOpacity(FSlateColor(Seed.GfcEarned > 0.0 ? Th.Gold : Th.TextSecondary));
	}
	if (BookmarkText)
	{
		BookmarkText->SetText(FText::FromString(Seed.bBookmarked ? Icon::Bookmarked : Icon::Bookmark));
		BookmarkText->SetColorAndOpacity(FSlateColor(Seed.bBookmarked ? Th.Accent : Th.TextSecondary));
	}
}

void UGFCSeedCard::HandleLike()
{
	UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	if (!C->IsLoggedIn())
	{
		if (UGFCRootWidget* Root = UGFCRootWidget::Find(this)) Root->ShowStatus(LOCTEXT("LoginToLike", "Log in to like posts."), true);
		return;
	}
	if (bTogglingLike) return;
	bTogglingLike = true;
	// Shown at once; the node's answer has the real count.
	Seed.bLiked = !Seed.bLiked;
	Seed.Likes = FMath::Max(0, Seed.Likes + (Seed.bLiked ? 1 : -1));
	UpdateActions();
	TWeakObjectPtr<UGFCSeedCard> WeakThis(this);
	const FString Id = Seed.Id;
	C->ToggleLike(Id, [WeakThis, Id](const FGFCApiResult& R, const FGFCToggleResult& Toggle)
	{
		if (!WeakThis.IsValid() || WeakThis->Seed.Id != Id) return;
		WeakThis->bTogglingLike = false;
		FGFCSeed& S = WeakThis->Seed;
		if (R.bOk)
		{
			S.bLiked = Toggle.bOn;
			if (Toggle.Count >= 0) S.Likes = Toggle.Count;
		}
		else
		{
			S.bLiked = !S.bLiked;
			S.Likes = FMath::Max(0, S.Likes + (S.bLiked ? 1 : -1));
			if (UGFCRootWidget* Root = UGFCRootWidget::Find(WeakThis.Get())) Root->ShowStatus(FText::FromString(R.Error), true);
		}
		WeakThis->UpdateActions();
	});
}

void UGFCSeedCard::HandleBookmark()
{
	UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	if (!C->IsLoggedIn())
	{
		if (UGFCRootWidget* Root = UGFCRootWidget::Find(this)) Root->ShowStatus(LOCTEXT("LoginToBookmark", "Log in to bookmark posts."), true);
		return;
	}
	if (bTogglingBookmark) return;
	bTogglingBookmark = true;
	Seed.bBookmarked = !Seed.bBookmarked;
	UpdateActions();
	TWeakObjectPtr<UGFCSeedCard> WeakThis(this);
	const FString Id = Seed.Id;
	C->ToggleBookmark(Id, [WeakThis, Id](const FGFCApiResult& R, const FGFCToggleResult& Toggle)
	{
		if (!WeakThis.IsValid() || WeakThis->Seed.Id != Id) return;
		WeakThis->bTogglingBookmark = false;
		UGFCRootWidget* Root = UGFCRootWidget::Find(WeakThis.Get());
		if (R.bOk)
		{
			WeakThis->Seed.bBookmarked = Toggle.bOn;
			if (Root) Root->ShowStatus(Toggle.bOn ? LOCTEXT("Bookmarked", "Added to your Bookmarks.") : LOCTEXT("Unbookmarked", "Removed from your Bookmarks."));
		}
		else
		{
			WeakThis->Seed.bBookmarked = !WeakThis->Seed.bBookmarked;
			if (Root) Root->ShowStatus(FText::FromString(R.Error), true);
		}
		WeakThis->UpdateActions();
	});
}

void UGFCSeedCard::HandleShare()
{
	FPlatformApplicationMisc::ClipboardCopy(*UGFCApiClient::BuildShareUrl(Seed.Id));
	if (UGFCRootWidget* Root = UGFCRootWidget::Find(this))
	{
		Root->ShowStatus(LOCTEXT("LinkCopied", "Link copied to the clipboard."));
	}
}

void UGFCSeedCard::SetSeed(const FGFCSeed& InSeed)
{
	if (InSeed.Id != Seed.Id)
	{
		StopInline();
	}
	Seed = InSeed;
	const bool bText = Seed.GetMediaKind() == EGFCMediaKind::Text;
	// A text post titled from its own first line shows the text once, not twice.
	const bool bTitleRepeats = bText && UGFCCreateScreen::TitleFromBody(Seed.Description) == Seed.Title;
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Seed.Title));
		TitleText->SetVisibility(bTitleRepeats || Seed.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(FeedBody(Seed.Description, bText ? 600 : 200)));
		BodyText->SetVisibility(Seed.Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (MediaFrame) MediaFrame->SetVisibility(bText ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	const FString Owner = Seed.Owner.IsEmpty() ? FString(TEXT("unknown")) : Seed.Owner;
	if (OwnerText) OwnerText->SetText(FText::FromString(Owner));
	if (HandleText)
	{
		const FText When = RelativeTime(Seed.CreatedAt);
		HandleText->SetText(When.IsEmpty() ? FText::FromString(TEXT("@") + Owner)
			: FText::Format(LOCTEXT("HandleTime", "@{0} · {1}"), FText::FromString(Owner), When));
	}
	if (RepostText)
	{
		RepostText->SetText(Seed.RepostOf.IsSet()
			? FText::Format(LOCTEXT("RepostOf", "Repost of @{0}. Credit goes to the original."), FText::FromString(Seed.RepostOf.Owner))
			: FText::GetEmpty());
		RepostText->SetVisibility(Seed.RepostOf.IsSet() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (AvatarSlot)
	{
		AvatarSlot->ClearChildren();
		AvatarSlot->AddChild(MakeAvatar(WidgetTree, Owner, 40.f));
	}
	if (KindText) KindText->SetText(GFCMediaKindLabel(Seed.GetMediaKind()));
	if (MetaText)
	{
		MetaText->SetText(FText::Format(LOCTEXT("Meta", "{0} views    {1} likes    {2} comments"),
			FText::AsNumber(Seed.Views), FText::AsNumber(Seed.Likes), FText::AsNumber(Seed.Comments)));
	}
	UpdateActions();
	UpdateMediaControls();
	RestoreContent();
}

FString UGFCSeedCard::FeedBody(const FString& Description, int32 MaxChars)
{
	FString S = Description.TrimStartAndEnd();
	if (S.Len() > MaxChars)
	{
		S = S.Left(MaxChars - 1).TrimEnd() + TEXT("\u2026");
	}
	return S;
}

FString UGFCSeedCard::ThumbUrl() const
{
	const UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	return C->ResolveMediaUrl(Seed.Thumb);
}

void UGFCSeedCard::UpdateMediaControls()
{
	const EGFCMediaKind Kind = Seed.GetMediaKind();
	const bool bPlayable = Kind == EGFCMediaKind::Video || Kind == EGFCMediaKind::Audio;
	if (MediaLabel)
	{
		FText Label;
		if (bLoadingInline)
		{
			Label = LOCTEXT("Loading", "Loading…");
		}
		else if (Inline)
		{
			Label = Inline->IsPlaying() ? FText::GetEmpty() : LOCTEXT("Paused", "▶ Play");
		}
		else if (bPlayable)
		{
			const UGFCMusicPlayer* Music = GetGameInstance()->GetSubsystem<UGFCMusicPlayer>();
			Label = Kind == EGFCMediaKind::Video ? LOCTEXT("PlayVideo", "▶ Play video")
				: (Music && Music->IsCurrent(Seed.Id) ? LOCTEXT("NowPlaying", "Now playing") : LOCTEXT("PlayAudio", "▶ Play audio"));
		}
		else if (Kind == EGFCMediaKind::Model3D)
		{
			Label = LOCTEXT("View3D", "View in 3D");
		}
		MediaLabel->SetText(Label);
		if (UWidget* Bg = MediaLabel->GetParent())
		{
			Bg->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		}
	}
	if (SoundButton)
	{
		const bool bVideoPlaying = Inline && Kind == EGFCMediaKind::Video;
		SoundButton->SetVisibility(bVideoPlaying ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (SoundLabel && Inline)
		{
			SoundLabel->SetText(Inline->IsMuted() ? LOCTEXT("SoundOn", "Sound on") : LOCTEXT("SoundOff", "Mute"));
		}
	}
}

void UGFCSeedCard::HandleMediaClicked()
{
	const EGFCMediaKind Kind = Seed.GetMediaKind();
	if (Kind == EGFCMediaKind::Audio)
	{
		// Music plays in the shared player: it keeps going while the user scrolls and browses.
		if (UGFCMusicPlayer* Music = GetGameInstance()->GetSubsystem<UGFCMusicPlayer>())
		{
			if (Music->IsCurrent(Seed.Id)) Music->TogglePause();
			else Music->Play(Seed);
		}
		UpdateMediaControls();
		return;
	}
	if (Kind != EGFCMediaKind::Video)
	{
		HandleClicked(); // images and 3D open the seed
		return;
	}
	if (Inline)
	{
		Inline->TogglePause();
		UpdateMediaControls();
		return;
	}
	if (bLoadingInline)
	{
		return;
	}
	UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>();
	const UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	if (!Cache || !C || Seed.GetPlaybackPath().IsEmpty())
	{
		return;
	}
	bLoadingInline = true;
	UpdateMediaControls();
	TWeakObjectPtr<UGFCSeedCard> WeakThis(this);
	const FString Id = Seed.Id;
	// The card becomes the one live heavy item: whatever else was playing stops.
	Cache->FetchFile(C->ResolveMediaUrl(Seed.GetPlaybackPath()), OwnerKey(), int64(GetDefault<UGFCApiSettings>()->MaxDiskCacheMB) * 1024 * 1024 / 2, true,
		[WeakThis, Id, Kind](EGFCFetchResult Result, const FString& Path, int64)
		{
			if (!WeakThis.IsValid() || WeakThis->Seed.Id != Id || !WeakThis->bLoadingInline)
			{
				return;
			}
			WeakThis->bLoadingInline = false;
			if (Result != EGFCFetchResult::Ok)
			{
				if (WeakThis->MediaLabel) WeakThis->MediaLabel->SetText(LOCTEXT("Failed", "Couldn't load. Tap to retry."));
				return;
			}
			UGFCMediaSession* Session = NewObject<UGFCMediaSession>(WeakThis.Get());
			Session->SetMuted(Kind == EGFCMediaKind::Video); // video starts muted; audio was asked for
			if (!Session->Open(WeakThis->GetWorld(), Path))
			{
				Session->Release();
				if (WeakThis->MediaLabel) WeakThis->MediaLabel->SetText(LOCTEXT("CantPlay", "Can't play this here."));
				return;
			}
			WeakThis->Inline = Session;
			if (Kind == EGFCMediaKind::Video && WeakThis->ThumbImage && Session->GetVideoTexture())
			{
				WeakThis->ThumbImage->SetBrushResourceObject(Session->GetVideoTexture());
				WeakThis->ThumbImage->SetColorAndOpacity(FLinearColor::White);
				// Whole frame, letterboxed: filling would crop it, and zoom in further when the phone turns.
				if (WeakThis->ThumbScale) WeakThis->ThumbScale->SetStretch(EStretch::ScaleToFit);
			}
			const UGFCApiClient* Client = WeakThis->GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
			WeakThis->Tracker.Begin(Id, Kind, Client->IsLoggedIn() && WeakThis->Seed.Owner.Equals(Client->GetUsername(), ESearchCase::IgnoreCase));
			WeakThis->UpdateMediaControls();
		});
}

void UGFCSeedCard::HandleMoreClicked()
{
	OnMore.ExecuteIfBound(Seed);
}

void UGFCSeedCard::HandleSoundClicked()
{
	if (Inline)
	{
		Inline->SetMuted(!Inline->IsMuted());
		UpdateMediaControls();
	}
}

void UGFCSeedCard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Inline)
	{
		return;
	}
	Inline->FitBrush(ThumbImage);
	if (Tracker.NotifyPlayback(Inline->GetPositionSeconds(), Inline->GetDurationSeconds()))
	{
		UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
		if (C->IsLoggedIn())
		{
			C->RecordPlay(Seed.Id, [](const FGFCApiResult& R, const FGFCPlayResult& Play)
			{
				UE_LOG(LogGFCApi, Log, TEXT("Inline view counted: %s"), R.bOk ? (Play.bCredited ? TEXT("credited") : *Play.Reason) : *R.Error);
			});
		}
	}
	UiTimer += InDeltaTime;
	if (UiTimer > 0.5f)
	{
		UiTimer = 0.f;
		UpdateMediaControls();
	}
}

void UGFCSeedCard::StopInline()
{
	bLoadingInline = false;
	Tracker.End();
	if (Inline)
	{
		Inline->Release();
		Inline = nullptr;
	}
	if (ThumbScale)
	{
		ThumbScale->SetStretch(EStretch::ScaleToFill); // thumbnails fill the frame
	}
}

void UGFCSeedCard::HandleOwnerReleased(const FString& Owner)
{
	if (Owner == OwnerKey() && (Inline || bLoadingInline))
	{
		StopInline();
		if (bContentLoaded && ThumbImage)
		{
			bContentLoaded = false;
			RestoreContent(); // back to the thumbnail
		}
		UpdateMediaControls();
	}
}

void UGFCSeedCard::ReleaseContent()
{
	StopInline();
	if (!bContentLoaded)
	{
		return;
	}
	bContentLoaded = false;
	if (ThumbImage)
	{
		ThumbImage->SetBrushResourceObject(nullptr);
		ThumbImage->SetColorAndOpacity(FLinearColor::Transparent);
	}
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		Cache->ReleaseOwner(OwnerKey()); // cancels the download and deletes the file
	}
	UpdateMediaControls();
}

void UGFCSeedCard::RestoreContent()
{
	if (bContentLoaded)
	{
		return;
	}
	bContentLoaded = true;
	if (ThumbImage)
	{
		GFCLoadImageInto(this, ThumbImage, ThumbUrl());
	}
}

void UGFCSeedCard::HandleClicked()
{
	StopInline();
	OnSelected.ExecuteIfBound(Seed);
}

#undef LOCTEXT_NAMESPACE
