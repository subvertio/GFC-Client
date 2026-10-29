#include "UI/GFCBridgeScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "GFCJson.h"
#include "UI/GFCRootWidget.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCBridge"

using namespace GFCUI;

namespace
{
	FText Gfc(int64 Cents)
	{
		return FText::Format(LOCTEXT("GfcAmount", "{0} GFC"), FText::FromString(FGFCJson::FormatCents(Cents)));
	}

	FString ShortAddress(const FString& Address)
	{
		const FString A = Address.TrimStartAndEnd();
		return A.Len() > 14 ? A.Left(8) + TEXT("…") + A.Right(6) : A;
	}
}

void UGFCBridgeScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;
	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	T->RootWidget = Scroll;
	UVerticalBox* Page = MakeColumn(T);
	Scroll->AddChild(Page);

	AddV(Page, MakeText(T, LOCTEXT("Title", "Bridge to Base"), 22, Text, true), 6.f);
	AddV(Page, MakeText(T, LOCTEXT("What",
		"Move GFC from your wallet to the GFC token on Base, where you can hold it in any Ethereum wallet or swap it. "
		"It's backed one for one by GFC locked in the bridge's escrow, and you can bridge it back."), 12, TextDim), 14.f);

	UnavailableText = MakeText(T, LOCTEXT("Loading", "Checking whether this node bridges…"), 13, Text);
	AddV(Page, UnavailableText, 14.f);

	UVerticalBox* Form = MakeColumn(T);
	FormPanel = Form;
	AddV(Page, Form, 0.f);

	// The not-an-investment notice comes first, outlined so it's read.
	{
		UBorder* Card = MakeCard(T, FMargin(16.f), &Warning);
		NoticeText = MakeText(T, FText::GetEmpty(), 12, Text);
		Card->AddChild(NoticeText);
		AddV(Form, Card, 14.f);
	}

	// From: the wallet and what it can bridge now
	{
		UBorder* Card = MakeCard(T, FMargin(18.f));
		UVerticalBox* Col = MakeColumn(T);
		Card->AddChild(Col);
		AddV(Col, MakeText(T, LOCTEXT("From", "From"), 17, Text, true), 10.f);
		ChainPicker = T->ConstructWidget<UComboBoxString>();
		StyleComboBox(ChainPicker);
		AddV(Col, ChainPicker, 10.f);
		WalletPicker = T->ConstructWidget<UComboBoxString>();
		StyleComboBox(WalletPicker);
		AddV(Col, WalletPicker, 12.f);
		BridgeableText = MakeText(T, FText::GetEmpty(), 26, AccentText, true);
		AddV(Col, BridgeableText, 4.f);
		HeldText = MakeText(T, FText::GetEmpty(), 11, TextDim);
		AddV(Col, HeldText, 0.f);
		AddV(Form, Card, 14.f);
	}

	// To: the address on Base, and how much
	{
		UBorder* Card = MakeCard(T, FMargin(18.f));
		UVerticalBox* Col = MakeColumn(T);
		Card->AddChild(Col);
		AddV(Col, MakeText(T, LOCTEXT("To", "To"), 17, Text, true), 10.f);
		AddressBox = MakeInput(T, LOCTEXT("AddressHint", "your Base address, 0x…"));
		AddV(Col, MakeField(T, LOCTEXT("Address", "Your address on Base"), AddressBox), 10.f);
		UHorizontalBox* Row = MakeRow(T);
		AmountBox = MakeInput(T, LOCTEXT("AmountHint", "e.g. 150"));
		AddH(Row, MakeField(T, LOCTEXT("Amount", "Amount (GFC)"), AmountBox), 8.f, true);
		MaxButton = MakeButton(T, LOCTEXT("Max", "Max"), PanelRaised);
		AddH(Row, MaxButton, 0.f);
		AddV(Col, Row, 10.f);
		RulesText = MakeText(T, FText::GetEmpty(), 11, TextDim);
		AddV(Col, RulesText, 0.f);
		AddV(Form, Card, 14.f);
	}

	// Review, then confirm
	SummaryText = MakeText(T, FText::GetEmpty(), 13, Text);
	AddV(Form, SummaryText, 10.f);
	UTextBlock* Label = nullptr;
	BridgeButton = MakeButton(T, LOCTEXT("Review", "Review"), Accent, &Label);
	BridgeButtonLabel = Label;
	AddV(Form, BridgeButton, 8.f);
	CancelButton = MakeButton(T, LOCTEXT("Cancel", "Change something"), PanelRaised);
	AddV(Form, CancelButton, 10.f);
	ResultText = MakeText(T, FText::GetEmpty(), 12, Success);
	AddV(Form, ResultText, 14.f);

	BackButton = MakeButton(T, LOCTEXT("Back", "Back to wallet"), PanelRaised);
	AddV(Page, BackButton, 0.f);
}

void UGFCBridgeScreen::BindEvents()
{
	if (ChainPicker) ChainPicker->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleChainChanged);
	if (WalletPicker) WalletPicker->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleWalletChanged);
	if (AddressBox) AddressBox->OnTextChanged.AddDynamic(this, &ThisClass::HandleInputChanged);
	if (AmountBox) AmountBox->OnTextChanged.AddDynamic(this, &ThisClass::HandleInputChanged);
	if (MaxButton) MaxButton->OnClicked.AddDynamic(this, &ThisClass::HandleMax);
	if (BridgeButton) BridgeButton->OnClicked.AddDynamic(this, &ThisClass::HandleBridge);
	if (CancelButton) CancelButton->OnClicked.AddDynamic(this, &ThisClass::HandleCancel);
	if (BackButton) BackButton->OnClicked.AddDynamic(this, &ThisClass::HandleBack);
}

void UGFCBridgeScreen::SetWallet(const FString& WalletId)
{
	SelectedWalletId = WalletId;
}

void UGFCBridgeScreen::OnScreenShown()
{
	if (ResultText) ResultText->SetText(FText::GetEmpty());
	SetReviewing(false);
	RebuildWalletPicker();
	LoadInfo();
	Super::OnScreenShown();
}

void UGFCBridgeScreen::LoadInfo()
{
	UGFCApiClient* C = Client();
	if (!C)
	{
		return;
	}
	bInfoLoaded = false;
	ShowInfo();
	TWeakObjectPtr<UGFCBridgeScreen> WeakThis(this);
	C->GetBridgeInfo([WeakThis](const FGFCApiResult& R, const FGFCBridgeInfo& I)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		// An older node has no /bridge endpoints (404): that's "not available", not an error.
		WeakThis->Info = R.bOk ? I : FGFCBridgeInfo();
		WeakThis->bInfoLoaded = true;
		if (!R.bOk && R.StatusCode != 404 && WeakThis->UnavailableText)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
		}
		WeakThis->ShowInfo();
		WeakThis->LoadQuote();
	});
}

void UGFCBridgeScreen::LoadQuote()
{
	UGFCApiClient* C = Client();
	bQuoteLoaded = false;
	if (BridgeableText) BridgeableText->SetText(LOCTEXT("Dots", "…"));
	if (HeldText) HeldText->SetText(FText::GetEmpty());
	if (!C || SelectedWalletId.IsEmpty() || !Info.bEnabled)
	{
		return;
	}
	TWeakObjectPtr<UGFCBridgeScreen> WeakThis(this);
	const FString Id = SelectedWalletId;
	C->GetBridgeQuote(Id, [WeakThis, Id](const FGFCApiResult& R, const FGFCBridgeQuote& Q)
	{
		if (!WeakThis.IsValid() || WeakThis->SelectedWalletId != Id)
		{
			return;
		}
		if (!R.bOk)
		{
			if (WeakThis->BridgeableText) WeakThis->BridgeableText->SetText(LOCTEXT("Unknown", "—"));
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		WeakThis->Quote = Q;
		WeakThis->bQuoteLoaded = true;
		if (WeakThis->BridgeableText)
		{
			WeakThis->BridgeableText->SetText(FText::Format(LOCTEXT("Bridgeable", "{0} can be bridged"), Gfc(Q.BridgeableCents)));
		}
		if (WeakThis->HeldText)
		{
			WeakThis->HeldText->SetText(Q.HeldCents > 0
				? FText::Format(LOCTEXT("Held",
					"Balance {0}. {1} is free credit (welcome grant, invite or post bonuses, also when passed on from "
					"another wallet) and can be bridged {2} days after it arrived."),
					Gfc(Q.BalanceCents), Gfc(Q.HeldCents), FText::AsNumber(WeakThis->Info.FaucetHoldDays))
				: FText::Format(LOCTEXT("Balance", "Balance {0}."), Gfc(Q.BalanceCents)));
		}
	});
}

const FGFCBridgeChain* UGFCBridgeScreen::SelectedChain() const
{
	return Info.Chains.IsValidIndex(ChainIndex) ? &Info.Chains[ChainIndex] : nullptr;
}

void UGFCBridgeScreen::ShowInfo()
{
	const FGFCBridgeChain* Chain = SelectedChain();
	const bool bHasWallet = !SelectedWalletId.IsEmpty();
	const bool bLive = Info.bEnabled && Chain && !Chain->TokenAddress.IsEmpty();
	if (UnavailableText)
	{
		FText Why;
		if (!bInfoLoaded) Why = LOCTEXT("Loading", "Checking whether this node bridges…");
		else if (!Info.bEnabled) Why = LOCTEXT("Off", "Bridging isn't available on this node yet.");
		else if (!bLive) Why = LOCTEXT("NoToken", "The GFC token on this chain isn't live yet. Bridging opens once it is.");
		else if (!bHasWallet) Why = LOCTEXT("NoWallet", "Create a wallet first: go back to your wallet.");
		UnavailableText->SetText(Why);
		UnavailableText->SetVisibility(Why.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (FormPanel) FormPanel->SetVisibility(bLive && bHasWallet ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bLive)
	{
		return;
	}

	if (NoticeText)
	{
		NoticeText->SetText(Info.Notice.IsEmpty()
			? LOCTEXT("DefaultNotice", "Wrapped GFC is an in-app credit, not an investment. Bridging is new code: start small.")
			: FText::FromString(Info.Notice));
	}
	if (ChainPicker)
	{
		bRebuilding = true;
		ChainPicker->ClearOptions();
		for (const FGFCBridgeChain& C : Info.Chains)
		{
			ChainPicker->AddOption(C.Name);
		}
		ChainPicker->SetSelectedIndex(ChainIndex);
		bRebuilding = false;
		ChainPicker->SetVisibility(Info.Chains.Num() > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (RulesText)
	{
		RulesText->SetText(FText::Format(LOCTEXT("Rules",
			"Minimum {0}. Bridge fee {1}, taken from what's minted. Tokens arrive on {2} after the bridge's safety wait. "
			"Check the address: it can't be changed afterwards."),
			Gfc(Info.MinDepositCents), Gfc(Info.RelayFeeCents), FText::FromString(Chain->Name)));
	}
	if (AddressBox)
	{
		AddressBox->SetHintText(FText::Format(LOCTEXT("AddressHintChain", "your {0} address, 0x…"), FText::FromString(Chain->Name)));
	}
}

void UGFCBridgeScreen::RebuildWalletPicker()
{
	const UGFCApiSubsystem* A = Api();
	if (!A || !WalletPicker)
	{
		return;
	}
	bRebuilding = true;
	WalletPicker->ClearOptions();
	PickerWalletIds.Reset();
	int32 SelectIndex = 0;
	for (const FGFCKnownWallet& W : A->GetKnownWallets())
	{
		if (W.WalletId == SelectedWalletId)
		{
			SelectIndex = PickerWalletIds.Num();
		}
		const FString Name = W.Name.IsEmpty() ? FString(TEXT("Wallet")) : W.Name;
		WalletPicker->AddOption(FString::Printf(TEXT("%s  (%s)"), *Name, *ShortId(W.WalletId)));
		PickerWalletIds.Add(W.WalletId);
	}
	SelectedWalletId = PickerWalletIds.IsValidIndex(SelectIndex) ? PickerWalletIds[SelectIndex] : FString();
	if (!SelectedWalletId.IsEmpty())
	{
		WalletPicker->SetSelectedIndex(SelectIndex);
	}
	bRebuilding = false;
}

void UGFCBridgeScreen::HandleChainChanged(FString, ESelectInfo::Type)
{
	if (!bRebuilding && ChainPicker && Info.Chains.IsValidIndex(ChainPicker->GetSelectedIndex()))
	{
		ChainIndex = ChainPicker->GetSelectedIndex();
		SetReviewing(false);
		ShowInfo();
	}
}

void UGFCBridgeScreen::HandleWalletChanged(FString, ESelectInfo::Type)
{
	if (!bRebuilding && WalletPicker && PickerWalletIds.IsValidIndex(WalletPicker->GetSelectedIndex()))
	{
		SelectedWalletId = PickerWalletIds[WalletPicker->GetSelectedIndex()];
		SetReviewing(false);
		LoadQuote();
	}
}

void UGFCBridgeScreen::HandleInputChanged(const FText&)
{
	// Anything edited after the review has to be reviewed again.
	if (bReviewing) SetReviewing(false);
}

void UGFCBridgeScreen::HandleMax()
{
	if (bQuoteLoaded && AmountBox)
	{
		AmountBox->SetText(FText::FromString(FGFCJson::FormatCents(Quote.BridgeableCents)));
	}
}

bool UGFCBridgeScreen::Validate(int64& OutCents, FText& OutError) const
{
	OutCents = 0;
	if (SelectedWalletId.IsEmpty() || !SelectedChain())
	{
		OutError = LOCTEXT("NeedWallet", "Choose a wallet.");
		return false;
	}
	if (!AddressBox || !FGFCJson::IsEvmAddress(AddressBox->GetText().ToString()))
	{
		OutError = LOCTEXT("BadAddress", "Enter your Base address: 0x followed by 40 letters and digits.");
		return false;
	}
	if (!AmountBox || !FGFCJson::ParseAmountToCents(AmountBox->GetText().ToString(), OutCents))
	{
		OutError = LOCTEXT("BadAmount", "Enter an amount like 150 or 150.50.");
		return false;
	}
	if (OutCents < Info.MinDepositCents || OutCents <= Info.RelayFeeCents)
	{
		OutError = FText::Format(LOCTEXT("TooSmall", "The minimum is {0}."), Gfc(FMath::Max(Info.MinDepositCents, Info.RelayFeeCents + 1)));
		return false;
	}
	if (bQuoteLoaded && OutCents > Quote.BridgeableCents)
	{
		OutError = FText::Format(LOCTEXT("TooMuch", "Only {0} can be bridged now."), Gfc(Quote.BridgeableCents));
		return false;
	}
	return true;
}

void UGFCBridgeScreen::SetReviewing(bool bReview)
{
	bReviewing = bReview;
	if (BridgeButtonLabel)
	{
		BridgeButtonLabel->SetText(bReview ? LOCTEXT("Confirm", "Confirm: bridge now") : LOCTEXT("Review", "Review"));
	}
	if (CancelButton) CancelButton->SetVisibility(bReview ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bReview && SummaryText) SummaryText->SetText(FText::GetEmpty());
}

void UGFCBridgeScreen::SetBusy(bool bInBusy)
{
	bBusy = bInBusy;
	if (BridgeButton) BridgeButton->SetIsEnabled(!bInBusy);
	if (CancelButton) CancelButton->SetIsEnabled(!bInBusy);
}

void UGFCBridgeScreen::HandleBridge()
{
	int64 Cents = 0;
	FText Error;
	if (bBusy)
	{
		return;
	}
	if (!Validate(Cents, Error))
	{
		SetReviewing(false);
		ShowStatus(Error, true);
		return;
	}
	const FGFCBridgeChain& Chain = *SelectedChain();
	const FString Address = AddressBox->GetText().ToString().TrimStartAndEnd();
	if (!bReviewing)
	{
		if (SummaryText)
		{
			SummaryText->SetText(FText::Format(LOCTEXT("Summary",
				"You're about to send {0} from wallet {1} to the bridge. {2} will be minted to {3} on {4} after the safety "
				"wait ({5} bridge fee). This can't be undone."),
				Gfc(Cents), FText::FromString(ShortId(SelectedWalletId)), Gfc(Cents - Info.RelayFeeCents),
				FText::FromString(ShortAddress(Address)), FText::FromString(Chain.Name), Gfc(Info.RelayFeeCents)));
		}
		SetReviewing(true);
		return;
	}

	UGFCApiClient* C = Client();
	if (!C)
	{
		return;
	}
	SetBusy(true);
	ShowStatus(LOCTEXT("Depositing", "Sending to the bridge…"));
	TWeakObjectPtr<UGFCBridgeScreen> WeakThis(this);
	C->BridgeDeposit(SelectedWalletId, Chain.ChainId, Address, Cents,
		[WeakThis, ChainName = Chain.Name](const FGFCApiResult& R, const FGFCBridgeDeposit& D)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			WeakThis->SetBusy(false);
			WeakThis->SetReviewing(false);
			if (!R.bOk)
			{
				WeakThis->ShowStatus(FText::FromString(R.Error), true);
				WeakThis->LoadQuote();
				return;
			}
			if (WeakThis->AmountBox) WeakThis->AmountBox->SetText(FText::GetEmpty());
			if (WeakThis->ResultText)
			{
				WeakThis->ResultText->SetText(FText::Format(LOCTEXT("Done",
					"Sent {0} to the bridge (tx {1}). {2} will arrive at {3} on {4} after the safety wait; it can take a while."),
					Gfc(D.AmountCents), FText::FromString(ShortId(D.TxHash)), Gfc(D.MintCents),
					FText::FromString(ShortAddress(D.EvmAddress)), FText::FromString(ChainName)));
			}
			WeakThis->ShowStatus(LOCTEXT("Deposited", "Sent to the bridge."));
			WeakThis->LoadQuote();
		});
}

void UGFCBridgeScreen::HandleCancel()
{
	SetReviewing(false);
}

void UGFCBridgeScreen::HandleBack()
{
	if (Root.IsValid()) Root->ShowWallet();
}

#undef LOCTEXT_NAMESPACE
