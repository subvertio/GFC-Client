#include "UI/GFCFeedScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCCacheSubsystem.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCSeedCard.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCFeed"

using namespace GFCUI;

UGFCFeedScreen::UGFCFeedScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SeedCardClass = UGFCSeedCard::StaticClass();
}

void UGFCFeedScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;
	UVerticalBox* Col = MakeColumn(T);
	T->RootWidget = Col;

	UHorizontalBox* Header = MakeRow(T);
	AddH(Header, MakeText(T, LOCTEXT("Title", "Feed"), 22, Text, true), 0.f);
	AddH(Header, T->ConstructWidget<USpacer>(), 0.f, true);
	RefreshButton = MakeButton(T, LOCTEXT("Refresh", "Refresh"), PanelRaised);
	AddH(Header, RefreshButton, 0.f);
	AddV(Col, Header, 12.f);

	EmptyText = MakeText(T, FText::GetEmpty(), 13, TextDim);
	AddV(Col, EmptyText, 8.f);

	SeedList = T->ConstructWidget<UScrollBox>();
	AddV(Col, SeedList, 0.f, true);
}

void UGFCFeedScreen::BindEvents()
{
	if (RefreshButton) RefreshButton->OnClicked.AddDynamic(this, &ThisClass::HandleRefresh);
}

void UGFCFeedScreen::OnScreenShown()
{
	Refresh();
	Super::OnScreenShown();
}

void UGFCFeedScreen::OnScreenHidden()
{
	// Leaving the feed: every card lets go of its content.
	if (SeedList)
	{
		for (UWidget* W : SeedList->GetAllChildren())
		{
			if (UGFCSeedCard* Card = Cast<UGFCSeedCard>(W))
			{
				Card->ReleaseContent();
			}
		}
	}
	OffscreenSeconds.Reset();
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		Cache->SetVisibleThumbnails({});
	}
}

void UGFCFeedScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateCardVisibility(InDeltaTime);
}

void UGFCFeedScreen::UpdateCardVisibility(float DeltaTime)
{
	if (!SeedList || !IsVisible())
	{
		return;
	}
	const FGeometry& ListGeo = SeedList->GetCachedGeometry();
	const FVector2D ListTop = ListGeo.GetAbsolutePosition();
	const FVector2D ListSize = ListGeo.GetAbsoluteSize();
	if (ListSize.Y <= 0.0)
	{
		return; // not laid out yet
	}
	// A margin of a quarter screen above and below: cards about to scroll in are ready.
	const double Margin = ListSize.Y * 0.25;
	const double Top = ListTop.Y - Margin, Bottom = ListTop.Y + ListSize.Y + Margin;
	const float Grace = GetDefault<UGFCApiSettings>()->OffscreenGraceSeconds;

	TSet<FString> VisibleThumbs;
	for (UWidget* W : SeedList->GetAllChildren())
	{
		UGFCSeedCard* Card = Cast<UGFCSeedCard>(W);
		if (!Card)
		{
			continue;
		}
		const FGeometry& G = Card->GetCachedGeometry();
		const double CardTop = G.GetAbsolutePosition().Y, CardBottom = CardTop + G.GetAbsoluteSize().Y;
		const bool bOnScreen = G.GetAbsoluteSize().Y > 0.0 && CardBottom > Top && CardTop < Bottom;
		if (bOnScreen)
		{
			OffscreenSeconds.Remove(Card);
			Card->RestoreContent();
			VisibleThumbs.Add(Card->ThumbUrl());
		}
		else if (Card->IsContentLoaded())
		{
			float& Secs = OffscreenSeconds.FindOrAdd(Card);
			Secs += DeltaTime;
			if (Secs >= Grace)
			{
				Card->ReleaseContent(); // scrolled away: deleted, re-fetched if the user comes back
				OffscreenSeconds.Remove(Card);
			}
		}
	}
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		Cache->SetVisibleThumbnails(VisibleThumbs);
	}
}

void UGFCFeedScreen::HandleRefresh()
{
	Refresh();
}

void UGFCFeedScreen::Refresh()
{
	UGFCApiClient* C = Client();
	if (!C || bLoading)
	{
		return;
	}
	bLoading = true;
	if (EmptyText) EmptyText->SetText(LOCTEXT("Loading", "Loading the feed…"));
	TWeakObjectPtr<UGFCFeedScreen> WeakThis(this);
	C->GetFeed([WeakThis](const FGFCApiResult& R, const TArray<FGFCSeed>& Seeds)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->bLoading = false;
		if (!R.bOk)
		{
			if (WeakThis->EmptyText) WeakThis->EmptyText->SetText(LOCTEXT("Failed", "Couldn't load the feed."));
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		WeakThis->Populate(Seeds);
	});
}

void UGFCFeedScreen::Populate(const TArray<FGFCSeed>& Seeds)
{
	if (EmptyText)
	{
		EmptyText->SetText(Seeds.Num() == 0
			? LOCTEXT("Empty", "Nothing here yet. Seeds appear as creators upload them.")
			: FText::Format(LOCTEXT("Count", "{0} seeds"), FText::AsNumber(Seeds.Num())));
	}
	if (!SeedList)
	{
		return;
	}
	for (UWidget* W : SeedList->GetAllChildren())
	{
		if (UGFCSeedCard* Old = Cast<UGFCSeedCard>(W))
		{
			Old->ReleaseContent();
		}
	}
	SeedList->ClearChildren();
	OffscreenSeconds.Reset();
	TWeakObjectPtr<UGFCFeedScreen> WeakThis(this);
	for (const FGFCSeed& Seed : Seeds)
	{
		UGFCSeedCard* Card = CreateWidget<UGFCSeedCard>(this, SeedCardClass);
		if (!Card)
		{
			continue;
		}
		Card->OnSelected.BindLambda([WeakThis](const FGFCSeed& Selected)
		{
			if (WeakThis.IsValid() && WeakThis->Root.IsValid())
			{
				WeakThis->Root->ShowSeed(Selected);
			}
		});
		UPanelSlot* PanelSlot = SeedList->AddChild(Card);
		if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(PanelSlot))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}
		Card->SetSeed(Seed);
	}
}

#undef LOCTEXT_NAMESPACE
