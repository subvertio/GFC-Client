#include "UI/GFCMusicWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "UI/GFCUIStyle.h"
#include "Viewer/GFCMusicPlayer.h"

#define LOCTEXT_NAMESPACE "GFCMusic"

using namespace GFCUI;

namespace
{
	FText PlayPauseLabel(const UGFCMusicPlayer* M, const FString& SeedId)
	{
		if (!M || (!SeedId.IsEmpty() && !M->IsCurrent(SeedId))) return LOCTEXT("Play", "Play");
		switch (M->GetState())
		{
		case EGFCMusicState::Loading: return LOCTEXT("Loading", "Loading…");
		case EGFCMusicState::Playing: return LOCTEXT("Pause", "Pause");
		case EGFCMusicState::Failed: return LOCTEXT("Retry", "Try again");
		default: return LOCTEXT("Play", "Play");
		}
	}

	void StyleSlider(USlider* S)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		FSliderStyle Style = S->GetWidgetStyle();
		Style.SetNormalBarImage(FSlateRoundedBoxBrush(Th.Border, 2.f));
		Style.SetHoveredBarImage(FSlateRoundedBoxBrush(Th.Border, 2.f));
		Style.SetNormalThumbImage(FSlateRoundedBoxBrush(FLinearColor::White, 9.f, FVector2f(18.f, 18.f)));
		Style.SetHoveredThumbImage(FSlateRoundedBoxBrush(FLinearColor::White, 9.f, FVector2f(18.f, 18.f)));
		Style.SetBarThickness(4.f);
		S->SetWidgetStyle(Style);
		S->SetSliderBarColor(Th.Border);
		S->SetSliderHandleColor(FLinearColor::White);
	}
}

// ---------------------------------------------------------------- full player

UGFCMusicPlayer* UGFCMusicPanel::Music() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGFCMusicPlayer>() : nullptr;
}

void UGFCMusicPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UWidgetTree* T = WidgetTree;
	UBorder* Card = MakeCard(T, FMargin(16.f));
	T->RootWidget = Card;
	UVerticalBox* Col = MakeColumn(T);
	Card->AddChild(Col);

	TrackText = MakeText(T, FText::GetEmpty(), 13, TextDim);
	AddV(Col, TrackText, 10.f);

	SeekSlider = T->ConstructWidget<USlider>();
	StyleSlider(SeekSlider);
	AddV(Col, MinHeight(T, SeekSlider, 32.f), 2.f);

	UHorizontalBox* Times = MakeRow(T);
	ElapsedText = MakeText(T, FText::FromString(TEXT("0:00")), 12, TextDim);
	TotalText = MakeText(T, FText::FromString(TEXT("0:00")), 12, TextDim);
	AddH(Times, ElapsedText, 0.f, true);
	AddH(Times, TotalText, 0.f);
	AddV(Col, Times, 14.f);

	UHorizontalBox* Controls = MakeRow(T);
	UTextBlock* L = nullptr;
	BackButton = MakeButton(T, LOCTEXT("Back15", "-15s"), PanelRaised);
	PlayButton = MakeButton(T, LOCTEXT("Play", "Play"), Accent, &L);
	PlayLabel = L;
	ForwardButton = MakeButton(T, LOCTEXT("Fwd15", "+15s"), PanelRaised);
	LoopButton = MakeButton(T, LOCTEXT("Loop", "Loop"), PanelRaised, &L);
	LoopLabel = L;
	AddH(Controls, BackButton, 8.f, true);
	AddH(Controls, PlayButton, 8.f, true);
	AddH(Controls, ForwardButton, 8.f, true);
	AddH(Controls, LoopButton, 0.f, true);
	AddV(Col, Controls, 8.f);

	StatusText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	AddV(Col, StatusText, 0.f);

	PlayButton->OnClicked.AddDynamic(this, &ThisClass::HandlePlay);
	BackButton->OnClicked.AddDynamic(this, &ThisClass::HandleBack);
	ForwardButton->OnClicked.AddDynamic(this, &ThisClass::HandleForward);
	LoopButton->OnClicked.AddDynamic(this, &ThisClass::HandleLoop);
	SeekSlider->OnMouseCaptureBegin.AddDynamic(this, &ThisClass::HandleSeekStarted);
	SeekSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleSeekEnded);
	SeekSlider->OnControllerCaptureBegin.AddDynamic(this, &ThisClass::HandleSeekStarted);
	SeekSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleSeekEnded);

	if (UGFCMusicPlayer* M = Music())
	{
		ChangedHandle = M->OnChanged.AddUObject(this, &UGFCMusicPanel::Refresh);
	}
}

void UGFCMusicPanel::NativeDestruct()
{
	if (UGFCMusicPlayer* M = Music()) M->OnChanged.Remove(ChangedHandle);
	Super::NativeDestruct();
}

void UGFCMusicPanel::SetSeed(const FGFCSeed& InSeed)
{
	Seed = InSeed;
	Refresh();
}

void UGFCMusicPanel::Refresh()
{
	UGFCMusicPlayer* M = Music();
	const bool bCurrent = M && M->IsCurrent(Seed.Id);
	if (PlayLabel) PlayLabel->SetText(PlayPauseLabel(M, Seed.Id));
	if (LoopLabel) LoopLabel->SetText(M && M->IsLooping() ? LOCTEXT("LoopOn", "Loop: on") : LOCTEXT("LoopOff", "Loop"));
	if (LoopLabel) LoopLabel->SetColorAndOpacity(FSlateColor(M && M->IsLooping() ? AccentText : Text));
	for (UButton* B : {BackButton.Get(), ForwardButton.Get()})
	{
		if (B) B->SetIsEnabled(bCurrent && M->GetState() != EGFCMusicState::Loading);
	}
	if (SeekSlider) SeekSlider->SetIsEnabled(bCurrent && M->GetDuration() > 0.f);
	if (TrackText) TrackText->SetText(FText::Format(LOCTEXT("Track", "{0} · @{1}"), FText::FromString(Seed.Title), FText::FromString(Seed.Owner)));
	if (StatusText)
	{
		const bool bFailed = bCurrent && M->GetState() == EGFCMusicState::Failed;
		StatusText->SetText(bFailed ? FText::FromString(M->GetError()) : FText::GetEmpty());
		StatusText->SetColorAndOpacity(FSlateColor(bFailed ? Danger : TextDim));
		StatusText->SetVisibility(bFailed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bCurrent)
	{
		if (SeekSlider) SeekSlider->SetValue(0.f);
		if (ElapsedText) ElapsedText->SetText(FText::FromString(TEXT("0:00")));
		if (TotalText) TotalText->SetText(FText::FromString(TEXT("--:--")));
	}
}

void UGFCMusicPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UGFCMusicPlayer* M = Music();
	if (!M || !M->IsCurrent(Seed.Id))
	{
		return;
	}
	const float Duration = M->GetDuration();
	const float Position = M->GetPosition();
	if (TotalText) TotalText->SetText(FText::FromString(Duration > 0.f ? UGFCMusicPlayer::FormatTime(Duration) : TEXT("--:--")));
	if (bDragging)
	{
		// While dragging, the time follows the thumb, not the music.
		if (ElapsedText && SeekSlider) ElapsedText->SetText(FText::FromString(UGFCMusicPlayer::FormatTime(SeekSlider->GetValue() * Duration)));
		return;
	}
	if (ElapsedText) ElapsedText->SetText(FText::FromString(UGFCMusicPlayer::FormatTime(Position)));
	if (SeekSlider) SeekSlider->SetValue(Duration > 0.f ? FMath::Clamp(Position / Duration, 0.f, 1.f) : 0.f);
	if (SeekSlider && !SeekSlider->GetIsEnabled() && Duration > 0.f) Refresh();
	if (PlayLabel) PlayLabel->SetText(PlayPauseLabel(M, Seed.Id)); // reaching the end turns Pause back into Play
}

void UGFCMusicPanel::HandlePlay()
{
	UGFCMusicPlayer* M = Music();
	if (!M) return;
	if (M->IsCurrent(Seed.Id)) M->TogglePause();
	else M->Play(Seed);
}

void UGFCMusicPanel::HandleBack()
{
	if (UGFCMusicPlayer* M = Music(); M && M->IsCurrent(Seed.Id)) M->Skip(-15.f);
}

void UGFCMusicPanel::HandleForward()
{
	if (UGFCMusicPlayer* M = Music(); M && M->IsCurrent(Seed.Id)) M->Skip(15.f);
}

void UGFCMusicPanel::HandleLoop()
{
	if (UGFCMusicPlayer* M = Music()) M->SetLooping(!M->IsLooping());
}

void UGFCMusicPanel::HandleSeekStarted()
{
	bDragging = true;
}

void UGFCMusicPanel::HandleSeekEnded()
{
	bDragging = false;
	if (UGFCMusicPlayer* M = Music(); M && M->IsCurrent(Seed.Id) && SeekSlider)
	{
		M->Seek(SeekSlider->GetValue() * M->GetDuration());
	}
}

// ---------------------------------------------------------------- mini player

UGFCMusicPlayer* UGFCMiniPlayer::Music() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGFCMusicPlayer>() : nullptr;
}

void UGFCMiniPlayer::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UWidgetTree* T = WidgetTree;
	UBorder* Bg = MakePanel(T, Panel, FMargin(0.f));
	T->RootWidget = Bg;
	UVerticalBox* Col = MakeColumn(T);
	Bg->AddChild(Col);

	Progress = T->ConstructWidget<UProgressBar>();
	Progress->SetFillColorAndOpacity(Accent);
	Progress->SetPercent(0.f);
	USizeBox* Thin = MinHeight(T, Progress, 3.f);
	AddV(Col, Thin, 0.f);

	UBorder* Pad = MakePanel(T, Panel, FMargin(12.f, 6.f, 8.f, 6.f));
	UHorizontalBox* Row = MakeRow(T);
	Pad->AddChild(Row);
	AddV(Col, Pad, 0.f);

	// The track: tap to open the post.
	OpenButton = T->ConstructWidget<UButton>();
	FButtonStyle Clear = FlatButtonStyle();
	Clear.Normal.TintColor = FSlateColor(FLinearColor::Transparent);
	Clear.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.04f));
	Clear.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f));
	OpenButton->SetStyle(Clear);
	UVerticalBox* Info = MakeColumn(T);
	TitleText = MakeText(T, FText::GetEmpty(), 14, Text, true);
	TitleText->SetAutoWrapText(false);
	TitleText->SetClipping(EWidgetClipping::ClipToBounds);
	OwnerText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	OwnerText->SetAutoWrapText(false);
	OwnerText->SetClipping(EWidgetClipping::ClipToBounds);
	AddV(Info, TitleText, 0.f);
	AddV(Info, OwnerText, 0.f);
	OpenButton->AddChild(Info);
	if (UButtonSlot* S = Cast<UButtonSlot>(Info->Slot)) S->SetHorizontalAlignment(HAlign_Fill);
	AddH(Row, OpenButton, 8.f, true);

	UTextBlock* L = nullptr;
	PlayButton = MakeButton(T, LOCTEXT("Pause", "Pause"), Accent, &L);
	PlayLabel = L;
	CloseButton = MakeButton(T, FText::FromString(TEXT("×")), PanelRaised);
	AddH(Row, PlayButton, 6.f);
	AddH(Row, CloseButton, 0.f);
	for (UWidget* W : TArray<UWidget*>{PlayButton, CloseButton})
	{
		if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(W->Slot)) S->SetVerticalAlignment(VAlign_Center);
	}

	OpenButton->OnClicked.AddDynamic(this, &ThisClass::HandleOpen);
	PlayButton->OnClicked.AddDynamic(this, &ThisClass::HandlePlay);
	CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleClose);
	if (UGFCMusicPlayer* M = Music())
	{
		ChangedHandle = M->OnChanged.AddUObject(this, &UGFCMiniPlayer::Refresh);
	}
	Refresh();
}

void UGFCMiniPlayer::NativeDestruct()
{
	if (UGFCMusicPlayer* M = Music()) M->OnChanged.Remove(ChangedHandle);
	Super::NativeDestruct();
}

void UGFCMiniPlayer::Refresh()
{
	UGFCMusicPlayer* M = Music();
	const bool bActive = M && M->IsActive();
	SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bActive) return;
	if (TitleText) TitleText->SetText(FText::FromString(M->GetSeed().Title));
	if (OwnerText)
	{
		OwnerText->SetText(M->GetState() == EGFCMusicState::Failed ? FText::FromString(M->GetError())
			: FText::Format(LOCTEXT("By", "@{0}"), FText::FromString(M->GetSeed().Owner)));
		OwnerText->SetColorAndOpacity(FSlateColor(M->GetState() == EGFCMusicState::Failed ? Danger : TextDim));
	}
	if (PlayLabel) PlayLabel->SetText(PlayPauseLabel(M, FString()));
}

void UGFCMiniPlayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UGFCMusicPlayer* M = Music();
	if (!M || !M->IsActive()) return;
	const float Duration = M->GetDuration();
	if (Progress) Progress->SetPercent(Duration > 0.f ? FMath::Clamp(M->GetPosition() / Duration, 0.f, 1.f) : 0.f);
	if (PlayLabel) PlayLabel->SetText(PlayPauseLabel(M, FString()));
}

void UGFCMiniPlayer::HandleOpen()
{
	if (UGFCMusicPlayer* M = Music(); M && M->IsActive() && OnOpen) OnOpen(M->GetSeed());
}

void UGFCMiniPlayer::HandlePlay()
{
	if (UGFCMusicPlayer* M = Music()) M->TogglePause();
}

void UGFCMiniPlayer::HandleClose()
{
	if (UGFCMusicPlayer* M = Music()) M->Stop();
}

#undef LOCTEXT_NAMESPACE
