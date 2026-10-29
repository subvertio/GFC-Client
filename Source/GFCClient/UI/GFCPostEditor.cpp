#include "UI/GFCPostEditor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "UI/GFCCreateScreen.h"
#include "UI/GFCSeedCard.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCPostEditor"

using namespace GFCUI;

namespace
{
	TWeakObjectPtr<UGFCPostPanel> GOpenPanel;

	UGFCApiClient* ClientFor(const UObject* Ctx)
	{
		const UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
		return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient() : nullptr;
	}

	UVerticalBox* Labelled(UWidgetTree* T, const FText& Label, UWidget* Input)
	{
		UVerticalBox* Box = MakeColumn(T);
		AddV(Box, MakeText(T, Label, 13, TextDim, true), 4.f);
		AddV(Box, Input, 0.f);
		return Box;
	}
}

// ---------------------------------------------------------------- rules (tested)

bool UGFCPostPanel::BuildRequest(const FGFCSeed& Seed, const FString& Title, const FString& Body, const FString& TagsText,
	FGFCSeedEditRequest& Out)
{
	Out = FGFCSeedEditRequest();
	const bool bText = Seed.GetMediaKind() == EGFCMediaKind::Text;
	const FString NewBody = bText ? Body.TrimStartAndEnd() : Body.TrimEnd();
	FString NewTitle = Title.TrimStartAndEnd();
	if (bText)
	{
		// Untouched or emptied, a text post's title is its first line, as when it was posted.
		const bool bAutoTitle = Seed.Title == UGFCCreateScreen::TitleFromBody(Seed.Description);
		if (NewTitle.IsEmpty() || (bAutoTitle && NewTitle == Seed.Title))
		{
			NewTitle = UGFCCreateScreen::TitleFromBody(NewBody);
		}
	}
	else if (NewTitle.IsEmpty())
	{
		NewTitle = Seed.Title;
	}
	if (NewTitle != Seed.Title) Out.Title = NewTitle;
	if (NewBody != Seed.Description) Out.Description = NewBody;
	const TArray<FString> NewTags = UGFCApiClient::SplitTags(TagsText);
	if (NewTags != Seed.Tags) Out.Tags = NewTags;
	return Out.Title.IsSet() || Out.Description.IsSet() || Out.Tags.IsSet();
}

FGFCSeed UGFCPostPanel::ApplyEdit(const FGFCSeed& Seed, const FGFCSeedEdit& Edit)
{
	FGFCSeed Out = Seed;
	Out.Title = Edit.Title;
	Out.Description = Edit.Description;
	Out.Tags = Edit.Tags;
	Out.EditedAt = Edit.EditedAt;
	return Out;
}

// ---------------------------------------------------------------- the panel

UGFCPostPanel* UGFCPostPanel::Open(UObject* WorldContext, const FGFCSeed& InSeed)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !World->GetGameInstance())
	{
		return nullptr;
	}
	CloseOpen();
	UGFCPostPanel* Sheet = CreateWidget<UGFCPostPanel>(World->GetGameInstance(), UGFCPostPanel::StaticClass());
	Sheet->Seed = InSeed;
	Sheet->AddToViewport(900); // over the screens, like the action sheet
	GOpenPanel = Sheet;
	return Sheet;
}

UGFCPostPanel* UGFCPostPanel::ShowEditor(UObject* WorldContext, const FGFCSeed& InSeed, FOnSaved InOnSaved)
{
	UGFCPostPanel* Sheet = Open(WorldContext, InSeed);
	if (Sheet)
	{
		Sheet->OnSaved = MoveTemp(InOnSaved);
		Sheet->BuildEditor();
	}
	return Sheet;
}

UGFCPostPanel* UGFCPostPanel::ShowHistory(UObject* WorldContext, const FGFCSeed& InSeed)
{
	UGFCPostPanel* Sheet = Open(WorldContext, InSeed);
	if (Sheet)
	{
		Sheet->BuildHistory();
	}
	return Sheet;
}

bool UGFCPostPanel::CloseOpen()
{
	if (UGFCPostPanel* Sheet = GOpenPanel.Get())
	{
		Sheet->HandleClose();
		return true;
	}
	return false;
}

void UGFCPostPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	// The dimmed screen behind: tapping it closes (an edit in progress is only sent by Save).
	UButton* Dim = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle DimStyle = FlatButtonStyle();
	DimStyle.Normal.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	DimStyle.Hovered.TintColor = DimStyle.Normal.TintColor;
	DimStyle.Pressed.TintColor = DimStyle.Normal.TintColor;
	Dim->SetStyle(DimStyle);
	Dim->OnClicked.AddDynamic(this, &ThisClass::HandleClose);
	if (UOverlaySlot* S = Root->AddChildToOverlay(Dim))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	// A card in the middle, no wider than the desktop feed column.
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetMaxDesiredWidth(Desktop::CenterWidth);
	UBorder* Card = MakeCard(WidgetTree, FMargin(16.f));
	List = MakeColumn(WidgetTree);
	Card->AddChild(List);
	Width->AddChild(Card);
	if (UOverlaySlot* S = Root->AddChildToOverlay(Width))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(12.f, 24.f));
	}
}

void UGFCPostPanel::NativeDestruct()
{
	if (GOpenPanel.Get() == this)
	{
		GOpenPanel.Reset();
	}
	Super::NativeDestruct();
}

void UGFCPostPanel::BuildEditor()
{
	UWidgetTree* T = WidgetTree;
	const bool bText = Seed.GetMediaKind() == EGFCMediaKind::Text;
	AddV(List, MakeText(T, LOCTEXT("EditTitle", "Edit post"), 18, Text, true), 4.f);
	AddV(List, MakeText(T, bText ? LOCTEXT("EditNoteText", "Fix a typo or change the text. Everyone can see the earlier versions.")
		: LOCTEXT("EditNoteMedia", "Change the title, description or tags. The file can't be changed. Everyone can see the earlier versions."),
		13, TextDim), 14.f);

	TitleBox = MakeInput(T, bText ? LOCTEXT("TitleHintText", "Title (the first line if empty)") : LOCTEXT("TitleHint", "Title"));
	TitleBox->SetText(FText::FromString(Seed.Title));
	BodyBox = MakeMultiLineInput(T, bText ? LOCTEXT("BodyHintText", "What's happening?") : LOCTEXT("BodyHint", "Description (optional)"));
	BodyBox->SetText(FText::FromString(Seed.Description));
	USizeBox* BodyHeight = T->ConstructWidget<USizeBox>();
	BodyHeight->SetMinDesiredHeight(bText ? 160.f : 110.f);
	BodyHeight->SetMaxDesiredHeight(320.f);
	BodyHeight->AddChild(BodyBox);
	TagsBox = MakeInput(T, LOCTEXT("TagsHint", "comma-separated, e.g. music, 3d"));
	TagsBox->SetText(FText::FromString(FString::Join(Seed.Tags, TEXT(", "))));

	// A text post reads body first; a media post leads with its title.
	if (bText)
	{
		AddV(List, Labelled(T, LOCTEXT("BodyL", "Text"), BodyHeight), 10.f);
		AddV(List, Labelled(T, LOCTEXT("TitleL", "Title"), TitleBox), 10.f);
	}
	else
	{
		AddV(List, Labelled(T, LOCTEXT("TitleL", "Title"), TitleBox), 10.f);
		AddV(List, Labelled(T, LOCTEXT("DescL", "Description"), BodyHeight), 10.f);
	}
	AddV(List, Labelled(T, LOCTEXT("TagsL", "Tags"), TagsBox), 12.f);

	StatusText = MakeText(T, FText::GetEmpty(), 13, TextDim);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	AddV(List, StatusText, 10.f);

	UHorizontalBox* Buttons = MakeRow(T);
	UButton* Cancel = MakeButton(T, LOCTEXT("Cancel", "Cancel"), Background);
	Cancel->OnClicked.AddDynamic(this, &ThisClass::HandleClose);
	UTextBlock* Label = nullptr;
	SaveButton = MakeButton(T, LOCTEXT("Save", "Save"), Accent, &Label);
	SaveLabel = Label;
	SaveButton->OnClicked.AddDynamic(this, &ThisClass::HandleSave);
	AddH(Buttons, Cancel, 8.f, true);
	AddH(Buttons, SaveButton, 0.f, true);
	AddV(List, Buttons, 0.f);
}

void UGFCPostPanel::SetStatus(const FText& Message, bool bError)
{
	if (!StatusText) return;
	StatusText->SetText(Message);
	StatusText->SetColorAndOpacity(FSlateColor(bError ? Danger : TextDim));
	StatusText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UGFCPostPanel::HandleSave()
{
	if (bSaving || !TitleBox || !BodyBox || !TagsBox)
	{
		return;
	}
	FGFCSeedEditRequest Edit;
	if (!BuildRequest(Seed, TitleBox->GetText().ToString(), BodyBox->GetText().ToString(), TagsBox->GetText().ToString(), Edit))
	{
		SetStatus(LOCTEXT("NoChange", "Nothing changed yet."), false);
		return;
	}
	UGFCApiClient* C = ClientFor(this);
	if (!C)
	{
		return;
	}
	bSaving = true;
	SaveButton->SetIsEnabled(false);
	if (SaveLabel) SaveLabel->SetText(LOCTEXT("Saving", "Saving…"));
	SetStatus(FText::GetEmpty(), false);
	TWeakObjectPtr<UGFCPostPanel> WeakThis(this);
	C->EditSeed(Seed.Id, Edit, [WeakThis](const FGFCApiResult& R, const FGFCSeedEdit& Saved)
	{
		if (!WeakThis.IsValid()) return;
		WeakThis->bSaving = false;
		WeakThis->SaveButton->SetIsEnabled(true);
		if (WeakThis->SaveLabel) WeakThis->SaveLabel->SetText(LOCTEXT("Save", "Save"));
		if (!R.bOk)
		{
			// The node says why: "nothing changed", too long, not your post, too many edits...
			WeakThis->SetStatus(R.Error.IsEmpty() ? LOCTEXT("SaveFailed", "Couldn't save the edit.") : FText::FromString(R.Error), true);
			return;
		}
		FOnSaved Done = MoveTemp(WeakThis->OnSaved);
		const FGFCSeed Updated = ApplyEdit(WeakThis->Seed, Saved);
		WeakThis->HandleClose();
		if (Done) Done(Updated);
	});
}

void UGFCPostPanel::BuildHistory()
{
	UWidgetTree* T = WidgetTree;
	AddV(List, MakeText(T, LOCTEXT("HistoryTitle", "Edit history"), 18, Text, true), 4.f);
	AddV(List, MakeText(T, LOCTEXT("HistoryNote", "Every version of this post's text, newest first."), 13, TextDim), 12.f);
	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	USizeBox* Height = T->ConstructWidget<USizeBox>();
	Height->SetMaxDesiredHeight(460.f);
	Height->AddChild(Scroll);
	Versions = MakeColumn(T);
	Scroll->AddChild(Versions);
	AddV(List, Height, 12.f);
	StatusText = MakeText(T, LOCTEXT("LoadingHistory", "Loading…"), 13, TextDim);
	AddV(Versions, StatusText, 0.f);
	UButton* Close = MakeButton(T, LOCTEXT("Close", "Close"), Background);
	Close->OnClicked.AddDynamic(this, &ThisClass::HandleClose);
	AddV(List, Close, 0.f);

	UGFCApiClient* C = ClientFor(this);
	if (!C)
	{
		return;
	}
	TWeakObjectPtr<UGFCPostPanel> WeakThis(this);
	C->GetSeedEdits(Seed.Id, [WeakThis](const FGFCApiResult& R, const TArray<FGFCSeedEdit>& Edits)
	{
		if (!WeakThis.IsValid()) return;
		if (!R.bOk)
		{
			WeakThis->SetStatus(R.Error.IsEmpty() ? LOCTEXT("HistoryFailed", "Couldn't load the history.") : FText::FromString(R.Error), true);
			return;
		}
		WeakThis->ShowEdits(Edits);
	});
}

void UGFCPostPanel::ShowEdits(const TArray<FGFCSeedEdit>& Edits)
{
	UWidgetTree* T = WidgetTree;
	Versions->ClearChildren();
	StatusText = nullptr;
	if (Edits.Num() == 0)
	{
		AddV(Versions, MakeText(T, LOCTEXT("NoEdits", "This post hasn't been edited."), 14, TextDim), 0.f);
		return;
	}
	for (int32 i = 0; i < Edits.Num(); ++i)
	{
		const FGFCSeedEdit& E = Edits[i];
		if (i > 0) AddV(Versions, MakeDivider(T), 10.f);
		const FText When = RelativeTime(E.EditedAt);
		AddV(Versions, MakeText(T, i == 0 ? FText::Format(LOCTEXT("Current", "{0} · current"), When) : When, 12, TextDim), 2.f);
		if (!E.Title.IsEmpty()) AddV(Versions, MakeText(T, FText::FromString(E.Title), 15, Text, true), 2.f);
		if (!E.Description.IsEmpty() && E.Description != E.Title)
		{
			AddV(Versions, MakeText(T, FText::FromString(UGFCSeedCard::FeedBody(E.Description, 400)), 14, Text), 2.f);
		}
		if (E.Tags.Num()) AddV(Versions, MakeText(T, FText::FromString(TEXT("#") + FString::Join(E.Tags, TEXT("  #"))), 12, TextDim), 0.f);
	}
}

void UGFCPostPanel::HandleClose()
{
	if (GOpenPanel.Get() == this)
	{
		GOpenPanel.Reset();
	}
	OnSaved = nullptr;
	RemoveFromParent();
}

#undef LOCTEXT_NAMESPACE

void UGFCPostPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	KeyboardScroll.Tick(WidgetTree, MyGeometry);
}
