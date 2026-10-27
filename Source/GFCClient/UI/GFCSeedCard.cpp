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
#include "UI/GFCCreateScreen.h"
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
			Label = Kind == EGFCMediaKind::Video ? LOCTEXT("PlayVideo", "▶ Play video") : LOCTEXT("PlayAudio", "▶ Play audio");
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
	if (Kind != EGFCMediaKind::Video && Kind != EGFCMediaKind::Audio)
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
	if (!Cache || !C || Seed.Full.IsEmpty())
	{
		return;
	}
	bLoadingInline = true;
	UpdateMediaControls();
	TWeakObjectPtr<UGFCSeedCard> WeakThis(this);
	const FString Id = Seed.Id;
	// The card becomes the one live heavy item: whatever else was playing stops.
	Cache->FetchFile(C->ResolveMediaUrl(Seed.Full), OwnerKey(), int64(GetDefault<UGFCApiSettings>()->MaxDiskCacheMB) * 1024 * 1024 / 2, true,
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
