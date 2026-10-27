#include "UI/GFCRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/GFCFeedScreen.h"
#include "UI/GFCLoginScreen.h"
#include "UI/GFCSeedDetailScreen.h"
#include "UI/GFCUIStyle.h"
#include "UI/GFCWalletScreen.h"

#define LOCTEXT_NAMESPACE "GFCRoot"

using namespace GFCUI;

UGFCRootWidget::UGFCRootWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LoginScreenClass = UGFCLoginScreen::StaticClass();
	FeedScreenClass = UGFCFeedScreen::StaticClass();
	WalletScreenClass = UGFCWalletScreen::StaticClass();
	SeedDetailScreenClass = UGFCSeedDetailScreen::StaticClass();
}

void UGFCRootWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	auto AddScreen = [this](auto* Screen)
	{
		Screen->SetRoot(this);
		if (UWidgetSwitcherSlot* S = Cast<UWidgetSwitcherSlot>(ScreenSwitcher->AddChild(Screen)))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
		return Screen;
	};
	// Screens are created with this widget's owning player, so they're initialised (and build their layout) now.
	LoginScreen = AddScreen(CreateWidget<UGFCLoginScreen>(this, LoginScreenClass));
	FeedScreen = AddScreen(CreateWidget<UGFCFeedScreen>(this, FeedScreenClass));
	WalletScreen = AddScreen(CreateWidget<UGFCWalletScreen>(this, WalletScreenClass));
	SeedDetailScreen = AddScreen(CreateWidget<UGFCSeedDetailScreen>(this, SeedDetailScreenClass));

	if (FeedNavButton) FeedNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowFeed);
	if (WalletNavButton) WalletNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowWallet);
	if (LogoutButton) LogoutButton->OnClicked.AddDynamic(this, &ThisClass::HandleLogout);

	UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>();
	Api->OnSessionChanged.AddDynamic(this, &ThisClass::HandleSessionChanged);

	// A remembered session goes straight to the feed; an expired one gets bounced back on the first 401.
	if (!Api->GetClient()->IsLoggedIn())
	{
		ShowLogin();
	}
	else if (!OpenStartScreenFromCommandLine())
	{
		ShowFeed();
	}
	ShowStatus(FText::Format(LOCTEXT("Connected", "Node: {0}"), FText::FromString(Api->GetClient()->GetBaseUrl())));
}

void UGFCRootWidget::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;

	UBorder* Bg = MakePanel(T, Background, FMargin(0.f));
	T->RootWidget = Bg;
	UVerticalBox* Col = MakeColumn(T);
	Bg->AddChild(Col);

	// Top bar
	UBorder* Bar = MakePanel(T, Panel, FMargin(20.f, 10.f));
	UHorizontalBox* BarRow = MakeRow(T);
	Bar->AddChild(BarRow);
	AddH(BarRow, MakeText(T, LOCTEXT("Brand", "GFC"), 22, Accent, true), 24.f);

	UHorizontalBox* Nav = MakeRow(T);
	FeedNavButton = MakeButton(T, LOCTEXT("Feed", "Feed"), PanelRaised);
	WalletNavButton = MakeButton(T, LOCTEXT("Wallet", "Wallet"), PanelRaised);
	AddH(Nav, FeedNavButton);
	AddH(Nav, WalletNavButton);
	NavBar = Nav;
	AddH(BarRow, Nav, 0.f);
	AddH(BarRow, T->ConstructWidget<USpacer>(), 0.f, true);

	UserText = MakeText(T, FText::GetEmpty(), 13, TextDim);
	UserText->SetAutoWrapText(false);
	AddH(BarRow, UserText, 12.f);
	LogoutButton = MakeButton(T, LOCTEXT("Logout", "Log out"), PanelRaised);
	AddH(BarRow, LogoutButton, 0.f);
	AddV(Col, Bar, 0.f);

	// Status line
	UBorder* StatusBar = MakePanel(T, Background, FMargin(20.f, 6.f));
	StatusText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	StatusBar->AddChild(StatusText);
	AddV(Col, StatusBar, 0.f);

	// Screens
	UBorder* Body = MakePanel(T, Background, FMargin(20.f, 8.f, 20.f, 20.f));
	ScreenSwitcher = T->ConstructWidget<UWidgetSwitcher>();
	Body->AddChild(ScreenSwitcher);
	AddV(Col, Body, 0.f, true);
}

void UGFCRootWidget::Activate(UGFCScreenBase* Screen)
{
	if (!Screen)
	{
		return;
	}
	ScreenSwitcher->SetActiveWidget(Screen);
	RefreshNav();
	Screen->OnScreenShown();
}

void UGFCRootWidget::RefreshNav()
{
	const UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	const bool bIn = C->IsLoggedIn();
	const ESlateVisibility Vis = bIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (NavBar) NavBar->SetVisibility(Vis);
	if (LogoutButton) LogoutButton->SetVisibility(Vis);
	if (UserText)
	{
		UserText->SetVisibility(Vis);
		UserText->SetText(FText::Format(LOCTEXT("SignedInAs", "Signed in as {0}"), FText::FromString(C->GetUsername())));
	}
}

bool UGFCRootWidget::OpenStartScreenFromCommandLine()
{
	// Development convenience: -GFCScreen=wallet | feed | seed:<id> opens that screen first.
	FString Screen;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GFCScreen="), Screen))
	{
		return false;
	}
	if (Screen.Equals(TEXT("wallet"), ESearchCase::IgnoreCase))
	{
		ShowWallet();
		return true;
	}
	FString SeedId;
	if (Screen.Split(TEXT(":"), nullptr, &SeedId) && !SeedId.IsEmpty())
	{
		TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
		GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient()->GetSeed(SeedId,
			[WeakThis](const FGFCApiResult& R, const FGFCSeed& Seed)
			{
				if (WeakThis.IsValid() && R.bOk)
				{
					WeakThis->ShowSeed(Seed);
				}
			});
		ShowFeed();
		return true;
	}
	return false;
}

void UGFCRootWidget::ShowLogin() { Activate(LoginScreen); }
void UGFCRootWidget::ShowFeed() { Activate(FeedScreen); }
void UGFCRootWidget::ShowWallet() { Activate(WalletScreen); }

void UGFCRootWidget::ShowSeed(const FGFCSeed& Seed)
{
	if (SeedDetailScreen)
	{
		SeedDetailScreen->SetSeed(Seed);
		Activate(SeedDetailScreen);
	}
}

void UGFCRootWidget::ShowStatus(const FText& Message, bool bError)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(FSlateColor(bError ? Danger : TextDim));
	}
}

void UGFCRootWidget::OnLoggedIn(bool bNewAccount)
{
	if (bNewAccount)
	{
		ShowWallet();
	}
	else
	{
		ShowFeed();
	}
}

void UGFCRootWidget::HandleLogout()
{
	ShowStatus(LOCTEXT("LoggingOut", "Logging out…"));
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient()->Logout([WeakThis](const FGFCApiResult&)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ShowStatus(LOCTEXT("LoggedOut", "Logged out."));
		}
	});
}

void UGFCRootWidget::HandleSessionChanged()
{
	const bool bIn = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient()->IsLoggedIn();
	if (!bIn && ScreenSwitcher->GetActiveWidget() != LoginScreen)
	{
		ShowLogin();
	}
	RefreshNav();
}

#undef LOCTEXT_NAMESPACE
