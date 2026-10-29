#include "UI/GFCDesktopRails.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "GFCJson.h"
#include "UI/GFCActionSheet.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCSeedCard.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCDesktopRails"

using namespace GFCUI;

namespace
{
	UGFCApiClient* ClientOf(const UUserWidget* W)
	{
		return W->GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	}

	FText Gfc(double Amount)
	{
		FNumberFormattingOptions Two;
		Two.MinimumFractionalDigits = 2;
		Two.MaximumFractionalDigits = 2;
		return FText::Format(LOCTEXT("GfcAmount", "{0} GFC"), FText::AsNumber(Amount, &Two));
	}
}

// ---------------------------------------------------------------------------
// Left rail
// ---------------------------------------------------------------------------

void UGFCLeftRail::SetRoot(UGFCRootWidget* InRoot) { Root = InRoot; }

void UGFCLeftRail::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		Build();
	}
}

void UGFCLeftRail::Build()
{
	UWidgetTree* T = WidgetTree;
	const FGFCTheme& Th = FGFCTheme::Get();

	UVerticalBox* Col = MakeColumn(T);
	T->RootWidget = Col;

	// The logo goes home, like the Home item.
	UButton* LogoButton = T->ConstructWidget<UButton>();
	LogoButton->SetStyle(GhostButtonStyle(Th.SurfacePressed, 26.f));
	LogoButton->AddChild(MakeLogo(T, 40.f));
	if (UButtonSlot* S = Cast<UButtonSlot>(LogoButton->GetChildAt(0)->Slot)) S->SetPadding(FMargin(6.f));
	AddV(Col, LogoButton, 4.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(LogoButton->Slot))
	{
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));
	}

	struct FDef { EGFCNavItem Item; const TCHAR* Glyph; FText Label; bool bNeedsLogin; };
	const TArray<FDef> Defs = {
		{EGFCNavItem::Home, Icon::Home, LOCTEXT("Home", "Home"), false},
		{EGFCNavItem::Explore, Icon::Explore, LOCTEXT("Explore", "Explore"), false},
		{EGFCNavItem::Bookmarks, Icon::Bookmark, LOCTEXT("Bookmarks", "Bookmarks"), true},
		{EGFCNavItem::Wallet, Icon::Wallet, LOCTEXT("Wallet", "Wallet"), true},
		{EGFCNavItem::Profile, Icon::Profile, LOCTEXT("Profile", "Profile"), true},
	};
	TWeakObjectPtr<UGFCLeftRail> WeakThis(this);
	auto Go = [WeakThis](int32 Index)
	{
		if (!WeakThis.IsValid() || !WeakThis->Root.IsValid() || !WeakThis->Items.IsValidIndex(Index)) return;
		UGFCRootWidget* R = WeakThis->Root.Get();
		switch (WeakThis->Items[Index].Item)
		{
		case EGFCNavItem::Home: R->ShowHome(); break;
		case EGFCNavItem::Explore: R->ShowExplore(FString()); break;
		case EGFCNavItem::Bookmarks: R->ShowBookmarks(); break;
		case EGFCNavItem::Wallet: R->ShowWallet(); break;
		case EGFCNavItem::Profile: R->ShowProfile(); break;
		default: break;
		}
	};
	{
		UGFCRailLink* HomeLink = NewObject<UGFCRailLink>(this);
		HomeLink->Index = 0;
		HomeLink->OnClick = Go;
		Links.Add(HomeLink);
		LogoButton->OnClicked.AddDynamic(HomeLink, &UGFCRailLink::Clicked);
	}
	for (const FDef& D : Defs)
	{
		FItem It;
		It.Item = D.Item;
		It.bNeedsLogin = D.bNeedsLogin;
		It.Button = T->ConstructWidget<UButton>();
		It.Button->SetStyle(GhostButtonStyle(Th.SurfacePressed, 26.f));
		UHorizontalBox* Row = MakeRow(T);
		It.Glyph = MakeText(T, FText::FromString(D.Glyph), 22, Th.Text, false);
		It.Glyph->SetAutoWrapText(false);
		It.Glyph->SetJustification(ETextJustify::Center);
		USizeBox* GlyphBox = T->ConstructWidget<USizeBox>();
		GlyphBox->SetWidthOverride(28.f);
		GlyphBox->AddChild(It.Glyph);
		AddH(Row, GlyphBox, 18.f);
		It.Label = MakeText(T, D.Label, 19, Th.Text, false);
		It.Label->SetAutoWrapText(false);
		AddH(Row, It.Label, 8.f);
		It.Button->AddChild(Row);
		if (UButtonSlot* S = Cast<UButtonSlot>(Row->Slot)) S->SetPadding(FMargin(12.f, 10.f, 16.f, 10.f));
		AddV(Col, It.Button, 2.f);
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(It.Button->Slot)) S->SetHorizontalAlignment(HAlign_Left);

		UGFCRailLink* Link = NewObject<UGFCRailLink>(this);
		Link->Index = Items.Num();
		Link->OnClick = Go;
		Links.Add(Link);
		It.Button->OnClicked.AddDynamic(Link, &UGFCRailLink::Clicked);
		Items.Add(It);
	}

	PostButton = MakeButton(T, LOCTEXT("Post", "Post"));
	if (UButtonSlot* S = Cast<UButtonSlot>(PostButton->GetChildAt(0)->Slot)) S->SetPadding(FMargin(18.f, 15.f));
	if (UTextBlock* L = Cast<UTextBlock>(PostButton->GetChildAt(0)))
	{
		FSlateFontInfo F = L->GetFont();
		F.Size = 17;
		L->SetFont(F);
	}
	PostButton->OnClicked.AddDynamic(this, &ThisClass::HandlePost);
	AddV(Col, PostButton, 0.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(PostButton->Slot)) S->SetPadding(FMargin(0.f, 14.f, 16.f, 0.f));
	LoginButton = MakeButton(T, LOCTEXT("Login", "Log in"));
	if (UButtonSlot* S = Cast<UButtonSlot>(LoginButton->GetChildAt(0)->Slot)) S->SetPadding(FMargin(18.f, 15.f));
	LoginButton->OnClicked.AddDynamic(this, &ThisClass::HandleLogin);
	AddV(Col, LoginButton, 0.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(LoginButton->Slot)) S->SetPadding(FMargin(0.f, 14.f, 16.f, 0.f));

	AddV(Col, T->ConstructWidget<USpacer>(), 0.f, true);

	// The account: avatar, name, @handle, and its menu.
	AccountButton = T->ConstructWidget<UButton>();
	AccountButton->SetStyle(GhostButtonStyle(Th.SurfacePressed, 32.f));
	UHorizontalBox* Acc = MakeRow(T);
	AccountAvatar = T->ConstructWidget<USizeBox>();
	AccountAvatar->SetWidthOverride(40.f);
	AccountAvatar->SetHeightOverride(40.f);
	AddH(Acc, AccountAvatar, 12.f);
	UVerticalBox* Names = MakeColumn(T);
	AccountName = MakeText(T, FText::GetEmpty(), 15, Th.Text, true);
	AccountName->SetAutoWrapText(false);
	AccountName->SetClipping(EWidgetClipping::ClipToBounds);
	AccountHandle = MakeText(T, FText::GetEmpty(), 15, Th.TextSecondary);
	AccountHandle->SetAutoWrapText(false);
	AccountHandle->SetClipping(EWidgetClipping::ClipToBounds);
	AddV(Names, AccountName, 0.f);
	AddV(Names, AccountHandle, 0.f);
	AddH(Acc, Names, 8.f, true);
	UTextBlock* Dots = MakeText(T, FText::FromString(Icon::More), 13, Th.Text, true);
	Dots->SetAutoWrapText(false);
	AddH(Acc, Dots, 0.f);
	AccountButton->AddChild(Acc);
	if (UButtonSlot* S = Cast<UButtonSlot>(Acc->Slot))
	{
		S->SetPadding(FMargin(12.f));
		S->SetHorizontalAlignment(HAlign_Fill);
	}
	AccountButton->SetToolTipText(LOCTEXT("AccountTip", "Account"));
	AccountButton->OnClicked.AddDynamic(this, &ThisClass::HandleAccount);
	AddV(Col, AccountButton, 12.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(AccountButton->Slot)) S->SetPadding(FMargin(0.f, 12.f, 8.f, 12.f));

	Refresh();
}

void UGFCLeftRail::SetActive(EGFCNavItem Item)
{
	Active = Item;
	for (const FItem& It : Items)
	{
		const bool bOn = It.Item == Active;
		FSlateFontInfo F = It.Label->GetFont();
		F.TypefaceFontName = bOn ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		It.Label->SetFont(F);
		FSlateFontInfo G = It.Glyph->GetFont();
		G.TypefaceFontName = F.TypefaceFontName;
		It.Glyph->SetFont(G);
		if (It.Item == EGFCNavItem::Bookmarks)
		{
			It.Glyph->SetText(FText::FromString(bOn ? Icon::Bookmarked : Icon::Bookmark));
		}
	}
}

void UGFCLeftRail::Refresh()
{
	const UGFCApiClient* C = ClientOf(this);
	const bool bIn = C->IsLoggedIn();
	for (const FItem& It : Items)
	{
		It.Button->SetVisibility(!It.bNeedsLogin || bIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (PostButton) PostButton->SetVisibility(bIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (LoginButton) LoginButton->SetVisibility(bIn ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (AccountButton) AccountButton->SetVisibility(bIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bIn)
	{
		const FString Name = C->GetUsername();
		AccountAvatar->ClearChildren();
		AccountAvatar->AddChild(MakeAvatar(WidgetTree, Name, 40.f));
		AccountName->SetText(FText::FromString(Name));
		AccountHandle->SetText(FText::FromString(TEXT("@") + Name));
	}
	SetActive(Active);
}

void UGFCLeftRail::HandlePost()
{
	if (Root.IsValid()) Root->ShowCreate();
}

void UGFCLeftRail::HandleLogin()
{
	if (Root.IsValid()) Root->ShowLogin();
}

void UGFCLeftRail::HandleAccount()
{
	const FString Name = ClientOf(this)->GetUsername();
	TWeakObjectPtr<UGFCLeftRail> WeakThis(this);
	UGFCActionSheet::Show(this, FText::FromString(TEXT("@") + Name),
		{LOCTEXT("OpenProfile", "Your posts"), FText::Format(LOCTEXT("LogOut", "Log out @{0}"), FText::FromString(Name))},
		[WeakThis](int32 Index)
		{
			if (!WeakThis.IsValid() || !WeakThis->Root.IsValid()) return;
			if (Index == 0) WeakThis->Root->ShowProfile();
			if (Index == 1) WeakThis->Root->Logout();
		}, true);
}

// ---------------------------------------------------------------------------
// Right rail
// ---------------------------------------------------------------------------

void UGFCRightRail::SetRoot(UGFCRootWidget* InRoot) { Root = InRoot; }

void UGFCRightRail::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		Build();
	}
}

UWidget* UGFCRightRail::MakeCardTitle(const FText& Title)
{
	UTextBlock* T = MakeText(WidgetTree, Title, 20, FGFCTheme::Get().Text, true);
	return T;
}

void UGFCRightRail::Build()
{
	UWidgetTree* T = WidgetTree;
	const FGFCTheme& Th = FGFCTheme::Get();

	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	T->RootWidget = Scroll;
	UVerticalBox* Col = MakeColumn(T);
	Scroll->AddChild(Col);

	SearchBox = MakeSearchBox(T, LOCTEXT("SearchHint", "Search GFC"));
	SearchBox->OnTextCommitted.AddDynamic(this, &ThisClass::HandleSearch);
	AddV(Col, SearchBox, 16.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(SearchBox->Slot)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 16.f));

	// Your GFC: the balance of the user's wallets on this node.
	{
		UBorder* Card = MakeCard(T, FMargin(16.f, 12.f));
		UVerticalBox* C = MakeColumn(T);
		AddV(C, MakeCardTitle(LOCTEXT("YourGfc", "Your GFC")), 8.f);
		BalanceText = MakeText(T, FText::GetEmpty(), 26, Th.Gold, true);
		AddV(C, BalanceText, 2.f);
		BalanceNote = MakeText(T, FText::GetEmpty(), 13, Th.TextSecondary);
		AddV(C, BalanceNote, 12.f);
		WalletButton = MakeButton(T, LOCTEXT("OpenWallet", "Open wallet"), Th.Border);
		if (UButtonSlot* S = Cast<UButtonSlot>(WalletButton->GetChildAt(0)->Slot)) S->SetPadding(FMargin(16.f, 7.f));
		WalletButton->OnClicked.AddDynamic(this, &ThisClass::HandleOpenWallet);
		AddV(C, WalletButton, 4.f);
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(WalletButton->Slot)) S->SetHorizontalAlignment(HAlign_Left);
		Card->AddChild(C);
		AddV(Col, Card, 16.f);
		WalletCard = Card;
	}

	// Trending: the feed's most liked and best earning posts.
	{
		UBorder* Card = MakeCard(T, FMargin(0.f, 12.f, 0.f, 4.f));
		UVerticalBox* C = MakeColumn(T);
		UWidget* Title = MakeCardTitle(LOCTEXT("Trending", "Trending on GFC"));
		AddV(C, Title, 4.f);
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Title->Slot)) S->SetPadding(FMargin(16.f, 0.f, 16.f, 4.f));
		TrendingList = MakeColumn(T);
		AddV(C, TrendingList, 0.f);
		Card->AddChild(C);
		AddV(Col, Card, 16.f);
		TrendingCard = Card;
		TrendingCard->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Top earners: the network's best paid wallets for views (GET /podt/stats).
	{
		UBorder* Card = MakeCard(T, FMargin(16.f, 12.f));
		UVerticalBox* C = MakeColumn(T);
		AddV(C, MakeCardTitle(LOCTEXT("TopEarners", "Top earners")), 2.f);
		EarnersNote = MakeText(T, FText::GetEmpty(), 13, Th.TextSecondary);
		AddV(C, EarnersNote, 10.f);
		EarnersList = MakeColumn(T);
		AddV(C, EarnersList, 0.f);
		Card->AddChild(C);
		AddV(Col, Card, 16.f);
	}

	NodeText = MakeText(T, FText::GetEmpty(), 12, Th.TextSecondary);
	AddV(Col, NodeText, 16.f);
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(NodeText->Slot)) S->SetPadding(FMargin(16.f, 0.f, 16.f, 16.f));
}

void UGFCRightRail::Refresh()
{
	const UGFCApiClient* C = ClientOf(this);
	if (NodeText)
	{
		NodeText->SetText(FText::Format(LOCTEXT("Node", "Node: {0}\nGFC pays creators when people view their posts."), FText::FromString(C->GetBaseUrl())));
	}
	RefreshBalance();
	RefreshEarners();
}

void UGFCRightRail::RefreshBalance()
{
	UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>();
	UGFCApiClient* C = Api->GetClient();
	const uint32 Serial = ++BalanceSerial;
	if (!C->IsLoggedIn())
	{
		if (WalletCard) WalletCard->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (WalletCard) WalletCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const TArray<FGFCKnownWallet> Wallets = Api->GetKnownWallets();
	if (Wallets.Num() == 0)
	{
		BalanceText->SetText(Gfc(0.0));
		BalanceNote->SetText(LOCTEXT("NoWallet", "No wallet on this device yet. Create one to earn from your posts."));
		return;
	}
	BalanceNote->SetText(LOCTEXT("Loading", "Loading…"));
	struct FSum { int64 Cents = 0; int32 Left = 0; int32 Failed = 0; };
	TSharedRef<FSum> Sum = MakeShared<FSum>();
	Sum->Left = Wallets.Num();
	TWeakObjectPtr<UGFCRightRail> WeakThis(this);
	const int32 Count = Wallets.Num();
	for (const FGFCKnownWallet& W : Wallets)
	{
		C->GetWallet(W.WalletId, [WeakThis, Sum, Serial, Count](const FGFCApiResult& R, const FGFCWallet& Wallet)
		{
			if (R.bOk) Sum->Cents += Wallet.BalanceCents;
			else ++Sum->Failed;
			if (--Sum->Left > 0 || !WeakThis.IsValid() || WeakThis->BalanceSerial != Serial) return;
			WeakThis->BalanceText->SetText(Gfc(double(Sum->Cents) / 100.0));
			WeakThis->BalanceNote->SetText(Sum->Failed > 0
				? LOCTEXT("SomeFailed", "Some wallets couldn't be reached.")
				: FText::Format(LOCTEXT("Across", "Across {0} {0}|plural(one=wallet,other=wallets)"), FText::AsNumber(Count)));
		});
	}
}

void UGFCRightRail::RefreshEarners()
{
	TWeakObjectPtr<UGFCRightRail> WeakThis(this);
	ClientOf(this)->GetNetworkStats([WeakThis](const FGFCApiResult& R, const FGFCNetworkStats& Stats)
	{
		if (!WeakThis.IsValid() || !WeakThis->EarnersList) return;
		UWidgetTree* T = WeakThis->WidgetTree;
		const FGFCTheme& Th = FGFCTheme::Get();
		WeakThis->EarnersList->ClearChildren();
		if (!R.bOk)
		{
			WeakThis->EarnersNote->SetText(LOCTEXT("NoStats", "Earnings aren't available from this node."));
			return;
		}
		WeakThis->EarnersNote->SetText(FText::Format(LOCTEXT("Paid", "{0} paid to creators for {1} views"),
			Gfc(Stats.TotalRewardGfc), FText::AsNumber(Stats.TotalTransfers)));
		int32 Rank = 0;
		for (const FGFCTopEarner& E : Stats.TopEarners)
		{
			if (++Rank > 5) break;
			UHorizontalBox* Row = MakeRow(T);
			UTextBlock* N = MakeText(T, FText::AsNumber(Rank), 15, Th.TextSecondary, true);
			N->SetAutoWrapText(false);
			USizeBox* NBox = T->ConstructWidget<USizeBox>();
			NBox->SetWidthOverride(22.f);
			NBox->AddChild(N);
			AddH(Row, NBox, 6.f);
			UVerticalBox* Who = MakeColumn(T);
			UTextBlock* Wallet = MakeText(T, FText::FromString(EarnerName(E)), 15, Th.Text, true);
			Wallet->SetAutoWrapText(false);
			Wallet->SetToolTipText(FText::FromString(E.Wallet));
			AddV(Who, Wallet, 0.f);
			AddV(Who, MakeText(T, FText::Format(LOCTEXT("Views", "{0} paid views"), FText::AsNumber(E.Transfers)), 13, Th.TextSecondary), 0.f);
			AddH(Row, Who, 8.f, true);
			UTextBlock* Amount = MakeText(T, FText::Format(LOCTEXT("Plus", "+{0}"), Gfc(E.EarnedGfc)), 14, Th.Gold, true);
			Amount->SetAutoWrapText(false);
			AddH(Row, Amount, 0.f);
			AddV(WeakThis->EarnersList, Row, 12.f);
		}
		if (Rank == 0)
		{
			AddV(WeakThis->EarnersList, MakeText(T, LOCTEXT("NoEarners", "No paid views yet."), 14, Th.TextSecondary), 4.f);
		}
	});
}

FString UGFCRightRail::EarnerName(const FGFCTopEarner& Earner)
{
	return Earner.Owner.IsEmpty() ? ShortId(Earner.Wallet) : TEXT("@") + Earner.Owner;
}

TArray<FGFCSeed> UGFCRightRail::RankTrending(const TArray<FGFCSeed>& Feed, int32 Max)
{
	TArray<FGFCSeed> Ranked = Feed;
	Ranked.StableSort([](const FGFCSeed& A, const FGFCSeed& B)
	{
		if (A.Likes != B.Likes) return A.Likes > B.Likes;
		if (A.GfcEarned != B.GfcEarned) return A.GfcEarned > B.GfcEarned;
		return A.Views > B.Views;
	});
	if (Ranked.Num() > Max) Ranked.SetNum(Max);
	return Ranked;
}

void UGFCRightRail::SetTrending(const TArray<FGFCSeed>& Feed)
{
	if (!TrendingList) return;
	UWidgetTree* T = WidgetTree;
	const FGFCTheme& Th = FGFCTheme::Get();
	Trending = RankTrending(Feed);
	TrendingList->ClearChildren();
	TrendingLinks.Reset();
	TrendingCard->SetVisibility(Trending.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	TWeakObjectPtr<UGFCRightRail> WeakThis(this);
	for (int32 i = 0; i < Trending.Num(); ++i)
	{
		const FGFCSeed& S = Trending[i];
		UButton* B = T->ConstructWidget<UButton>();
		B->SetStyle(GhostButtonStyle(Th.SurfacePressed, 0.f));
		UVerticalBox* Row = MakeColumn(T);
		AddV(Row, MakeText(T, FText::Format(LOCTEXT("TrendKind", "{0} · by @{1}"), GFCMediaKindLabel(S.GetMediaKind()), FText::FromString(S.Owner)),
			13, Th.TextSecondary), 2.f);
		const FString Title = UGFCSeedCard::FeedBody(S.Title.IsEmpty() ? S.Description : S.Title, 70);
		AddV(Row, MakeText(T, FText::FromString(Title), 15, Th.Text, true), 2.f);
		FText Stats = FText::Format(LOCTEXT("TrendStats", "{0} {0}|plural(one=like,other=likes) · {1} {1}|plural(one=view,other=views)"),
			FText::AsNumber(S.Likes), FText::AsNumber(S.Views));
		if (S.GfcEarned > 0.0)
		{
			Stats = FText::Format(LOCTEXT("TrendEarned", "{0} · {1} earned"), Stats, Gfc(S.GfcEarned));
		}
		AddV(Row, MakeText(T, Stats, 13, Th.TextSecondary), 0.f);
		B->AddChild(Row);
		if (UButtonSlot* BS = Cast<UButtonSlot>(Row->Slot))
		{
			BS->SetPadding(FMargin(16.f, 10.f));
			BS->SetHorizontalAlignment(HAlign_Fill);
		}
		UGFCRailLink* Link = NewObject<UGFCRailLink>(this);
		Link->Index = i;
		Link->OnClick = [WeakThis](int32 Index)
		{
			if (WeakThis.IsValid() && WeakThis->Root.IsValid() && WeakThis->Trending.IsValidIndex(Index))
			{
				WeakThis->Root->ShowSeed(WeakThis->Trending[Index]);
			}
		};
		TrendingLinks.Add(Link);
		B->OnClicked.AddDynamic(Link, &UGFCRailLink::Clicked);
		AddV(TrendingList, B, 0.f);
	}
}

void UGFCRightRail::HandleSearch(const FText& InText, ETextCommit::Type Commit)
{
	if (Commit == ETextCommit::OnEnter && Root.IsValid())
	{
		Root->ShowExplore(InText.ToString());
		SearchBox->SetText(FText::GetEmpty());
	}
}

void UGFCRightRail::HandleOpenWallet()
{
	if (Root.IsValid()) Root->ShowWallet();
}

#undef LOCTEXT_NAMESPACE
