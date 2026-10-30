#include "UI/GFCRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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
#include "UI/GFCDesktopRails.h"
#include "UI/GFCFeedScreen.h"
#include "UI/GFCLoginScreen.h"
#include "UI/GFCSeedDetailScreen.h"
#include "GFCApi.h"
#include "UI/GFCActionSheet.h"
#include "UI/GFCPostEditor.h"
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
			if (!UGFCActionSheet::CloseOpen() && !UGFCPostPanel::CloseOpen()) // an open sheet or panel closes first
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

	if (FeedNavButton) FeedNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowHome); // everyone's posts, even from someone's page
	if (BrandButton) BrandButton->OnClicked.AddDynamic(this, &ThisClass::ShowHome);
	if (WalletNavButton) WalletNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowWallet);
	if (CreateNavButton) CreateNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowCreate);
	if (LogoutButton) LogoutButton->OnClicked.AddDynamic(this, &ThisClass::HandleLogout);
	if (LoginNavButton) LoginNavButton->OnClicked.AddDynamic(this, &ThisClass::ShowLogin);
	if (LeftRail) LeftRail->SetRoot(this);
	if (RightRail) RightRail->SetRoot(this);
	TWeakObjectPtr<UGFCRootWidget> WeakRail(this);
	FeedScreen->OnForYouLoaded = [WeakRail](const TArray<FGFCSeed>& Seeds)
	{
		if (WeakRail.IsValid() && WeakRail->RightRail) WeakRail->RightRail->SetTrending(Seeds);
	};
	ApplyLayout(ELayout::Phone); // the first tick switches to the desktop layout if the window is wide

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
	UOverlay* Layers = T->ConstructWidget<UOverlay>();
	Safe->AddChild(Layers);

	// Shell: [left rail | line | centre column | line | right rail]. The phone layout is the
	// centre column alone, filling the width; the desktop layout centres all five.
	Shell = MakeRow(T);
	if (UOverlaySlot* S = Layers->AddChildToOverlay(Shell))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	auto VLine = [T]()
	{
		USizeBox* Box = T->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(1.f);
		Box->AddChild(MakePanel(T, BorderColor, FMargin(0.f)));
		return Box;
	};
	auto AddColumn = [this](UWidget* W, const FMargin& Pad)
	{
		UHorizontalBoxSlot* S = Shell->AddChildToHorizontalBox(W);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(Pad);
		return S;
	};

	LeftRail = CreateWidget<UGFCLeftRail>(this, UGFCLeftRail::StaticClass());
	USizeBox* LeftBox = T->ConstructWidget<USizeBox>();
	LeftBox->SetWidthOverride(GFCUI::Desktop::LeftRailWidth);
	LeftBox->AddChild(LeftRail);
	AddColumn(LeftBox, FMargin(0.f, 0.f, 0.f, 0.f));
	UWidget* LeftLine = VLine();
	AddColumn(LeftLine, FMargin(0.f));

	CenterBox = T->ConstructWidget<USizeBox>();
	UVerticalBox* Col = MakeColumn(T);
	CenterBox->AddChild(Col);
	AddColumn(CenterBox, FMargin(0.f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UWidget* RightLine = VLine();
	AddColumn(RightLine, FMargin(0.f));
	RightRail = CreateWidget<UGFCRightRail>(this, UGFCRightRail::StaticClass());
	USizeBox* RightBox = T->ConstructWidget<USizeBox>();
	RightBox->SetWidthOverride(GFCUI::Desktop::RightRailWidth);
	RightBox->AddChild(RightRail);
	AddColumn(RightBox, FMargin(GFCUI::Desktop::Gutter, 0.f, 0.f, 0.f));
	DesktopChrome = {LeftBox, LeftLine, RightLine, RightBox};

	// Desktop status: a toast at the bottom of the window.
	UBorder* ToastBg = T->ConstructWidget<UBorder>();
	ToastBg->SetBrush(FSlateRoundedBoxBrush(FGFCTheme::Get().Accent, 8.f));
	ToastBg->SetPadding(FMargin(16.f, 12.f));
	ToastText = MakeText(T, FText::GetEmpty(), 15, FLinearColor::White);
	ToastBg->AddChild(ToastText);
	ToastBg->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* S = Layers->AddChildToOverlay(ToastBg))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(16.f, 0.f, 16.f, 32.f));
	}
	Toast = ToastBg;

	// Top bar: brand on the left, account on the right.
	UBorder* Bar = MakePanel(T, Background, FMargin(16.f, 6.f));
	UHorizontalBox* BarRow = MakeRow(T);
	Bar->AddChild(BarRow);
	// The GFC logo goes back to the full feed, like the Feed button.
	BrandButton = T->ConstructWidget<UButton>();
	BrandButton->SetStyle(FlatButtonStyle());
	BrandButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
	UTextBlock* Brand = MakeText(T, LOCTEXT("Brand", "GFC"), 22, AccentText, true);
	Brand->SetAutoWrapText(false);
	BrandButton->AddChild(Brand);
	if (UButtonSlot* S = Cast<UButtonSlot>(Brand->Slot)) S->SetPadding(FMargin(0.f));
	AddH(BarRow, BrandButton, 12.f);
	UserText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	UserText->SetAutoWrapText(false);
	UserText->SetClipping(EWidgetClipping::ClipToBounds); // long names end at the Log out button
	AddH(BarRow, UserText, 8.f, true);
	LogoutButton = MakeButton(T, LOCTEXT("Logout", "Log out"), PanelRaised);
	LoginNavButton = MakeButton(T, LOCTEXT("Login", "Log in"));
	AddH(BarRow, LogoutButton, 0.f);
	AddH(BarRow, LoginNavButton, 0.f);
	AddV(Col, Bar, 0.f);
	UWidget* BarLine = MakeDivider(T);
	AddV(Col, BarLine, 0.f);

	UBorder* StatusBar = MakePanel(T, Background, FMargin(16.f, 6.f));
	StatusText = MakeText(T, FText::GetEmpty(), 12, TextDim);
	StatusBar->AddChild(StatusText);
	AddV(Col, StatusBar, 0.f);

	Body = MakePanel(T, Background, FMargin(12.f, 0.f, 12.f, 8.f));
	ScreenSwitcher = T->ConstructWidget<UWidgetSwitcher>();
	Body->AddChild(ScreenSwitcher);
	AddV(Col, Body, 0.f, true);

	// Music keeps playing across screens: the mini player sits just above the bottom bar.
	MiniPlayer = CreateWidget<UGFCMiniPlayer>(this, UGFCMiniPlayer::StaticClass());
	TWeakObjectPtr<UGFCRootWidget> WeakRoot(this);
	MiniPlayer->OnOpen = [WeakRoot](const FGFCSeed& Playing) { if (WeakRoot.IsValid()) WeakRoot->ShowSeed(Playing); };
	AddV(Col, MiniPlayer, 0.f);

	// Bottom navigation: one-handed reach.
	UWidget* NavLine = MakeDivider(T);
	AddV(Col, NavLine, 0.f);
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
	PhoneChrome = {Bar, BarLine, StatusBar, NavLine};
}

void UGFCRootWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The layout follows the window, live: resizing across the breakpoint switches it.
	const float Width = MyGeometry.GetLocalSize().X;
	if (Width > 0.f)
	{
		const ELayout Wanted = Width < GFCUI::Desktop::Breakpoint ? ELayout::Phone
			: (Width < GFCUI::Desktop::RightRailFrom ? ELayout::Wide : ELayout::WideWithRightRail);
		if (Wanted != Layout)
		{
			ApplyLayout(Wanted);
		}
	}
	if (ToastSeconds > 0.f)
	{
		ToastSeconds -= InDeltaTime;
		if (ToastSeconds <= 0.f && Toast)
		{
			Toast->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGFCRootWidget::ApplyLayout(ELayout NewLayout)
{
	const bool bWasWide = IsWideLayout();
	Layout = NewLayout;
	const bool bWide = IsWideLayout();
	if (!Shell || !CenterBox)
	{
		return; // a Blueprint root without the desktop parts stays as it is
	}
	for (UWidget* W : PhoneChrome)
	{
		if (W) W->SetVisibility(bWide ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	for (UWidget* W : DesktopChrome)
	{
		if (W) W->SetVisibility(bWide ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	const bool bRightRail = Layout == ELayout::WideWithRightRail;
	if (DesktopChrome.Num() == 4)
	{
		DesktopChrome[3]->SetVisibility(bRightRail ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// Phone: the centre column fills the window. Desktop: fixed width, everything centred.
	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(CenterBox->Slot))
	{
		S->SetSize(FSlateChildSize(bWide ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill));
	}
	if (bWide) CenterBox->SetWidthOverride(GFCUI::Desktop::CenterWidth);
	else CenterBox->ClearWidthOverride();
	if (UOverlaySlot* S = Cast<UOverlaySlot>(Shell->Slot))
	{
		S->SetHorizontalAlignment(bWide ? HAlign_Center : HAlign_Fill);
	}
	if (FeedScreen)
	{
		FeedScreen->SetWideLayout(bWide);
		if (!bWide && FeedScreen->GetSource() != EGFCFeedSource::ForYou)
		{
			// The phone has only the feed: pages from the desktop rail go back to it.
			FeedScreen->SetSource(EGFCFeedSource::ForYou);
			if (ActiveScreen == FeedScreen) FeedScreen->Refresh();
		}
	}
	if (!bWide && Toast)
	{
		Toast->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshNav();
	if (bWide && !bWasWide && RightRail)
	{
		RightRail->Refresh();
	}
}

void UGFCRootWidget::UpdateDesktopNav()
{
	if (!LeftRail)
	{
		return;
	}
	EGFCNavItem Item = EGFCNavItem::None;
	if (ActiveScreen == WalletScreen)
	{
		Item = EGFCNavItem::Wallet;
	}
	else if (ActiveScreen == FeedScreen)
	{
		switch (FeedScreen->GetSource())
		{
		case EGFCFeedSource::Explore: Item = EGFCNavItem::Explore; break;
		case EGFCFeedSource::Bookmarks: Item = EGFCNavItem::Bookmarks; break;
		case EGFCFeedSource::Mine: Item = EGFCNavItem::Profile; break;
		case EGFCFeedSource::User: Item = EGFCNavItem::None; break;
		default: Item = EGFCNavItem::Home; break;
		}
	}
	LeftRail->SetActive(Item);
	// The feed runs edge to edge between the column lines; other screens get a margin.
	if (Body)
	{
		Body->SetPadding(!IsWideLayout() ? FMargin(12.f, 0.f, 12.f, 8.f)
			: (ActiveScreen == FeedScreen ? FMargin(0.f) : FMargin(16.f, 12.f, 16.f, 8.f)));
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
	if (Screen.Equals(TEXT("create"), ESearchCase::IgnoreCase))
	{
		ShowCreate();
		// -GFCPost=<file>: preselect a file to post (testing on PC without the file dialog).
		FString PostFile;
		if (FParse::Value(FCommandLine::Get(), TEXT("GFCPost="), PostFile))
		{
			CreateScreen->UseFile(PostFile);
		}
		// -GFCPostExclusive ticks "Exclusive to GFC Network"; -GFCPostNow posts right away;
		// -GFCShareTeaser then presses "Share teaser on X".
		if (FParse::Param(FCommandLine::Get(), TEXT("GFCPostExclusive")))
		{
			CreateScreen->SetExclusive(true);
		}
		CreateScreen->bShareAfterPost = FParse::Param(FCommandLine::Get(), TEXT("GFCShareTeaser"));
		if (FParse::Param(FCommandLine::Get(), TEXT("GFCPostNow")))
		{
			CreateScreen->Post();
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
	if (NavBar) NavBar->SetVisibility(bIn && !IsWideLayout() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (LeftRail) LeftRail->Refresh();
	UpdateDesktopNav();
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

void UGFCRootWidget::ShowFeedPage(uint8 Source, const FString& Query)
{
	if (FeedScreen)
	{
		FeedScreen->SetSource(EGFCFeedSource(Source), Query);
		Activate(FeedScreen);
	}
}

void UGFCRootWidget::ShowHome()
{
	ShowFeedPage(uint8(EGFCFeedSource::ForYou), FString());
	RefreshRightRail();
}

void UGFCRootWidget::RefreshRightRail()
{
	if (RightRail && IsWideLayout()) RightRail->Refresh();
}

void UGFCRootWidget::ShowExplore(const FString& Query) { ShowFeedPage(uint8(EGFCFeedSource::Explore), Query); }
void UGFCRootWidget::ShowBookmarks() { ShowFeedPage(uint8(EGFCFeedSource::Bookmarks), FString()); }
void UGFCRootWidget::ShowProfile() { ShowFeedPage(uint8(EGFCFeedSource::Mine), FString()); }

void UGFCRootWidget::ShowUser(const FString& Username)
{
	const UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	if (C->IsLoggedIn() && Username.Equals(C->GetUsername(), ESearchCase::IgnoreCase))
	{
		ShowProfile(); // your own page
		return;
	}
	ShowFeedPage(uint8(EGFCFeedSource::User), Username);
}
void UGFCRootWidget::Logout() { HandleLogout(); }

void UGFCRootWidget::ComposeFromFeed(const FString& PostBody, TOptional<EGFCCaptureSource> Pick, bool bPostNow)
{
	if (!CreateScreen)
	{
		return;
	}
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	WhenAllowedToPost([WeakThis, PostBody, Pick, bPostNow]()
	{
		if (!WeakThis.IsValid() || !WeakThis->CreateScreen) return;
		WeakThis->Activate(WeakThis->CreateScreen);
		WeakThis->CreateScreen->ComposeFrom(PostBody, Pick, bPostNow);
	});
}
void UGFCRootWidget::ShowWallet() { Activate(WalletScreen); }

void UGFCRootWidget::ShowCreate()
{
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	WhenAllowedToPost([WeakThis]()
	{
		if (WeakThis.IsValid()) WeakThis->Activate(WeakThis->CreateScreen);
	});
}

void UGFCRootWidget::WhenAllowedToPost(TFunction<void()> Then)
{
	UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	// Logged out, or known verified: nothing to ask. The create screen handles logging in.
	if (!C->IsLoggedIn() || C->GetLastKnownVerified().Get(false))
	{
		Then();
		return;
	}
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	TWeakObjectPtr<UGFCApiClient> WeakClient(C);
	C->GetNodeHealth([WeakThis, WeakClient, Then = MoveTemp(Then)](const FGFCApiResult& R, const FGFCNodeHealth& Health) mutable
	{
		if (!WeakThis.IsValid() || !WeakClient.IsValid()) return;
		// Test nets (and older nodes, which don't say) let anyone post.
		if (!R.bOk || !Health.bPhoneRequiredToPost)
		{
			Then();
			return;
		}
		WeakClient->GetVerifyStatus([WeakThis, Then = MoveTemp(Then)](const FGFCApiResult& S, const FGFCVerifyStatus& Status)
		{
			if (!WeakThis.IsValid()) return;
			if (!S.bOk || Status.bVerified)
			{
				Then(); // can't tell: the upload itself says if it's refused
				return;
			}
			WeakThis->OfferPhoneVerification(LOCTEXT("PhoneToPost",
				"Verify your phone number to post on the GFC network. It takes a minute, and it keeps bots and spam out."));
		});
	});
}

void UGFCRootWidget::OfferPhoneVerification(const FText& Why)
{
	TWeakObjectPtr<UGFCRootWidget> WeakThis(this);
	UGFCActionSheet::Show(this, Why, {LOCTEXT("VerifyPhone", "Verify my phone")}, [WeakThis](int32 Choice)
	{
		if (Choice == 0 && WeakThis.IsValid()) WeakThis->StartPhoneVerification();
	});
}

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
	if (ActiveScreen == FeedScreen && FeedScreen->GetSource() != EGFCFeedSource::ForYou)
	{
		ShowHome(); // from someone's page (or Explore, Bookmarks, your profile) back to the full feed
		return true;
	}
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
	if (Toast && ToastText && IsWideLayout())
	{
		ToastText->SetText(Message);
		if (UBorder* Bg = Cast<UBorder>(Toast))
		{
			Bg->SetBrush(FSlateRoundedBoxBrush(bError ? Danger : FGFCTheme::Get().Accent, 8.f));
		}
		Toast->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		ToastSeconds = bError ? 6.f : 3.5f;
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

void UGFCRootWidget::OnPostEdited(const FGFCSeed& Updated)
{
	if (FeedScreen) FeedScreen->ApplyEdit(Updated);
	if (SeedDetailScreen && SeedDetailScreen->GetSeed().Id == Updated.Id)
	{
		SeedDetailScreen->ApplyEdit(Updated);
	}
}

bool UGFCRootWidget::OpenNodePage(const FString& Path)
{
	const FString Url = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient()->GetBaseUrl() + Path;
	FString Error;
	FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
	if (!Error.IsEmpty())
	{
		ShowStatus(LOCTEXT("NoBrowserPage", "Couldn't open the browser."), true);
		return false;
	}
	return true;
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
	RefreshRightRail();
}

#undef LOCTEXT_NAMESPACE
