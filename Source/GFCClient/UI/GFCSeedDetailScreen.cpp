#include "UI/GFCSeedDetailScreen.h"

#include "Async/Async.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GFCApi.h"
#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCCacheSubsystem.h"
#include "GFCModelParser.h"
#include "GFCUrl.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "MediaTexture.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Components/Spacer.h"
#include "UI/GFCActionSheet.h"
#include "UI/GFCMusicWidgets.h"
#include "UI/GFCPostEditor.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCSeedCard.h"
#include "UI/GFCUIStyle.h"
#include "Viewer/GFCMediaSession.h"
#include "Viewer/GFCModelView.h"
#include "Viewer/GFCModelViewer.h"

#define LOCTEXT_NAMESPACE "GFCSeedDetail"

using namespace GFCUI;

namespace
{
	FText MegaBytes(int64 Bytes)
	{
		FNumberFormattingOptions O;
		O.MaximumFractionalDigits = 1;
		return FText::AsNumber(Bytes / 1048576.0, &O);
	}

	/** "2.5 MB", or "180 KB" for small files. */
	FText FileSize(int64 Bytes)
	{
		if (Bytes < 1024 * 1024)
		{
			return FText::Format(NSLOCTEXT("GFCSeedDetail", "KB", "{0} KB"), FText::AsNumber(FMath::Max<int64>(1, Bytes / 1024)));
		}
		return FText::Format(NSLOCTEXT("GFCSeedDetail", "MB", "{0} MB"), MegaBytes(Bytes));
	}
}

void UGFCSeedDetailScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		ReleasedHandle = Cache->OnOwnerReleased.AddUObject(this, &UGFCSeedDetailScreen::HandleOwnerReleased);
	}
	Tracker.Rules = FGFCViewRules::FromSettings();
	// On phones the app is either in the foreground or not; a desktop window can be on screen without focus.
	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddWeakLambda(this, [this]() { bAppInBackground = true; });
	ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddWeakLambda(this, [this]() { bAppInBackground = false; });
}

void UGFCSeedDetailScreen::NativeDestruct()
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundHandle);
	ReleaseContent();
	if (UGFCCacheSubsystem* Cache = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>() : nullptr)
	{
		Cache->OnOwnerReleased.Remove(ReleasedHandle);
	}
	Super::NativeDestruct();
}

void UGFCSeedDetailScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;
	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	T->RootWidget = Scroll;
	UVerticalBox* Col = MakeColumn(T);
	Scroll->AddChild(Col);

	BackButton = MakeButton(T, LOCTEXT("Back", "← Back"), PanelRaised);
	UHorizontalBox* BackRow = MakeRow(T);
	AddH(BackRow, BackButton, 0.f);
	AddH(BackRow, T->ConstructWidget<USpacer>(), 0.f, true);
	MoreButton = MakeButton(T, FText::FromString(TEXT("\u2022\u2022\u2022")), PanelRaised);
	AddH(BackRow, MoreButton, 0.f);
	AddV(Col, BackRow, 10.f);

	// Preview: thumbnail, then the model, image or video. Full width, square-ish.
	USizeBox* PreviewBox = T->ConstructWidget<USizeBox>();
	PreviewBox->SetHeightOverride(340.f);
	PreviewBox->SetClipping(EWidgetClipping::ClipToBounds);
	UBorder* PreviewBg = MakePanel(T, FGFCTheme::Get().Surface, FMargin(0.f));
	UOverlay* Over = T->ConstructWidget<UOverlay>();
	UScaleBox* Fit = T->ConstructWidget<UScaleBox>();
	Fit->SetStretch(EStretch::ScaleToFit);
	PreviewImage = T->ConstructWidget<UImage>();
	Fit->AddChild(PreviewImage);
	ModelView = CreateWidget<UGFCModelView>(this, UGFCModelView::StaticClass());
	PreviewNote = MakeText(T, FText::GetEmpty(), 14, Text, true);
	PreviewNote->SetJustification(ETextJustify::Center);
	UBorder* NoteBg = MakePanel(T, FLinearColor(0.f, 0.f, 0.f, 0.7f), FMargin(10.f));
	NoteBg->AddChild(PreviewNote);
	for (UWidget* W : TArray<UWidget*>{Fit, ModelView, NoteBg})
	{
		if (UOverlaySlot* S = Over->AddChildToOverlay(W))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(W == NoteBg ? VAlign_Bottom : VAlign_Fill);
		}
	}
	PreviewBg->AddChild(Over);
	PreviewBox->AddChild(PreviewBg);
	AddV(Col, PreviewBox, 10.f);
	PreviewFrame = PreviewBox;

	UTextBlock* Label = nullptr;
	PrimaryButton = MakeButton(T, LOCTEXT("View", "View"), Accent, &Label);
	PrimaryLabel = Label;
	AddV(Col, PrimaryButton, 16.f);
	MusicPanel = CreateWidget<UGFCMusicPanel>(this, UGFCMusicPanel::StaticClass());
	MusicPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddV(Col, MusicPanel, 16.f);

	TitleText = MakeText(T, FText::GetEmpty(), 24, Text, true);
	OwnerText = MakeText(T, FText::GetEmpty(), 14, AccentText);
	MetaText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	DescriptionText = MakeText(T, FText::GetEmpty(), 15, Text);
	AddV(Col, TitleText, 4.f);
	ExclusiveBadge = MakeExclusiveBadge(T);
	AddV(Col, ExclusiveBadge, 6.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(ExclusiveBadge->Slot)) S->SetHorizontalAlignment(HAlign_Left);
	UHorizontalBox* OwnerRow = MakeRow(T);
	AddH(OwnerRow, OwnerText, 4.f);
	EditedButton = MakeTextLink(T, LOCTEXT("Edited", "· Edited"), 14);
	AddH(OwnerRow, EditedButton, 0.f);
	AddV(Col, OwnerRow, 8.f);
	AddV(Col, MetaText, 12.f);
	AddV(Col, DescriptionText, 12.f);
	UTextBlock* UnlockLabel = nullptr;
	UnlockButton = MakeExclusiveLink(T, &UnlockLabel);
	UnlockText = UnlockLabel;
	AddV(Col, UnlockButton, 12.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(UnlockButton->Slot)) S->SetHorizontalAlignment(HAlign_Left);

	UHorizontalBox* Share = MakeRow(T);
	ShareButton = MakeButton(T, LOCTEXT("ShareX", "Share on X"), PanelRaised);
	CopyLinkButton = MakeButton(T, LOCTEXT("Copy", "Copy link"), PanelRaised);
	AddH(Share, ShareButton, 8.f, true);
	AddH(Share, CopyLinkButton, 0.f, true);
	AddV(Col, Share, 6.f);
	ShareLinkText = MakeText(T, FText::GetEmpty(), 11, TextDim);
	AddV(Col, ShareLinkText, 12.f);
	OpenMediaButton = MakeButton(T, LOCTEXT("Open", "Open the original file"), PanelRaised);
	AddV(Col, OpenMediaButton, 0.f);
}

void UGFCSeedDetailScreen::BindEvents()
{
	if (ShareButton) ShareButton->OnClicked.AddDynamic(this, &ThisClass::HandleShare);
	if (CopyLinkButton) CopyLinkButton->OnClicked.AddDynamic(this, &ThisClass::HandleCopyLink);
	if (BackButton) BackButton->OnClicked.AddDynamic(this, &ThisClass::HandleBack);
	if (OpenMediaButton) OpenMediaButton->OnClicked.AddDynamic(this, &ThisClass::HandleOpenMedia);
	if (PrimaryButton) PrimaryButton->OnClicked.AddDynamic(this, &ThisClass::HandlePrimary);
	if (MoreButton) MoreButton->OnClicked.AddDynamic(this, &ThisClass::HandleMore);
	if (EditedButton) EditedButton->OnClicked.AddDynamic(this, &ThisClass::HandleEdited);
	if (UnlockButton) UnlockButton->OnClicked.AddDynamic(this, &ThisClass::HandleUnlock);
	if (ModelView)
	{
		TWeakObjectPtr<UGFCSeedDetailScreen> WeakThis(this);
		ModelView->OnInteract.BindLambda([WeakThis]()
		{
			if (WeakThis.IsValid() && WeakThis->Tracker.NotifyInteraction())
			{
				WeakThis->OnQualifiedView();
			}
		});
	}
}

void UGFCSeedDetailScreen::SetSeed(const FGFCSeed& InSeed)
{
	if (InSeed.Id != Seed.Id)
	{
		ReleaseContent();
		PlayLinks.Reset();
		bFetchingLinks = false;
		bLoadAfterLinks = false;
	}
	Seed = InSeed;
	SetState(EGFCDetailState::Thumbnail);
	Render();
}

void UGFCSeedDetailScreen::OnScreenShown()
{
	const UGFCApiClient* C = Client();
	Tracker.Begin(Seed.Id, Seed.GetMediaKind(), C && C->IsLoggedIn() && Seed.Owner.Equals(C->GetUsername(), ESearchCase::IgnoreCase));
	// Refresh counts from the node; the feed entry may be stale.
	if (UGFCApiClient* Api = Client(); Api && !Seed.Id.IsEmpty())
	{
		TWeakObjectPtr<UGFCSeedDetailScreen> WeakThis(this);
		const FString Id = Seed.Id;
		Api->GetSeed(Id, [WeakThis, Id](const FGFCApiResult& R, const FGFCSeed& Fresh)
		{
			if (WeakThis.IsValid() && R.bOk && WeakThis->Seed.Id == Id)
			{
				const bool bLiked = WeakThis->Seed.bLiked, bBookmarked = WeakThis->Seed.bBookmarked;
				WeakThis->Seed = Fresh;
				WeakThis->Seed.bLiked = bLiked; // /seeds/{id} has no per-user state
				WeakThis->Seed.bBookmarked = bBookmarked;
				WeakThis->Render();
				WeakThis->FetchPlayLinks(); // it may have become exclusive since the feed loaded
			}
		});
	}
	FetchPlayLinks();
	Super::OnScreenShown();

	// Development checks (docs/cache-policy.md): -GFCAutoView opens the full content right away.
	if (FParse::Param(FCommandLine::Get(), TEXT("GFCAutoView")) && State == EGFCDetailState::Thumbnail)
	{
		LoadFullContent();
	}
}

void UGFCSeedDetailScreen::LogResourceUsage(const TCHAR* When) const
{
	const UGFCCacheSubsystem* Cache = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>() : nullptr;
	const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
	UE_LOG(LogGFCApi, Display, TEXT("GFC lifecycle [%s]: process memory %.1f MB | cache %s"), When,
		Mem.UsedPhysical / 1048576.0, Cache ? *Cache->GetStats().ToString() : TEXT("n/a"));
}

void UGFCSeedDetailScreen::OnScreenHidden()
{
	// Scrolled away from this card: everything it loaded goes.
	bLoadAfterLinks = false;
	ReleaseContent();
	Tracker.End();
	SetState(EGFCDetailState::Thumbnail);
}

FString UGFCSeedDetailScreen::GetShareUrl() const
{
	return UGFCApiClient::BuildShareUrl(Seed.Id);
}

void UGFCSeedDetailScreen::FetchPlayLinks()
{
	UGFCApiClient* C = Client();
	if (!C || !Seed.bExclusive || PlayLinks.IsSet() || bFetchingLinks)
	{
		return;
	}
	const EGFCExclusiveAccess Access = C->GetExclusiveAccess(Seed);
	if (Access != EGFCExclusiveAccess::Full && Access != EGFCExclusiveAccess::Unknown)
	{
		return; // logged out, or already refused: the teaser (or title) is all there is
	}
	bFetchingLinks = true;
	TWeakObjectPtr<UGFCSeedDetailScreen> WeakThis(this);
	const FString Id = Seed.Id;
	C->GetPlayLinks(Id, [WeakThis, Id](const FGFCApiResult& R, const FGFCPlayLinks& Links)
	{
		if (!WeakThis.IsValid() || WeakThis->Seed.Id != Id)
		{
			return;
		}
		WeakThis->bFetchingLinks = false;
		if (R.bOk)
		{
			WeakThis->PlayLinks = Links;
		}
		else if (R.StatusCode != 403 && R.StatusCode != 401)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
		}
		WeakThis->Render(); // the body of a text post, or the lock line
		if (WeakThis->bLoadAfterLinks)
		{
			WeakThis->bLoadAfterLinks = false;
			WeakThis->LoadFullContent(); // the full clip, or the teaser if the node said no
		}
	});
}

FString UGFCSeedDetailScreen::PlaybackPath() const
{
	if (!Seed.bExclusive)
	{
		return Seed.GetPlaybackPath();
	}
	return PlayLinks.IsSet() && !PlayLinks->GetPlaybackPath().IsEmpty() ? PlayLinks->GetPlaybackPath() : Seed.Teaser;
}

void UGFCSeedDetailScreen::HandleUnlock()
{
	if (const UGFCApiClient* C = Client())
	{
		GFCUnlockExclusive(this, C->GetExclusiveAccess(Seed));
	}
}

int64 UGFCSeedDetailScreen::MaxBytesForKind() const
{
	const UGFCApiSettings* S = GetDefault<UGFCApiSettings>();
	switch (Seed.GetMediaKind())
	{
	case EGFCMediaKind::Model3D: return int64(S->MaxModelMB) * 1024 * 1024;
	case EGFCMediaKind::Image: return int64(S->MaxImageMB) * 1024 * 1024;
	default: return int64(S->MaxDiskCacheMB) * 1024 * 1024 / 2; // video/audio: half the cache at most
	}
}

void UGFCSeedDetailScreen::Render()
{
	const EGFCMediaKind Kind = Seed.GetMediaKind();
	if (TitleText) TitleText->SetText(FText::FromString(Seed.Title));
	if (OwnerText)
	{
		const FText When = RelativeTime(Seed.CreatedAt);
		OwnerText->SetText(When.IsEmpty() ? FText::Format(LOCTEXT("By", "by {0}"), FText::FromString(Seed.Owner))
			: FText::Format(LOCTEXT("ByWhen", "by {0} · {1}"), FText::FromString(Seed.Owner), When));
	}
	if (EditedButton) EditedButton->SetVisibility(Seed.EditedAt.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	const UGFCApiClient* Api = Client();
	const EGFCExclusiveAccess Access = Api ? Api->GetExclusiveAccess(Seed) : EGFCExclusiveAccess::NeedsLogin;
	if (DescriptionText)
	{
		// An exclusive text post's body comes from /play; until then (or when locked) only the title is shown.
		const bool bExclusiveText = Seed.bExclusive && Kind == EGFCMediaKind::Text;
		const FString Body = bExclusiveText && PlayLinks.IsSet() ? PlayLinks->Text : Seed.Description;
		FText Shown = FText::FromString(Body);
		if (Body.IsEmpty())
		{
			Shown = !Seed.bExclusive ? LOCTEXT("NoDesc", "No description.")
				: bExclusiveText && bFetchingLinks ? LOCTEXT("LoadingPost", "Loading the post…") : FText::GetEmpty();
		}
		DescriptionText->SetText(Shown);
		DescriptionText->SetVisibility(Shown.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		FSlateFontInfo Font = DescriptionText->GetFont();
		Font.Size = Kind == EGFCMediaKind::Text ? 18 : 15; // a text post reads bigger
		DescriptionText->SetFont(Font);
	}
	if (ExclusiveBadge) ExclusiveBadge->SetVisibility(Seed.bExclusive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UnlockButton)
	{
		// Only what unlocks it: the post's screen already shows what they may see.
		const bool bLocked = Seed.bExclusive && (Access == EGFCExclusiveAccess::NeedsLogin || Access == EGFCExclusiveAccess::NeedsVerify);
		if (UnlockText) UnlockText->SetText(bLocked ? GFCExclusiveUnlockText(Access, Kind) : FText::GetEmpty());
		UnlockButton->SetVisibility(bLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	const ESlateVisibility MediaVis = Kind == EGFCMediaKind::Text ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	if (PreviewFrame) PreviewFrame->SetVisibility(MediaVis);
	const bool bAudio = Kind == EGFCMediaKind::Audio;
	if (PrimaryButton) PrimaryButton->SetVisibility(bAudio ? ESlateVisibility::Collapsed : MediaVis);
	if (MusicPanel)
	{
		MusicPanel->SetVisibility(bAudio ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bAudio) MusicPanel->SetSeed(Seed);
	}
	if (MetaText)
	{
		const FString Tags = Seed.Tags.Num() ? FString::Join(Seed.Tags, TEXT(", ")) : FString(TEXT("none"));
		FText Size = Seed.FileSize > 0 ? FileSize(Seed.FileSize) : FText::GetEmpty();
		MetaText->SetText(FText::Format(
			LOCTEXT("Meta", "{0} · {1} · {2} views · {3} likes · earned {4} GFC\nTags: {5}"),
			GFCMediaKindLabel(Kind), Size, FText::AsNumber(Seed.Views), FText::AsNumber(Seed.Likes),
			FText::AsNumber(Seed.GfcEarned), FText::FromString(Tags)));
	}
	if (ShareLinkText) ShareLinkText->SetText(FText::FromString(GetShareUrl()));
	if (OpenMediaButton) OpenMediaButton->SetVisibility(Kind == EGFCMediaKind::Other ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (State == EGFCDetailState::Thumbnail && PreviewImage)
	{
		if (const UGFCApiClient* C = Client())
		{
			GFCLoadImageInto(this, PreviewImage, C->ResolveMediaUrl(Seed.Thumb));
		}
	}
	SetState(State);
}

void UGFCSeedDetailScreen::SetState(EGFCDetailState NewState, const FText& Note)
{
	State = NewState;
	const EGFCMediaKind Kind = Seed.GetMediaKind();
	FText Label;
	FText NoteText = Note;
	bool bButtonEnabled = true;
	switch (State)
	{
	case EGFCDetailState::Thumbnail:
		switch (Kind)
		{
		case EGFCMediaKind::Model3D: Label = LOCTEXT("View3D", "View in 3D"); break;
		case EGFCMediaKind::Image: Label = LOCTEXT("ViewImage", "View full image"); break;
		case EGFCMediaKind::Video:
			Label = Seed.bExclusive && PlaybackPath() == Seed.Teaser ? LOCTEXT("PlayTeaser", "Play the 5-second teaser") : LOCTEXT("PlayVideo", "Play video");
			break;
		case EGFCMediaKind::Audio: Label = LOCTEXT("PlayAudio", "Play audio"); break;
		default: Label = LOCTEXT("NoPreview", "No in-app preview"); bButtonEnabled = false; break;
		}
		if (NoteText.IsEmpty() && Seed.Thumb.IsEmpty())
		{
			NoteText = LOCTEXT("NoThumb", "No thumbnail");
		}
		break;
	case EGFCDetailState::ConfirmCellular:
		Label = LOCTEXT("ConfirmCell", "Download on mobile data");
		break;
	case EGFCDetailState::Downloading:
		Label = LOCTEXT("Downloading", "Downloading…");
		bButtonEnabled = false;
		break;
	case EGFCDetailState::Preparing:
		Label = Kind == EGFCMediaKind::Model3D ? LOCTEXT("Building", "Building the 3D view…") : LOCTEXT("Opening", "Opening…");
		bButtonEnabled = false;
		break;
	case EGFCDetailState::Ready:
		Label = Kind == EGFCMediaKind::Video ? (Media && Media->IsMuted() ? LOCTEXT("SoundOnD", "Sound on") : LOCTEXT("MuteD", "Mute"))
			: Kind == EGFCMediaKind::Audio ? LOCTEXT("PlayPause", "Play / pause")
			: Kind == EGFCMediaKind::Image ? LOCTEXT("FullShown", "Full image") : LOCTEXT("Reset", "Reset view");
		bButtonEnabled = Kind != EGFCMediaKind::Image;
		break;
	case EGFCDetailState::Failed:
		Label = LOCTEXT("Retry", "Try again");
		break;
	}
	if (PrimaryLabel) PrimaryLabel->SetText(Label);
	if (PrimaryButton) PrimaryButton->SetIsEnabled(bButtonEnabled);
	if (PreviewNote)
	{
		PreviewNote->SetText(NoteText);
		if (UWidget* Parent = PreviewNote->GetParent())
		{
			Parent->SetVisibility(NoteText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
	const bool bModelLive = State == EGFCDetailState::Ready && Viewer;
	if (ModelView) ModelView->SetVisibility(bModelLive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (PreviewImage && PreviewImage->GetParent())
	{
		PreviewImage->GetParent()->SetVisibility(bModelLive ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UGFCSeedDetailScreen::HandleMore()
{
	TWeakObjectPtr<UGFCSeedDetailScreen> WeakThis(this);
	const FString Id = Seed.Id;
	GFCShowPostActions(this, Seed, [WeakThis, Id]()
	{
		if (WeakThis.IsValid() && WeakThis->Root.IsValid()) WeakThis->Root->OnPostHidden(Id);
	});
}

void UGFCSeedDetailScreen::HandleEdited()
{
	UGFCPostPanel::ShowHistory(this, Seed);
}

void UGFCSeedDetailScreen::ApplyEdit(const FGFCSeed& Updated)
{
	Seed.Title = Updated.Title;
	Seed.Description = Updated.Description;
	Seed.Tags = Updated.Tags;
	Seed.EditedAt = Updated.EditedAt;
	if (PlayLinks.IsSet() && Seed.bExclusive && Seed.GetMediaKind() == EGFCMediaKind::Text)
	{
		PlayLinks->Text = Updated.Description; // the poster's own new text
	}
	Render();
}

void UGFCSeedDetailScreen::HandlePrimary()
{
	switch (State)
	{
	case EGFCDetailState::Ready:
		if (Viewer) Viewer->ResetView();
		if (Media && Seed.GetMediaKind() == EGFCMediaKind::Video)
		{
			Media->SetMuted(!Media->IsMuted()); // video: the button is the sound switch
			SetState(State);
		}
		else if (Media)
		{
			Media->TogglePause();
		}
		return;
	case EGFCDetailState::ConfirmCellular:
		StartDownload();
		return;
	default:
		LoadFullContent();
		return;
	}
}

void UGFCSeedDetailScreen::LoadFullContent()
{
	UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>();
	const UGFCApiClient* C = Client();
	if (!Cache || !C || PlaybackPath().IsEmpty())
	{
		if (Seed.bExclusive) SetState(EGFCDetailState::Failed, LOCTEXT("NoTeaser", "The teaser isn't ready yet. Try again in a minute."));
		return;
	}
	if (Seed.bExclusive && bFetchingLinks)
	{
		bLoadAfterLinks = true; // the full clip's links are on their way: play as soon as they're here
		SetState(EGFCDetailState::Downloading);
		return;
	}
	// FileSize is the original's; a streaming copy (or an exclusive's teaser) is much smaller (the download's own cap still applies).
	const bool bStreamCopy = Seed.bExclusive ? PlaybackPath() != (PlayLinks.IsSet() ? PlayLinks->Full : FString()) : !Seed.Stream.IsEmpty();
	const int64 Cap = MaxBytesForKind();
	if (!bStreamCopy && Seed.FileSize > Cap)
	{
		SetState(EGFCDetailState::Failed, FText::Format(LOCTEXT("TooBigKnown", "This file is {0} MB; the limit on phones is {1} MB."),
			MegaBytes(Seed.FileSize), MegaBytes(Cap)));
		return;
	}
	// Ask before big downloads on mobile data.
	const int64 AskAbove = int64(GetDefault<UGFCApiSettings>()->CellularConfirmMB) * 1024 * 1024;
	if (UGFCCacheSubsystem::IsOnCellular() && Seed.FileSize > AskAbove)
	{
		SetState(EGFCDetailState::ConfirmCellular, bStreamCopy
			? LOCTEXT("AskCellStream", "You're on mobile data. Play it anyway?")
			: FText::Format(LOCTEXT("AskCell", "You're on mobile data. Download {0} MB?"), MegaBytes(Seed.FileSize)));
		return;
	}
	StartDownload();
}

void UGFCSeedDetailScreen::StartDownload()
{
	UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>();
	const FString MediaPath = PlaybackPath();
	const FString Url = Client()->ResolveMediaUrl(MediaPath);
	const uint32 Serial = ++LoadSerial;
	LoadStarted = FPlatformTime::Seconds();
	SetState(EGFCDetailState::Downloading, Seed.FileSize > 0 && Seed.Stream.IsEmpty() && MediaPath == Seed.Full && !Seed.bExclusive
		? FText::Format(LOCTEXT("DlSize", "Downloading {0}…"), FileSize(Seed.FileSize))
		: LOCTEXT("Dl", "Downloading…"));

	TWeakObjectPtr<UGFCSeedDetailScreen> WeakThis(this);
	Cache->FetchFile(Url, OwnerKey(), MaxBytesForKind(), true, [WeakThis, Serial](EGFCFetchResult Result, const FString& Path, int64)
	{
		if (!WeakThis.IsValid() || WeakThis->LoadSerial != Serial)
		{
			return; // released or replaced meanwhile
		}
		switch (Result)
		{
		case EGFCFetchResult::Ok: WeakThis->OnDownloaded(Path); return;
		case EGFCFetchResult::Cancelled: return;
		case EGFCFetchResult::TooBig: WeakThis->SetState(EGFCDetailState::Failed, LOCTEXT("TooBig", "This file is too big to open on a phone.")); return;
		case EGFCFetchResult::LowDiskSpace: WeakThis->SetState(EGFCDetailState::Failed, LOCTEXT("LowDisk", "Your device is low on storage, so the file wasn't downloaded. Free some space and try again.")); return;
		case EGFCFetchResult::CacheFull: WeakThis->SetState(EGFCDetailState::Failed, LOCTEXT("CacheFull", "Couldn't store the file. Try again.")); return;
		default: WeakThis->SetState(EGFCDetailState::Failed, LOCTEXT("NetFail", "Download failed. Check your connection and try again.")); return;
		}
	});
}

void UGFCSeedDetailScreen::OnDownloaded(const FString& Path)
{
	const EGFCMediaKind Kind = Seed.GetMediaKind();
	const uint32 Serial = LoadSerial;
	TWeakObjectPtr<UGFCSeedDetailScreen> WeakThis(this);
	SetState(EGFCDetailState::Preparing);

	if (Kind == EGFCMediaKind::Model3D)
	{
		// Parse off the game thread; budgets are checked before anything big is built.
		const FGFCModelBudget Budget = FGFCModelBudget::FromSettings();
		Async(EAsyncExecution::ThreadPool, [WeakThis, Serial, Path, Budget]()
		{
			TArray<uint8> Bytes;
			TSharedPtr<FGFCModelData> Model = MakeShared<FGFCModelData>();
			FString Error;
			const bool bOk = FFileHelper::LoadFileToArray(Bytes, *Path) && FGFCModelParser::Parse(Bytes, Budget, *Model, Error);
			if (!bOk && Error.IsEmpty())
			{
				Error = TEXT("The downloaded file couldn't be read.");
			}
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Serial, Model, bOk, Error]()
			{
				if (!WeakThis.IsValid() || WeakThis->LoadSerial != Serial)
				{
					return;
				}
				if (!bOk)
				{
					WeakThis->SetState(EGFCDetailState::Failed, FText::FromString(Error));
					return;
				}
				WeakThis->ShowModel(Model);
			});
		});
		return;
	}

	if (Kind == EGFCMediaKind::Image)
	{
		TArray<uint8> Bytes;
		FullImage = FFileHelper::LoadFileToArray(Bytes, *Path)
			? UGFCCacheSubsystem::DecodeTexture(Bytes, GetDefault<UGFCApiSettings>()->MaxTextureSize) : nullptr;
		if (!FullImage)
		{
			SetState(EGFCDetailState::Failed, LOCTEXT("BadImage", "This image couldn't be opened."));
			return;
		}
		if (PreviewImage)
		{
			PreviewImage->SetBrushFromTexture(FullImage, true);
			PreviewImage->SetColorAndOpacity(FLinearColor::White);
		}
		SetState(EGFCDetailState::Ready);
		return;
	}

	// Video or audio
	Media = NewObject<UGFCMediaSession>(this);
	Media->SetMuted(Kind == EGFCMediaKind::Video); // never start with sound; the user turns it on
	if (!Media->Open(GetWorld(), Path))
	{
		Media->Release();
		Media = nullptr;
		SetState(EGFCDetailState::Failed, LOCTEXT("BadMedia", "This file can't be played on this device."));
		return;
	}
	if (Kind == EGFCMediaKind::Video && PreviewImage && Media->GetVideoTexture())
	{
		PreviewImage->SetBrushResourceObject(Media->GetVideoTexture());
		PreviewImage->SetColorAndOpacity(FLinearColor::White);
	}
	SetState(EGFCDetailState::Ready, Kind == EGFCMediaKind::Audio ? LOCTEXT("PlayingAudio", "Playing") : FText::GetEmpty());
}

void UGFCSeedDetailScreen::ShowModel(TSharedPtr<FGFCModelData> Model)
{
	UWorld* World = GetWorld();
	if (!World || !Model.IsValid())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Viewer = World->SpawnActor<AGFCModelViewer>(AGFCModelViewer::StaticClass(), FTransform::Identity, Params);
	if (!Viewer)
	{
		SetState(EGFCDetailState::Failed, LOCTEXT("NoViewer", "The 3D view couldn't start."));
		return;
	}
	// Render target sized to the screen's preview, capped for phones.
	Viewer->ShowModel(*Model, GetDefault<UGFCApiSettings>()->MaxTextureSize, 1024);
	if (ModelView)
	{
		ModelView->SetViewer(Viewer);
	}
	UE_LOG(LogGFCApi, Log, TEXT("3D view: %d triangles, %d sections, %d textures"), Model->Triangles, Model->Sections.Num(), Model->Images.Num());
	LogResourceUsage(TEXT("model ready"));
	SetState(EGFCDetailState::Ready, LOCTEXT("Gestures", "Drag to orbit · pinch to zoom · two fingers to pan"));
}

void UGFCSeedDetailScreen::ReleaseContent()
{
	if (bReleasing)
	{
		return;
	}
	TGuardValue<bool> Guard(bReleasing, true);
	++LoadSerial; // ignore anything still in flight
	if (ModelView) ModelView->SetViewer(nullptr);
	if (Viewer)
	{
		Viewer->Release();
		Viewer = nullptr;
	}
	if (Media)
	{
		Media->Release();
		Media = nullptr;
	}
	if (FullImage)
	{
		FullImage->ReleaseResource();
		FullImage = nullptr;
	}
	if (!Seed.Id.IsEmpty() && GetGameInstance())
	{
		if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
		{
			Cache->ReleaseOwner(OwnerKey()); // cancels the download and deletes the temp file
		}
	}
}

void UGFCSeedDetailScreen::HandleOwnerReleased(const FString& Owner)
{
	// The cache let go of this seed (app backgrounded, low memory, logout, another heavy item).
	if (Owner == OwnerKey() && !bReleasing && State != EGFCDetailState::Thumbnail)
	{
		ReleaseContent();
		SetState(EGFCDetailState::Thumbnail);
		Render();
	}
}

void UGFCSeedDetailScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Media && Media->GetVideoTexture())
	{
		Media->FitBrush(PreviewImage); // letterboxed at the video's own aspect, in any orientation
	}

	const bool bLoading =State == EGFCDetailState::Downloading || State == EGFCDetailState::Preparing;
	if (bLoading && FPlatformTime::Seconds() - LoadStarted > GetDefault<UGFCApiSettings>()->ModelLoadTimeoutSeconds)
	{
		ReleaseContent();
		SetState(EGFCDetailState::Failed, LOCTEXT("Timeout", "This took too long to load. Try again on a faster connection."));
		return;
	}

	// -GFCAutoBackAfter=N: leave the card N seconds after its content is ready, then log usage again.
	float AutoBack = 0.f;
	if (State == EGFCDetailState::Ready && FParse::Value(FCommandLine::Get(), TEXT("GFCAutoBackAfter="), AutoBack))
	{
		ReadySeconds += InDeltaTime;
		if (ReadySeconds >= AutoBack && Root.IsValid())
		{
			LogResourceUsage(TEXT("viewing"));
			ReadySeconds = -1000.f;
			Root->GoBack();
			TWeakObjectPtr<const UGFCSeedDetailScreen> WeakThis(this);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
			{
				if (WeakThis.IsValid()) WeakThis->LogResourceUsage(TEXT("3 s after leaving"));
				return false;
			}), 3.f);
			return;
		}
	}

	const bool bVisible = IsVisible() && !bAppInBackground;
	// A text post is all there as soon as it's on screen; media must be loaded first.
	const bool bLoaded = State == EGFCDetailState::Ready || Seed.GetMediaKind() == EGFCMediaKind::Text;
	if (Tracker.Tick(InDeltaTime, bVisible, bLoaded))
	{
		OnQualifiedView();
	}
	if (Media && Tracker.NotifyPlayback(Media->GetPositionSeconds(), Media->GetDurationSeconds()))
	{
		OnQualifiedView();
	}
}

void UGFCSeedDetailScreen::OnQualifiedView()
{
	UGFCApiClient* C = Client();
	if (!C || !C->IsLoggedIn())
	{
		return; // anonymous views can't pay anyone
	}
	const FString Title = Seed.Title;
	TWeakObjectPtr<UGFCSeedDetailScreen> WeakThis(this);
	C->RecordPlay(Seed.Id, [WeakThis, Title](const FGFCApiResult& R, const FGFCPlayResult& Play)
	{
		// Not crediting is normal (unverified phone, daily limit...): never shown as an error.
		if (!R.bOk)
		{
			UE_LOG(LogGFCApi, Verbose, TEXT("Play not recorded: %s"), *R.Error);
			return;
		}
		if (Play.bCredited && WeakThis.IsValid())
		{
			WeakThis->ShowStatus(FText::Format(LOCTEXT("Credited", "Your view of {0} earned its creator {1} GFC."),
				FText::FromString(Title), FText::AsNumber(Play.Amount)));
		}
		else
		{
			UE_LOG(LogGFCApi, Log, TEXT("View counted, not credited: %s"), *Play.Reason);
		}
	});
}

void UGFCSeedDetailScreen::HandleShare()
{
	const FString Url = GetShareUrl();
	const FString XUrl = FGFCUrl::XIntentUrl(FGFCUrl::ShareText(Seed.Title, Seed.Owner), Url);
	FString Error;
	FPlatformProcess::LaunchURL(*XUrl, nullptr, &Error);
	if (Error.IsEmpty())
	{
		ShowStatus(LOCTEXT("OpenedX", "Opened X to share."));
	}
	else
	{
		FPlatformApplicationMisc::ClipboardCopy(*Url);
		ShowStatus(LOCTEXT("NoBrowser", "Couldn't open X, so the link was copied instead."), true);
	}
}

void UGFCSeedDetailScreen::HandleCopyLink()
{
	FPlatformApplicationMisc::ClipboardCopy(*GetShareUrl());
	ShowStatus(LOCTEXT("Copied", "Share link copied."));
}

void UGFCSeedDetailScreen::HandleBack()
{
	if (Root.IsValid())
	{
		Root->GoBack();
	}
}

void UGFCSeedDetailScreen::HandleOpenMedia()
{
	if (const UGFCApiClient* C = Client(); C && !Seed.Full.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*C->ResolveMediaUrl(Seed.Full), nullptr, nullptr);
	}
}

#undef LOCTEXT_NAMESPACE
