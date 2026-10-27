#include "UI/GFCRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/SafeZone.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "GFCDeepLink.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/GFCCreateScreen.h"
#include "UI/GFCFeedScreen.h"
#include "UI/GFCLoginScreen.h"
#include "UI/GFCSeedDetailScreen.h"
#include "GFCApi.h"
#include "UI/GFCActionSheet.h"
#include "UI/GFCMusicWidgets.h"
#include "UI/GFCRewardBurst.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformProcess.h"
#include "UObject/UObjectIterator.h"
#include "UI/GFCUIStyle.h"
#include "UI/GFCWalletScreen.h"

#define LOCTEXT_NAMESPACE "GFCRoot"

/** Sees the Android Back key before anything else, wherever focus is, and turns it into navigation. */
class FGFCBackKey : public IInputProcessor
{
public:
	TWeakObjectPtr<UGFCRootWidget> Root;
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		if (Event.GetKey() == EKeys::Android_Back && Root.IsValid())
		{
			if (!UGFCActionSheet::CloseOpen()) // an open sheet closes first
			{
				Root->HandleBack();
			}
			return true; // always handled: unhandled, the engine dims the app
		}
		return false;
	}
	virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		return Event.GetKey() == EKeys::Android_Back;
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("GFCBackKey"); }
};

using namespace GFCUI;

UGFCRootWidget::UGFCRootWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LoginScreenClass = UGFCLoginScreen::StaticClass();
	FeedScreenClass = UGFCFeedScreen::StaticClass();
	WalletScreenClass = UGFCWalletScreen::StaticClass();
	SeedDetailScreenClass = UGFCSeedDetailScreen::StaticClass();
	CreateScreenClass = UGFCCreateScreen::StaticClass();
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
	CreateScreen = AddScreen(CreateWidget<UGFCCreateScreen>(this, CreateScreenClass));

	if (FeedNavButton) FeedNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowFeed);
	if (WalletNavButton) WalletNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowWallet);
	if (CreateNavButton) CreateNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowCreate);
	if (LogoutButton) LogoutButton->OnClicked.AddDynamic(this, &ThisClass::HandleLogout);
	if (LoginNavButton) LoginNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowLogin);

	UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>();
	Api->OnSessionChanged.AddDynamic(this, &ThisClass::HandleSessionChanged);
	DeepLinkHandle = FGFCDeepLink::OnSeedLink.AddUObject(this, &UGFCRootWidget::HandleDeepLink);
	VerifiedLinkHandle = FGFCDeepLink::OnVerifiedLink.AddUObject(this, &UGFCRootWidget::OnBackFromVerification);
	// Back from the browser without tapping "Back to GFC" counts too.
	ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddWeakLambda(this, [this]()
	{
		if (bVerifyingPhone) OnBackFromVerification();
	});
	if (FSlateApplication::IsInitialized())
	{
		BackKey = MakeShared<FGFCBackKey>();
		BackKey->Root = this;
		FSlateApplication::Get().RegisterInputPreProcessor(BackKey);
	}

	// A link that launched the app wins; then a remembered session goes to the feed.
	FGFCDeepLink::CollectLaunchLink();
	FString LinkedSeed;
	if (FGFCDeepLink::TakePending(LinkedSeed))
	{
		OpenSeedById(LinkedSeed);
	}
	else if (!Api->GetClient()->IsLoggedIn())
	{
		ShowLogin();
	}
	else if (!OpenStartScreenFromCommandLine())
	{
		ShowFeed();
	}
	if (Api->GetClient()->IsLoggedIn())
	{
		AnnounceEarnings(); // a remembered session is a login too
		Api->GetClient()->RefreshBlocks();
	}
	ShowStatus(FText::Format(LOCTEXT("Connected", "Node: {0}"), FText::FromString(Api->GetClient()->GetBaseUrl())));
}

void UGFCRootWidget::NativeDestruct()
{
	FGFCDeepLink::OnSeedLink.Remove(DeepLinkHandle);
	FGFCDeepLink::OnVerifiedLink.Remove(VerifiedLinkHandle);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundHandle);
	if (BackKey.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(BackKey);
	}
	BackKey.Reset();
	Super::NativeDestruct();
}

void UGFCRootWidget::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;

	UBorder* Bg = MakePanel(T, Background, FMargin(0.f));
	T->RootWidget = Bg;
	USafeZone* Safe = T->ConstructWidget<USafeZone>(); // notches, status bar, gesture bar
	Bg->AddChild(Safe);
	UVerticalBox* Col = MakeColumn(T);
	Safe->AddChild(Col);

	// Top bar: brand on the left, account on the right.
	UBorder* Bar = MakePanel(T, Background, FMargin(16.f, 6.f));
	UHorizontalBox* BarRow = MakeRow(T);
	Bar->AddChild(BarRow);
	AddH(BarRow, MakeText(T, LOCTEXT("Brand", "GFC"), 22, AccentText, true), 12.f);
	UserText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	UserText->SetAutoWrapText(false);
	UserText->SetClipping(EWidgetClipping::ClipToBounds); // long names end at the Log out button
	AddH(BarRow, UserText, 8.f, true);
	LogoutButton = MakeButton(T, LOCTEXT("Logout", "Log out"), PanelRaised);
	LoginNavButton = MakeButton(T, LOCTEXT("Login", "Log in"));
	AddH(BarRow, LogoutButton, 0.f);
	AddH(BarRow, LoginNavButton, 0.f);
	AddV(Col, Bar, 0.f);
	AddV(Col, MakeDivider(T), 0.f);

	UBorder* StatusBar = MakePanel(T, Background, FMargin(16.f, 6.f));
	StatusText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	StatusBar->AddChild(StatusText);
	AddV(Col, StatusBar, 0.f);

	UBorder* Body = MakePanel(T, Background, FMargin(12.f, 0.f, 12.f, 8.f));
	ScreenSwitcher = T->ConstructWidget<UWidgetSwitcher>();
	Body->AddChild(ScreenSwitcher);
	AddV(Col, Body, 0.f, true);

	// Music keeps playing across screens: the mini player sits just above the bottom bar.
	MiniPlayer = CreateWidget<UGFCMiniPlayer>(this, UGFCMiniPlayer::StaticClass());
	TWeakObjectPtr<UGFCRootWidget> WeakRoot(this);
	MiniPlayer->OnOpen = [WeakRoot](const FGFCSeed& Playing) { if (WeakRoot.IsValid()) WeakRoot->ShowSeed(Playing); };
	AddV(Col, MiniPlayer, 0.f);

	// Bottom navigation: one-handed reach.
	AddV(Col, MakeDivider(T), 0.f);
	UBorder* NavBg = MakePanel(T, Background, FMargin(12.f, 8.f));
	UHorizontalBox* Nav = MakeRow(T);
	NavBg->AddChild(Nav);
	FeedNavButton = MakeButton(T, LOCTEXT("Feed", "Feed"), PanelRaised);
	CreateNavButton = MakeButton(T, LOCTEXT("Create", "+ Post"));
	WalletNavButton = MakeButton(T, LOCTEXT("Wallet", "Wallet"), PanelRaised);
	AddH(Nav, FeedNavButton, 8.f, true);
	AddH(Nav, CreateNavButton, 8.f, true);
	AddH(Nav, WalletNavButton, 0.f, true);
	NavBar = NavBg;
	AddV(Col, NavBg, 0.f);
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
	if (Screen.Equals(TEXT("create"), ESearchCase::IgnoreCase))
	{
		ShowCreate();
		// -GFCPost=<file>: preselect a file to post (testing on PC without the file dialog).
		FString PostFile;
		if (FParse::Value(FCommandLine::Get(), TEXT("GFCPost="), PostFile))
		{
			CreateScreen->UseFile(PostFile);
		}
		return true;
	}
	FString SeedId;
	if (Screen.Split(TEXT(":"), nullptr, &SeedId) && FGFCDeepLink::IsValidSeedId(SeedId))
	{
		OpenSeedById(SeedId);
		return true;
	}
	return false;
}

void UGFCRootWidget::Activate(UGFCScreenBase* Screen)
{
	if (!Screen)
	{
		return;
	}
	if (ActiveScreen && ActiveScreen != Screen)
	{
		ActiveScreen->OnScreenHidden(); // frees heavy content (models, media)
	}
	ActiveScreen = Screen;
	ScreenSwitcher->SetActiveWidget(Screen);
	RefreshNav();
	Screen->OnScreenShown();
}

void UGFCRootWidget::RefreshNav()
{
	const UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	const bool bIn = C->IsLoggedIn();
	const bool bOnLogin = ActiveScreen == LoginScreen;
	if (NavBar) NavBar->SetVisibility(bIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (LogoutButton) LogoutButton->SetVisibility(bIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (LoginNavButton) LoginNavButton->SetVisibility(!bIn && !bOnLogin ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (UserText)
	{
		UserText->SetText(bIn ? FText::FromString(C->GetUsername()) : FText::GetEmpty());
	}
}

void UGFCRootWidget::ShowLogin()
{
	// From a shared seed: come back to it after logging in.
	bReturnToSeed = ActiveScreen == SeedDetailScreen && SeedDetailScreen && !SeedDetailScreen->GetSeed().Id.IsEmpty();
	Activate(LoginScreen);
}

void UGFCRootWidget::ShowFeed() { Activate(FeedScreen); }
void UGFCRootWidget::ShowWallet() { Activate(WalletScreen); }
void UGFCRootWidget::ShowCreate() { Activate(CreateScreen); }

void UGFCRootWidget::ShowSeed(const FGFCSeed& Seed)
{
	if (SeedDetailScreen)
	{
		if (ActiveScreen == SeedDetailScreen)
		{
			SeedDetailScreen->OnScreenHidden(); // release the previous seed first
		}
		SeedDetailScreen->SetSeed(Seed);
		Activate(SeedDetailScreen);
	}
}

void UGFCRootWidget::OpenSeedById(const FString& SeedId)
{
	ShowStatus(LOCTEXT("Opening", "Opening the shared seed…"));
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient()->GetSeed(SeedId,
		[WeakThis](const FGFCApiResult& R, const FGFCSeed& Seed)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (R.bOk)
			{
				WeakThis->ShowStatus(FText::GetEmpty());
				WeakThis->ShowSeed(Seed);
				return;
			}
			WeakThis->ShowStatus(R.StatusCode == 404
				? LOCTEXT("NoSeed", "That seed isn't on this node (yet). It may still be syncing.")
				: FText::FromString(R.Error), true);
			if (!WeakThis->ActiveScreen)
			{
				WeakThis->GoBack();
			}
		});
}

void UGFCRootWidget::GoBack()
{
	if (GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient()->IsLoggedIn())
	{
		ShowFeed();
	}
	else
	{
		ShowLogin();
	}
}

bool UGFCRootWidget::HandleBack()
{
	if (ActiveScreen == FeedScreen || ActiveScreen == LoginScreen)
	{
		return false;
	}
	GoBack();
	return true;
}

void UGFCRootWidget::HandleDeepLink(const FString& SeedId)
{
	OpenSeedById(SeedId);
}

void UGFCRootWidget::ShowStatus(const FText& Message, bool bError)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(FSlateColor(bError ? Danger : TextDim));
	}
}

void UGFCRootWidget::AnnounceEarnings()
{
	UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>();
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	auto Check = [WeakThis, Api]()
	{
		Api->CheckEarnings([WeakThis](int64 EarnedCents)
		{
			if (WeakThis.IsValid() && EarnedCents > 0)
			{
				UE_LOG(LogGFCApi, Log, TEXT("Earned since last login: %lld cents"), EarnedCents);
				UGFCRewardBurst::Play(WeakThis.Get(), double(EarnedCents) / 100.0, LOCTEXT("Earned", "You earned GFC while you were away!"));
			}
		});
	};
	// Learn the account's wallets first, so a new device shows the balance (their first look isn't "earned").
	Api->GetClient()->GetVerifyStatus([WeakThis, Api, Check](const FGFCApiResult& R, const FGFCVerifyStatus& S)
	{
		if (!WeakThis.IsValid()) return;
		if (R.bOk && Api->LearnWallets(S.Wallets) > 0 && WeakThis->WalletScreen)
		{
			WeakThis->WalletScreen->OnWalletsLearned();
		}
		Check();
	});
}

UGFCRootWidget* UGFCRootWidget::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	for (TObjectIterator<UGFCRootWidget> It; It; ++It)
	{
		if (It->GetWorld() == World && It->IsInViewport())
		{
			return *It;
		}
	}
	return nullptr;
}

void UGFCRootWidget::OnPostHidden(const FString& SeedId)
{
	if (FeedScreen) FeedScreen->RemoveHidden();
	if (ActiveScreen == SeedDetailScreen && SeedDetailScreen && SeedDetailScreen->GetSeed().Id == SeedId)
	{
		ShowFeed();
	}
}

void UGFCRootWidget::StartPhoneVerification()
{
	UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	ShowStatus(LOCTEXT("OpeningVerify", "Opening phone verification…"));
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	C->CreateVerifyLink([WeakThis](const FGFCApiResult& R, const FGFCVerifyLink& Link)
	{
		if (!WeakThis.IsValid()) return;
		if (!R.bOk)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		if (Link.bVerified)
		{
			WeakThis->ShowStatus(LOCTEXT("AlreadyVerified", "Your phone is already verified."));
			WeakThis->OnBackFromVerification();
			return;
		}
		WeakThis->bVerifyingPhone = true;
		FString Error;
		FPlatformProcess::LaunchURL(*Link.Url, nullptr, &Error);
		if (!Error.IsEmpty())
		{
			WeakThis->bVerifyingPhone = false;
			WeakThis->ShowStatus(LOCTEXT("NoBrowser", "Couldn't open the browser."), true);
		}
	});
}

void UGFCRootWidget::OnBackFromVerification()
{
	UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>();
	UGFCApiClient* C = Api->GetClient();
	// Celebrate only the change to verified: we sent the user to verify, or last saw them unverified.
	const TOptional<bool> Before = C->GetLastKnownVerified();
	const bool bWasUnverified = bVerifyingPhone || (Before.IsSet() && !Before.GetValue());
	bVerifyingPhone = false;
	if (!C->IsLoggedIn()) return;
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	C->GetVerifyStatus([WeakThis, Api, bWasUnverified](const FGFCApiResult& R, const FGFCVerifyStatus& S)
	{
		if (!WeakThis.IsValid() || !R.bOk) return;
		if (Api->LearnWallets(S.Wallets) > 0 && WeakThis->WalletScreen) WeakThis->WalletScreen->OnWalletsLearned();
		if (WeakThis->WalletScreen) WeakThis->WalletScreen->SetPhoneVerified(S.bVerified);
		if (!S.bVerified || !bWasUnverified)
		{
			return;
		}
		WeakThis->ShowStatus(LOCTEXT("Verified", "Phone verified. Rewards are on."));
		if (S.Rewards() > 0.0)
		{
			// The welcome grant and any first post bonus, together: ka-ching.
			UGFCRewardBurst::Play(WeakThis.Get(), S.Rewards(), S.FirstPostBonus > 0.0
				? LOCTEXT("VerifiedBoth", "You're verified! Welcome grant + first post bonus")
				: LOCTEXT("VerifiedGrant", "You're verified! Welcome grant"));
		}
		Api->CheckEarnings([](int64) {}); // counted as seen, so the next login doesn't celebrate it again
	});
}

void UGFCRootWidget::OnLoggedIn(bool bNewAccount)
{
	AnnounceEarnings();
	GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient()->RefreshBlocks();
	if (bReturnToSeed)
	{
		bReturnToSeed = false;
		ShowSeed(SeedDetailScreen->GetSeed());
		return;
	}
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
			WeakThis->ShowStatus(LOCTEXT("LoggedOut", "Logged out. Downloaded content was deleted."));
		}
	});
}

void UGFCRootWidget::HandleSessionChanged()
{
	const bool bIn = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient()->IsLoggedIn();
	if (!bIn && ActiveScreen != LoginScreen && ActiveScreen != SeedDetailScreen)
	{
		ShowLogin();
	}
	RefreshNav();
}

#undef LOCTEXT_NAMESPACE
