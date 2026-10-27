#include "UI/GFCLoginScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCApiSubsystem.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCLogin"

using namespace GFCUI;

void UGFCLoginScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;

	// Portrait, one column, scrolls when the on-screen keyboard is up.
	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	T->RootWidget = Scroll;
	USizeBox* Size = T->ConstructWidget<USizeBox>();
	Size->SetMaxDesiredWidth(520.f);
	Scroll->AddChild(Size);

	UBorder* Card = MakeCard(T, FMargin(20.f));
	Size->AddChild(Card);
	UVerticalBox* Col = MakeColumn(T);
	Card->AddChild(Col);

	AddV(Col, MakeText(T, LOCTEXT("Title", "Galactic Federation Coin"), 24, Text, true), 4.f);
	AddV(Col, MakeText(T, LOCTEXT("Subtitle", "Log in or create an account to see the feed and your wallet."), 13, TextDim), 18.f);

	UsernameBox = MakeInput(T, LOCTEXT("UserHint", "username"));
	PasswordBox = MakeInput(T, LOCTEXT("PassHint", "password"), true);
	EmailBox = MakeInput(T, LOCTEXT("EmailHint", "e-mail (optional)"));
	AddV(Col, MakeField(T, LOCTEXT("User", "Username"), UsernameBox), 12.f);
	AddV(Col, MakeField(T, LOCTEXT("Pass", "Password"), PasswordBox), 12.f);
	AddV(Col, MakeField(T, LOCTEXT("Email", "E-mail, for new accounts"), EmailBox), 18.f);

	LoginButton = MakeButton(T, LOCTEXT("Login", "Log in"));
	RegisterButton = MakeButton(T, LOCTEXT("Register", "Create account"), PanelRaised);
	AddV(Col, LoginButton, 10.f);
	AddV(Col, RegisterButton, 18.f);

	AddV(Col, MakeText(T, LOCTEXT("PasswordWarning",
		"Your password also unlocks your wallet keys on the node. There is no reset: back up your wallet after you create it."),
		12, Warning), 22.f);

	NodeText = MakeText(T, FText::GetEmpty(), 11, TextDim);
	AddV(Col, NodeText, 6.f);
	UHorizontalBox* Presets = MakeRow(T);
	LiveNodeButton = MakeButton(T, LOCTEXT("Live", "Live node"), PanelRaised);
	LocalNodeButton = MakeButton(T, LOCTEXT("Local", "Test node (USB)"), PanelRaised);
	AddH(Presets, LiveNodeButton, 8.f, true);
	AddH(Presets, LocalNodeButton, 0.f, true);
	AddV(Col, Presets, 10.f);
	NodeUrlBox = MakeInput(T, LOCTEXT("NodeHint", "https://node.galacticfederation.tech"));
	UseNodeButton = MakeButton(T, LOCTEXT("UseNode", "Use this node"), PanelRaised);
	AddV(Col, NodeUrlBox, 8.f);
	AddV(Col, UseNodeButton, 0.f);
}

void UGFCLoginScreen::BindEvents()
{
	if (LoginButton) LoginButton->OnClicked.AddDynamic(this, &ThisClass::HandleLogin);
	if (RegisterButton) RegisterButton->OnClicked.AddDynamic(this, &ThisClass::HandleRegister);
	if (UseNodeButton) UseNodeButton->OnClicked.AddDynamic(this, &ThisClass::HandleUseNode);
	if (LiveNodeButton) LiveNodeButton->OnClicked.AddDynamic(this, &ThisClass::HandleLiveNode);
	if (LocalNodeButton) LocalNodeButton->OnClicked.AddDynamic(this, &ThisClass::HandleLocalNode);
	if (PasswordBox) PasswordBox->OnTextCommitted.AddDynamic(this, &ThisClass::HandlePasswordCommitted);
}

void UGFCLoginScreen::OnScreenShown()
{
	if (PasswordBox)
	{
		PasswordBox->SetText(FText::GetEmpty());
	}
	RefreshNodeText();
	SetBusy(false);
	Super::OnScreenShown();
}

void UGFCLoginScreen::RefreshNodeText()
{
	if (const UGFCApiClient* C = Client())
	{
		if (NodeText)
		{
			NodeText->SetText(FText::Format(LOCTEXT("NodeIs", "Node: {0}"), FText::FromString(C->GetBaseUrl())));
		}
		if (NodeUrlBox)
		{
			NodeUrlBox->SetText(FText::FromString(C->GetBaseUrl()));
		}
	}
}

bool UGFCLoginScreen::ReadCredentials(FString& OutUser, FString& OutPassword) const
{
	OutUser = UsernameBox ? UsernameBox->GetText().ToString().TrimStartAndEnd() : FString();
	OutPassword = PasswordBox ? PasswordBox->GetText().ToString() : FString();
	if (OutUser.IsEmpty() || OutPassword.IsEmpty())
	{
		ShowStatus(LOCTEXT("NeedBoth", "Enter a username and password."), true);
		return false;
	}
	return true;
}

void UGFCLoginScreen::SetBusy(bool bBusy)
{
	if (LoginButton) LoginButton->SetIsEnabled(!bBusy);
	if (RegisterButton) RegisterButton->SetIsEnabled(!bBusy);
}

void UGFCLoginScreen::HandlePasswordCommitted(const FText&, ETextCommit::Type Method)
{
	if (Method == ETextCommit::OnEnter)
	{
		HandleLogin();
	}
}

void UGFCLoginScreen::HandleLogin()
{
	FString User, Password;
	UGFCApiClient* C = Client();
	if (!C || !ReadCredentials(User, Password))
	{
		return;
	}
	SetBusy(true);
	ShowStatus(LOCTEXT("LoggingIn", "Logging in…"));
	TWeakObjectPtr<UGFCLoginScreen> WeakThis(this);
	C->Login(User, Password, [WeakThis](const FGFCApiResult& R, const FGFCAuthResult& A)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->SetBusy(false);
		if (!R.bOk)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		if (WeakThis->PasswordBox) WeakThis->PasswordBox->SetText(FText::GetEmpty());
		WeakThis->ShowStatus(FText::Format(LOCTEXT("Welcome", "Welcome back, {0}."), FText::FromString(A.Username)));
		if (WeakThis->Root.IsValid()) WeakThis->Root->OnLoggedIn(false);
	});
}

void UGFCLoginScreen::HandleRegister()
{
	FString User, Password;
	UGFCApiClient* C = Client();
	if (!C || !ReadCredentials(User, Password))
	{
		return;
	}
	const FString Email = EmailBox ? EmailBox->GetText().ToString() : FString();
	SetBusy(true);
	ShowStatus(LOCTEXT("Registering", "Creating your account…"));
	TWeakObjectPtr<UGFCLoginScreen> WeakThis(this);
	C->Register(User, Password, Email, [WeakThis](const FGFCApiResult& R, const FGFCAuthResult& A)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->SetBusy(false);
		if (!R.bOk)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		if (WeakThis->PasswordBox) WeakThis->PasswordBox->SetText(FText::GetEmpty());
		WeakThis->ShowStatus(FText::Format(LOCTEXT("Created", "Account created. Welcome, {0}! Create your first wallet."), FText::FromString(A.Username)));
		if (WeakThis->Root.IsValid()) WeakThis->Root->OnLoggedIn(true);
	});
}

void UGFCLoginScreen::HandleLiveNode()
{
	SwitchNode(UGFCApiSettings::DefaultNodeBaseUrl);
}

void UGFCLoginScreen::HandleLocalNode()
{
	// On the phone, 127.0.0.1:8100 reaches the PC's test node through `adb reverse tcp:8100 tcp:8100`.
	SwitchNode(TEXT("http://127.0.0.1:8100"));
}

void UGFCLoginScreen::HandleUseNode()
{
	if (NodeUrlBox)
	{
		SwitchNode(NodeUrlBox->GetText().ToString());
	}
}

void UGFCLoginScreen::SwitchNode(const FString& Url)
{
	UGFCApiSubsystem* A = Api();
	if (!A)
	{
		return;
	}
	if (A->SetNode(Url))
	{
		ShowStatus(FText::Format(LOCTEXT("NodeSet", "Using node {0}."), FText::FromString(A->GetClient()->GetBaseUrl())));
	}
	else
	{
		ShowStatus(LOCTEXT("BadNode", "Enter a node address starting with http:// or https://."), true);
	}
	RefreshNodeText();
}

#undef LOCTEXT_NAMESPACE
