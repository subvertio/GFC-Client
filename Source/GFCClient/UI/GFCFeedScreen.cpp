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
	SeedList->ClearChildren();
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
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}
		Card->SetSeed(Seed);
	}
}

#undef LOCTEXT_NAMESPACE
