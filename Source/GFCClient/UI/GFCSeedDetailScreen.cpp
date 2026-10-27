#include "UI/GFCSeedDetailScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCUrl.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCSeedCard.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCSeedDetail"

using namespace GFCUI;

void UGFCSeedDetailScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;
	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	T->RootWidget = Scroll;
	UVerticalBox* Col = MakeColumn(T);
	Scroll->AddChild(Col);

	BackButton = MakeButton(T, LOCTEXT("Back", "← Back to feed"), PanelRaised);
	UHorizontalBox* BackRow = MakeRow(T);
	AddH(BackRow, BackButton, 0.f);
	AddV(Col, BackRow, 12.f);

	UHorizontalBox* Main = MakeRow(T);
	AddV(Col, Main, 0.f);

	// Preview
	USizeBox* PreviewBox = T->ConstructWidget<USizeBox>();
	PreviewBox->SetWidthOverride(420.f);
	PreviewBox->SetHeightOverride(420.f);
	UBorder* PreviewBg = MakePanel(T, Panel, FMargin(0.f));
	UOverlay* Over = T->ConstructWidget<UOverlay>();
	PreviewImage = T->ConstructWidget<UImage>();
	PreviewNote = MakeText(T, FText::GetEmpty(), 13, Text, true);
	PreviewNote->SetJustification(ETextJustify::Center);
	UBorder* NoteBg = MakePanel(T, FLinearColor(0.f, 0.f, 0.f, 0.65f), FMargin(10.f));
	NoteBg->AddChild(PreviewNote);
	if (UOverlaySlot* S = Over->AddChildToOverlay(PreviewImage))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* S = Over->AddChildToOverlay(NoteBg))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Bottom);
	}
	PreviewBg->AddChild(Over);
	PreviewBox->AddChild(PreviewBg);
	AddH(Main, PreviewBox, 24.f);

	// Details
	UVerticalBox* Info = MakeColumn(T);
	TitleText = MakeText(T, FText::GetEmpty(), 26, Text, true);
	OwnerText = MakeText(T, FText::GetEmpty(), 14, Accent);
	MetaText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	DescriptionText = MakeText(T, FText::GetEmpty(), 14, Text);
	AddV(Info, TitleText, 4.f);
	AddV(Info, OwnerText, 10.f);
	AddV(Info, MetaText, 16.f);
	AddV(Info, DescriptionText, 24.f);

	UHorizontalBox* Actions = MakeRow(T);
	ShareButton = MakeButton(T, LOCTEXT("ShareX", "Share on X"));
	CopyLinkButton = MakeButton(T, LOCTEXT("Copy", "Copy link"), PanelRaised);
	OpenMediaButton = MakeButton(T, LOCTEXT("Open", "Open original"), PanelRaised);
	AddH(Actions, ShareButton);
	AddH(Actions, CopyLinkButton);
	AddH(Actions, OpenMediaButton, 0.f);
	AddV(Info, Actions, 8.f);
	ShareLinkText = MakeText(T, FText::GetEmpty(), 11, TextDim);
	AddV(Info, ShareLinkText, 0.f);
	AddH(Main, Info, 0.f, true);
}

void UGFCSeedDetailScreen::BindEvents()
{
	if (ShareButton) ShareButton->OnClicked.AddDynamic(this, &ThisClass::HandleShare);
	if (CopyLinkButton) CopyLinkButton->OnClicked.AddDynamic(this, &ThisClass::HandleCopyLink);
	if (BackButton) BackButton->OnClicked.AddDynamic(this, &ThisClass::HandleBack);
	if (OpenMediaButton) OpenMediaButton->OnClicked.AddDynamic(this, &ThisClass::HandleOpenMedia);
}

void UGFCSeedDetailScreen::SetSeed(const FGFCSeed& InSeed)
{
	Seed = InSeed;
	Render();
}

void UGFCSeedDetailScreen::OnScreenShown()
{
	// Refresh counts from the node; the feed entry may be stale.
	if (UGFCApiClient* C = Client(); C && !Seed.Id.IsEmpty())
	{
		TWeakObjectPtr<UGFCSeedDetailScreen> WeakThis(this);
		const FString Id = Seed.Id;
		C->GetSeed(Id, [WeakThis, Id](const FGFCApiResult& R, const FGFCSeed& Fresh)
		{
			if (WeakThis.IsValid() && R.bOk && WeakThis->Seed.Id == Id)
			{
				const bool bLiked = WeakThis->Seed.bLiked, bBookmarked = WeakThis->Seed.bBookmarked;
				WeakThis->Seed = Fresh;
				WeakThis->Seed.bLiked = bLiked; // /seeds/{id} has no per-user state
				WeakThis->Seed.bBookmarked = bBookmarked;
				WeakThis->Render();
			}
		});
	}
	Super::OnScreenShown();
}

FString UGFCSeedDetailScreen::GetShareUrl() const
{
	return UGFCApiClient::BuildShareUrl(Seed.Id);
}

void UGFCSeedDetailScreen::Render()
{
	const EGFCMediaKind Kind = Seed.GetMediaKind();
	if (TitleText) TitleText->SetText(FText::FromString(Seed.Title));
	if (OwnerText) OwnerText->SetText(FText::Format(LOCTEXT("By", "by {0}"), FText::FromString(Seed.Owner)));
	if (DescriptionText)
	{
		DescriptionText->SetText(Seed.Description.IsEmpty() ? LOCTEXT("NoDesc", "No description.") : FText::FromString(Seed.Description));
	}
	if (MetaText)
	{
		const FString Tags = Seed.Tags.Num() ? FString::Join(Seed.Tags, TEXT(", ")) : FString(TEXT("none"));
		MetaText->SetText(FText::Format(
			LOCTEXT("Meta", "{0} · {1} views · {2} likes · {3} comments · earned {4} GFC\nTags: {5}"),
			GFCMediaKindLabel(Kind), FText::AsNumber(Seed.Views), FText::AsNumber(Seed.Likes), FText::AsNumber(Seed.Comments),
			FText::AsNumber(Seed.GfcEarned), FText::FromString(Tags)));
	}
	if (PreviewNote)
	{
		FText Note;
		if (Kind == EGFCMediaKind::Model3D)
		{
			Note = LOCTEXT("ModelSoon", "3D model · interactive 3D view coming in milestone 2");
		}
		else if (Seed.Thumb.IsEmpty())
		{
			Note = LOCTEXT("NoThumb", "No preview");
		}
		PreviewNote->SetText(Note);
		if (UWidget* NoteParent = PreviewNote->GetParent())
		{
			NoteParent->SetVisibility(Note.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
	if (PreviewImage)
	{
		if (const UGFCApiClient* C = Client())
		{
			GFCLoadImageInto(this, PreviewImage, C->ResolveMediaUrl(Seed.Thumb));
		}
	}
	if (ShareLinkText) ShareLinkText->SetText(FText::FromString(GetShareUrl()));
}

void UGFCSeedDetailScreen::HandleShare()
{
	const FString Url = GetShareUrl();
	const FString XUrl = FGFCUrl::XIntentUrl(FGFCUrl::ShareText(Seed.Title, Seed.Owner), Url);
	FString Error;
	FPlatformProcess::LaunchURL(*XUrl, nullptr, &Error);
	if (Error.IsEmpty())
	{
		ShowStatus(LOCTEXT("Opened", "Opened X in your browser."));
	}
	else
	{
		FPlatformApplicationMisc::ClipboardCopy(*Url);
		ShowStatus(LOCTEXT("NoBrowser", "Couldn't open a browser, so the link was copied instead."), true);
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
		Root->ShowFeed();
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
