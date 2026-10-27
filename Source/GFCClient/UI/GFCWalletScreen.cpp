#include "UI/GFCWalletScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "GFCJson.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCWallet"

using namespace GFCUI;

namespace
{
	FString PickerLabel(const FGFCKnownWallet& W)
	{
		const FString Name = W.Name.IsEmpty() ? FString(TEXT("Wallet")) : W.Name;
		return FString::Printf(TEXT("%s  (%s)"), *Name, *ShortId(W.WalletId));
	}

	UVerticalBox* Section(UWidgetTree* T, UVerticalBox* Parent, const FText& Title, const FLinearColor& Color = Panel)
	{
		UBorder* Card = MakeCard(T, FMargin(18.f));
		UVerticalBox* Col = MakeColumn(T);
		Card->AddChild(Col);
		AddV(Col, MakeText(T, Title, 17, Text, true), 10.f);
		AddV(Parent, Card, 14.f);
		return Col;
	}
}

void UGFCWalletScreen::BuildDefaultLayout()
{
	UWidgetTree* T = WidgetTree;
	// Portrait: one scrolling column, most-used things first.
	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	T->RootWidget = Scroll;
	UVerticalBox* Left = MakeColumn(T);
	Scroll->AddChild(Left);

	// Balance
	{
		UVerticalBox* Col = Section(T, Left, LOCTEXT("YourWallet", "Your wallet"));
		WalletPicker = T->ConstructWidget<UComboBoxString>();
		StyleComboBox(WalletPicker);
		NoWalletText = MakeText(T, LOCTEXT("NoWallet", "You don't have a wallet on this device yet. Create one below."), 12, TextDim);
		AddV(Col, NoWalletText, 6.f);
		AddV(Col, WalletPicker, 12.f);
		BalanceText = MakeText(T, FText::GetEmpty(), 36, AccentText, true);
		AddV(Col, BalanceText, 4.f);
		WalletIdText = MakeText(T, FText::GetEmpty(), 12, TextDim);
		AddV(Col, WalletIdText, 8.f);
		UHorizontalBox* IdRow = MakeRow(T);
		CopyIdButton = MakeButton(T, LOCTEXT("CopyId", "Copy ID"), PanelRaised);
		RefreshButton = MakeButton(T, LOCTEXT("Refresh", "Refresh"), PanelRaised);
		AddH(IdRow, CopyIdButton, 8.f, true);
		AddH(IdRow, RefreshButton, 0.f, true);
		AddV(Col, IdRow, 0.f);
	}

	// Backup prompt: deliberately loud.
	{
		UBorder* Card = MakeCard(T, FMargin(18.f), &Warning); // the one card outlined in colour: hard to miss
		UVerticalBox* Col = MakeColumn(T);
		Card->AddChild(Col);
		BackupTitleText = MakeText(T, LOCTEXT("BackupTitle", "⚠ Back up your wallet"), 18, Warning, true);
		AddV(Col, BackupTitleText, 6.f);
		AddV(Col, MakeText(T, LOCTEXT("BackupWhy",
			"Your wallet keys live encrypted on your node, unlocked by your password. If you forget the password or the node "
			"loses its data, your GFC is gone. There is no reset. Export an encrypted backup and keep the file and its "
			"passphrase somewhere safe, apart from each other. Anyone with both can spend your GFC."), 12, Text), 12.f);
		BackupPasswordBox = MakeInput(T, LOCTEXT("PwHint", "your account password"), true);
		BackupPassphraseBox = MakeInput(T, LOCTEXT("PpHint", "a new backup passphrase, 12+ characters"), true);
		BackupConfirmBox = MakeInput(T, LOCTEXT("PcHint", "type the passphrase again"), true);
		AddV(Col, MakeField(T, LOCTEXT("Pw", "Account password"), BackupPasswordBox), 8.f);
		AddV(Col, MakeField(T, LOCTEXT("Pp", "Backup passphrase"), BackupPassphraseBox), 8.f);
		AddV(Col, MakeField(T, LOCTEXT("Pc", "Confirm passphrase"), BackupConfirmBox), 12.f);
		ExportButton = MakeButton(T, LOCTEXT("Export", "Export backup file"), Warning);
		AddV(Col, ExportButton, 8.f);
		BackupResultText = MakeText(T, FText::GetEmpty(), 11, Text);
		AddV(Col, BackupResultText, 0.f);
		BackupPanel = Card;
		AddV(Left, Card, 14.f);
	}

	// Send
	{
		UVerticalBox* Col = Section(T, Left, LOCTEXT("Send", "Send GFC"));
		RecipientBox = MakeInput(T, LOCTEXT("ToHint", "recipient wallet ID"));
		AmountBox = MakeInput(T, LOCTEXT("AmtHint", "e.g. 2.50"));
		ReasonBox = MakeInput(T, LOCTEXT("ReasonHint", "optional note, e.g. tip"));
		AddV(Col, MakeField(T, LOCTEXT("To", "To"), RecipientBox), 8.f);
		UHorizontalBox* Row = MakeRow(T);
		AddH(Row, MakeField(T, LOCTEXT("Amt", "Amount (GFC)"), AmountBox), 8.f, true);
		AddH(Row, MakeField(T, LOCTEXT("Reason", "Note"), ReasonBox), 0.f, true);
		AddV(Col, Row, 12.f);
		SendButton = MakeButton(T, LOCTEXT("SendBtn", "Send"));
		AddV(Col, SendButton, 0.f);
	}

	// Create / add
	{
		UVerticalBox* Col = Section(T, Left, LOCTEXT("More", "Wallets"));
		NewWalletNameBox = MakeInput(T, LOCTEXT("NameHint", "new wallet name (optional)"));
		CreateWalletButton = MakeButton(T, LOCTEXT("Create", "Create wallet"));
		AddV(Col, NewWalletNameBox, 8.f);
		AddV(Col, CreateWalletButton, 16.f);
		AddWalletIdBox = MakeInput(T, LOCTEXT("AddHint", "wallet ID you already own on this node"));
		AddWalletButton = MakeButton(T, LOCTEXT("Add", "Add by ID"), PanelRaised);
		AddV(Col, AddWalletIdBox, 8.f);
		AddV(Col, AddWalletButton, 10.f);
		AddV(Col, MakeText(T, LOCTEXT("PhoneNote",
			"Your first wallet receives the 100 GFC welcome grant once your phone number is verified (coming to this app in milestone 3)."),
			11, TextDim), 0.f);
	}

	// History, last: rows are added to a plain box inside the page's scroll.
	{
		UVerticalBox* Col = Section(T, Left, LOCTEXT("History", "History"));
		HistoryList = MakeColumn(T);
		AddV(Col, HistoryList, 0.f);
	}
}

void UGFCWalletScreen::BindEvents()
{
	if (WalletPicker) WalletPicker->OnSelectionChanged.AddDynamic(this, &ThisClass::HandlePickerChanged);
	if (CreateWalletButton) CreateWalletButton->OnClicked.AddDynamic(this, &ThisClass::HandleCreateWallet);
	if (AddWalletButton) AddWalletButton->OnClicked.AddDynamic(this, &ThisClass::HandleAddWallet);
	if (CopyIdButton) CopyIdButton->OnClicked.AddDynamic(this, &ThisClass::HandleCopyId);
	if (RefreshButton) RefreshButton->OnClicked.AddDynamic(this, &ThisClass::HandleRefresh);
	if (ExportButton) ExportButton->OnClicked.AddDynamic(this, &ThisClass::HandleExport);
	if (SendButton) SendButton->OnClicked.AddDynamic(this, &ThisClass::HandleSend);
}

void UGFCWalletScreen::OnScreenShown()
{
	RebuildPicker(SelectedWalletId);
	if (BackupResultText) BackupResultText->SetText(FText::GetEmpty());
	Super::OnScreenShown();
}

void UGFCWalletScreen::RebuildPicker(const FString& SelectId)
{
	const UGFCApiSubsystem* A = Api();
	if (!A || !WalletPicker)
	{
		return;
	}
	const TArray<FGFCKnownWallet> Wallets = A->GetKnownWallets();
	bRebuildingPicker = true;
	WalletPicker->ClearOptions();
	PickerWalletIds.Reset();
	int32 SelectIndex = 0;
	for (const FGFCKnownWallet& W : Wallets)
	{
		if (W.WalletId == SelectId)
		{
			SelectIndex = PickerWalletIds.Num();
		}
		WalletPicker->AddOption(PickerLabel(W));
		PickerWalletIds.Add(W.WalletId);
	}
	const bool bAny = PickerWalletIds.Num() > 0;
	SelectedWalletId = bAny ? PickerWalletIds[SelectIndex] : FString();
	if (bAny)
	{
		WalletPicker->SetSelectedIndex(SelectIndex);
	}
	bRebuildingPicker = false;

	WalletPicker->SetVisibility(bAny ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (NoWalletText) NoWalletText->SetVisibility(bAny ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (SendButton) SendButton->SetIsEnabled(bAny);
	if (!bAny)
	{
		if (BalanceText) BalanceText->SetText(FText::GetEmpty());
		if (WalletIdText) WalletIdText->SetText(FText::GetEmpty());
		if (HistoryList) HistoryList->ClearChildren();
	}
	UpdateBackupPrompt();
	RefreshSelected();
}

void UGFCWalletScreen::UpdateBackupPrompt()
{
	const UGFCApiSubsystem* A = Api();
	if (!BackupPanel || !A)
	{
		return;
	}
	const bool bHasWallets = A->GetKnownWallets().Num() > 0;
	BackupPanel->SetVisibility(bHasWallets ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (BackupTitleText)
	{
		// Keep the panel after a backup (new wallets need a new one), but tone the title down.
		BackupTitleText->SetText(A->NeedsBackup()
			? LOCTEXT("BackupNeeded", "⚠ Back up your wallet")
			: LOCTEXT("BackupDone", "✓ Backed up. Export again after adding a wallet."));
	}
}

void UGFCWalletScreen::HandlePickerChanged(FString, ESelectInfo::Type)
{
	if (bRebuildingPicker || !WalletPicker)
	{
		return;
	}
	const int32 Index = WalletPicker->GetSelectedIndex();
	if (PickerWalletIds.IsValidIndex(Index))
	{
		SelectedWalletId = PickerWalletIds[Index];
		RefreshSelected();
	}
}

void UGFCWalletScreen::HandleRefresh()
{
	RefreshSelected();
}

void UGFCWalletScreen::RefreshSelected()
{
	UGFCApiClient* C = Client();
	if (!C || SelectedWalletId.IsEmpty())
	{
		return;
	}
	if (WalletIdText) WalletIdText->SetText(FText::Format(LOCTEXT("IdShort", "ID {0}"), FText::FromString(ShortId(SelectedWalletId))));
	if (BalanceText) BalanceText->SetText(LOCTEXT("Loading", "…"));

	TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
	const FString Id = SelectedWalletId;
	C->GetWallet(Id, [WeakThis, Id](const FGFCApiResult& R, const FGFCWallet& W)
	{
		if (!WeakThis.IsValid() || WeakThis->SelectedWalletId != Id)
		{
			return;
		}
		if (!R.bOk)
		{
			if (WeakThis->BalanceText) WeakThis->BalanceText->SetText(LOCTEXT("Unknown", "—"));
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		if (WeakThis->BalanceText)
		{
			WeakThis->BalanceText->SetText(FText::Format(LOCTEXT("Balance", "{0} GFC"), FText::FromString(FGFCJson::FormatCents(W.BalanceCents))));
		}
	});
	C->GetHistory(Id, [WeakThis, Id](const FGFCApiResult& R, const TArray<FGFCTransaction>& Txs)
	{
		if (WeakThis.IsValid() && WeakThis->SelectedWalletId == Id && R.bOk)
		{
			WeakThis->ShowHistory(Txs);
		}
	});
}

void UGFCWalletScreen::ShowHistory(const TArray<FGFCTransaction>& Transactions)
{
	if (!HistoryList)
	{
		return;
	}
	HistoryList->ClearChildren();
	UWidgetTree* T = WidgetTree;
	if (Transactions.Num() == 0)
	{
		HistoryList->AddChild(MakeText(T, LOCTEXT("NoTx", "No transactions yet."), 12, TextDim));
		return;
	}
	for (const FGFCTransaction& Tx : Transactions)
	{
		const bool bIncoming = Tx.Recipient == SelectedWalletId;
		const FString Other = bIncoming ? Tx.Sender : Tx.Recipient;
		UBorder* RowBg = MakePanel(T, FGFCTheme::Get().Surface, FMargin(0.f, 10.f));
		UVerticalBox* Col = MakeColumn(T);
		RowBg->AddChild(Col);
		UHorizontalBox* Top = MakeRow(T);
		UTextBlock* Amount = MakeText(T, FText::FromString((bIncoming ? TEXT("+") : TEXT("−")) + FGFCJson::FormatCents(Tx.AmountCents) + TEXT(" GFC")),
			15, bIncoming ? Success : Text, true);
		Amount->SetAutoWrapText(false);
		AddH(Top, Amount, 12.f);
		AddH(Top, MakeText(T, FText::FromString(Tx.Reason.IsEmpty() ? Tx.TxType : Tx.Reason), 12, Text), 0.f, true);
		AddV(Col, Top, 2.f);
		AddV(Col, MakeText(T, FText::Format(LOCTEXT("TxMeta", "{0} {1} · {2} · {3}"),
			bIncoming ? LOCTEXT("From", "from") : LOCTEXT("ToL", "to"), FText::FromString(ShortId(Other)),
			FText::FromString(Tx.TxType), FText::FromString(Tx.Timestamp.Left(19).Replace(TEXT("T"), TEXT(" ")))), 10, TextDim), 0.f);
		if (HistoryList->GetChildrenCount() > 0)
		{
			HistoryList->AddChild(MakeDivider(T));
		}
		UPanelSlot* PanelSlot = HistoryList->AddChild(RowBg);
		if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(PanelSlot))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		else if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(PanelSlot))
		{
			V->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
	}
}

void UGFCWalletScreen::HandleCreateWallet()
{
	UGFCApiClient* C = Client();
	if (!C)
	{
		return;
	}
	const FString Name = NewWalletNameBox ? NewWalletNameBox->GetText().ToString() : FString();
	if (CreateWalletButton) CreateWalletButton->SetIsEnabled(false);
	ShowStatus(LOCTEXT("Creating", "Creating a wallet…"));
	TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
	C->CreateWallet(Name, [WeakThis](const FGFCApiResult& R, const FGFCWallet& W)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		if (WeakThis->CreateWalletButton) WeakThis->CreateWalletButton->SetIsEnabled(true);
		if (!R.bOk)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		if (UGFCApiSubsystem* A = WeakThis->Api())
		{
			A->RememberWallet(W.Id, W.Name);
		}
		if (WeakThis->NewWalletNameBox) WeakThis->NewWalletNameBox->SetText(FText::GetEmpty());
		WeakThis->ShowStatus(LOCTEXT("Created", "Wallet created. Now back it up: see the orange box."));
		WeakThis->RebuildPicker(W.Id);
	});
}

void UGFCWalletScreen::HandleAddWallet()
{
	UGFCApiClient* C = Client();
	const FString Id = AddWalletIdBox ? AddWalletIdBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (!C || Id.IsEmpty())
	{
		return;
	}
	// Check it exists before remembering it. Ownership is enforced by the node when spending.
	TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
	C->GetWallet(Id, [WeakThis](const FGFCApiResult& R, const FGFCWallet& W)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		if (!R.bOk)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		if (UGFCApiSubsystem* A = WeakThis->Api())
		{
			A->RememberWallet(W.Id, W.Name);
		}
		if (WeakThis->AddWalletIdBox) WeakThis->AddWalletIdBox->SetText(FText::GetEmpty());
		WeakThis->RebuildPicker(W.Id);
	});
}

void UGFCWalletScreen::HandleCopyId()
{
	if (!SelectedWalletId.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*SelectedWalletId);
		ShowStatus(LOCTEXT("CopiedId", "Wallet ID copied. Share it to receive GFC."));
	}
}

void UGFCWalletScreen::HandleExport()
{
	UGFCApiClient* C = Client();
	if (!C || !BackupPasswordBox || !BackupPassphraseBox)
	{
		return;
	}
	const FString Password = BackupPasswordBox->GetText().ToString();
	const FString Passphrase = BackupPassphraseBox->GetText().ToString();
	if (Password.IsEmpty())
	{
		ShowStatus(LOCTEXT("NeedPw", "Enter your account password."), true);
		return;
	}
	if (Passphrase.Len() < 12)
	{
		ShowStatus(LOCTEXT("ShortPp", "The backup passphrase needs at least 12 characters."), true);
		return;
	}
	if (BackupConfirmBox && BackupConfirmBox->GetText().ToString() != Passphrase)
	{
		ShowStatus(LOCTEXT("Mismatch", "The two passphrases don't match."), true);
		return;
	}
	ExportButton->SetIsEnabled(false);
	ShowStatus(LOCTEXT("Exporting", "Encrypting your backup on the node…"));
	TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
	C->ExportWallets(Password, Passphrase, [WeakThis](const FGFCApiResult& R, const FString& BackupJson)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->ExportButton->SetIsEnabled(true);
		if (!R.bOk)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		UGFCApiSubsystem* A = WeakThis->Api();
		const FString Path = A ? A->SaveBackupFile(BackupJson) : FString();
		if (Path.IsEmpty())
		{
			WeakThis->ShowStatus(LOCTEXT("SaveFailed", "The backup was made but couldn't be saved to a file."), true);
			return;
		}
		A->MarkWalletsBackedUp();
		WeakThis->BackupPasswordBox->SetText(FText::GetEmpty());
		WeakThis->BackupPassphraseBox->SetText(FText::GetEmpty());
		if (WeakThis->BackupConfirmBox) WeakThis->BackupConfirmBox->SetText(FText::GetEmpty());
		if (WeakThis->BackupResultText)
		{
			WeakThis->BackupResultText->SetText(FText::Format(LOCTEXT("Saved",
				"Saved to {0}\nMove it somewhere safe (a USB stick, a password manager). Store the passphrase separately."),
				FText::FromString(Path)));
		}
		WeakThis->ShowStatus(LOCTEXT("BackedUp", "Backup saved."));
		WeakThis->UpdateBackupPrompt();
	});
}

void UGFCWalletScreen::SetSendBusy(bool bBusy)
{
	if (SendButton) SendButton->SetIsEnabled(!bBusy && !SelectedWalletId.IsEmpty());
}

void UGFCWalletScreen::HandleSend()
{
	UGFCApiClient* C = Client();
	if (!C || SelectedWalletId.IsEmpty() || !RecipientBox || !AmountBox)
	{
		return;
	}
	const FString Recipient = RecipientBox->GetText().ToString().TrimStartAndEnd();
	int64 Cents = 0;
	if (Recipient.IsEmpty())
	{
		ShowStatus(LOCTEXT("NeedTo", "Enter the recipient's wallet ID."), true);
		return;
	}
	if (Recipient == SelectedWalletId)
	{
		ShowStatus(LOCTEXT("Self", "That's this wallet. Choose another recipient."), true);
		return;
	}
	if (!FGFCJson::ParseAmountToCents(AmountBox->GetText().ToString(), Cents))
	{
		ShowStatus(LOCTEXT("BadAmt", "Enter an amount like 2.50 (up to two decimals)."), true);
		return;
	}
	const FString Reason = ReasonBox ? ReasonBox->GetText().ToString() : FString();
	SetSendBusy(true);
	ShowStatus(FText::Format(LOCTEXT("Sending", "Sending {0} GFC…"), FText::FromString(FGFCJson::FormatCents(Cents))));
	TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
	C->CreateTransfer(SelectedWalletId, Recipient, Cents, Reason, [WeakThis](const FGFCApiResult& R, const FGFCTransaction& Tx)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->SetSendBusy(false);
		if (!R.bOk)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		WeakThis->AmountBox->SetText(FText::GetEmpty());
		if (WeakThis->ReasonBox) WeakThis->ReasonBox->SetText(FText::GetEmpty());
		WeakThis->ShowStatus(FText::Format(LOCTEXT("Sent", "Sent {0} GFC (tx {1})."),
			FText::FromString(FGFCJson::FormatCents(Tx.AmountCents)), FText::FromString(ShortId(Tx.TxHash.IsEmpty() ? Tx.Id : Tx.TxHash))));
		WeakThis->RefreshSelected();
	});
}

#undef LOCTEXT_NAMESPACE
