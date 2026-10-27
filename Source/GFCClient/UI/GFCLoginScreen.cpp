#include "UI/GFCLoginScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCLogin"

using namespace GFCUI;

void UGFCLoginScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;

	UOverlay* Center = T->ConstructWidget<UOverlay>();
	T->RootWidget = Center;
	USizeBox* Size = T->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(480.f);
	if (UOverlaySlot* S = Center->AddChildToOverlay(Size))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(0.f, 40.f, 0.f, 0.f));
	}

	UBorder* Card = MakePanel(T, Panel, FMargin(28.f));
	Size->AddChild(Card);
	UVerticalBox* Col = MakeColumn(T);
	Card->AddChild(Col);

	AddV(Col, MakeText(T, LOCTEXT("Title", "Galactic Federation Coin"), 24, Text, true), 4.f);
	AddV(Col, MakeText(T, LOCTEXT("Subtitle", "Log in or create an account to see the feed and your wallet."), 12, TextDim), 18.f);

	UsernameBox = MakeInput(T, LOCTEXT("UserHint", "username"));
	PasswordBox = MakeInput(T, LOCTEXT("PassHint", "password"), true);
	EmailBox = MakeInput(T, LOCTEXT("EmailHint", "e-mail (optional, for registering)"));
	AddV(Col, MakeField(T, LOCTEXT("User", "Username"), UsernameBox), 10.f);
	AddV(Col, MakeField(T, LOCTEXT("Pass", "Password"), PasswordBox), 10.f);
	AddV(Col, MakeField(T, LOCTEXT("Email", "E-mail"), EmailBox), 16.f);

	UHorizontalBox* Buttons = MakeRow(T);
	LoginButton = MakeButton(T, LOCTEXT("Login", "Log in"));
	RegisterButton = MakeButton(T, LOCTEXT("Register", "Create account"), PanelRaised);
	AddH(Buttons, LoginButton, 8.f, true);
	AddH(Buttons, RegisterButton, 0.f, true);
	AddV(Col, Buttons, 18.f);

	AddV(Col, MakeText(T, LOCTEXT("PasswordWarning",
		"Your password also unlocks your wallet keys on the node. There is no reset: back up your wallet after you create it."),
		11, Warning), 18.f);

	NodeText = MakeText(T, FText::GetEmpty(), 11, TextDim);
	AddV(Col, NodeText, 4.f);
	UHorizontalBox* NodeRow = MakeRow(T);
	NodeUrlBox = MakeInput(T, LOCTEXT("NodeHint", "https://node.galacticfederation.tech"));
	UseNodeButton = MakeButton(T, LOCTEXT("UseNode", "Use node"), PanelRaised);
	AddH(NodeRow, NodeUrlBox, 8.f, true);
	AddH(NodeRow, UseNodeButton, 0.f);
	AddV(Col, NodeRow, 0.f);
}

void UGFCLoginScreen::BindEvents()
{
	if (LoginButton) LoginButton->OnClicked.AddDynamic(this, &ThisClass::HandleLogin);
	if (RegisterButton) RegisterButton->OnClicked.AddDynamic(this, &ThisClass::HandleRegister);
	if (UseNodeButton) UseNodeButton->OnClicked.AddDynamic(this, &ThisClass::HandleUseNode);
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

void UGFCLoginScreen::HandleUseNode()
{
	UGFCApiSubsystem* A = Api();
	if (!A || !NodeUrlBox)
	{
		return;
	}
	if (A->SetNode(NodeUrlBox->GetText().ToString()))
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
