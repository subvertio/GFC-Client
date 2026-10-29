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
#include "HAL/PlatformProcess.h"
#include "UI/GFCRewardBurst.h"
#include "UI/GFCRootWidget.h"
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
	// Portrait: one scrolling column in the order things are done (create, back up, receive, send, history).
	UScrollBox* Scroll = T->ConstructWidget<UScrollBox>();
	T->RootWidget = Scroll;
	UVerticalBox* Left = MakeColumn(T);
	Scroll->AddChild(Left);

	// Recovery code to write down: first thing on screen after registering (or making a new code).
	{
		UBorder* Card = MakeCard(T, FMargin(18.f), &Warning);
		UVerticalBox* Col = MakeColumn(T);
		Card->AddChild(Col);
		AddV(Col, MakeText(T, LOCTEXT("RecoveryTitle", "Write down your recovery code"), 18, Warning, true), 6.f);
		AddV(Col, MakeText(T, LOCTEXT("RecoveryWhy",
			"If you forget your password, reset it with your phone or email and this code, and your wallets come back with it. "
			"It's shown only this once and GFC can't show it again. Keep it somewhere safe, away from your phone."), 12, Text), 12.f);
		RecoveryCodeText = MakeText(T, FText::GetEmpty(), 20, Text, true);
		AddV(Col, RecoveryCodeText, 12.f);
		UHorizontalBox* Row = MakeRow(T);
		CopyRecoveryButton = MakeButton(T, LOCTEXT("CopyCode", "Copy"), PanelRaised);
		SavedRecoveryButton = MakeButton(T, LOCTEXT("SavedCode", "I've written it down"), Warning);
		AddH(Row, CopyRecoveryButton, 8.f, true);
		AddH(Row, SavedRecoveryButton, 0.f, true);
		AddV(Col, Row, 0.f);
		RecoveryCodePanel = Card;
		RecoveryCodePanel->SetVisibility(ESlateVisibility::Collapsed);
		AddV(Left, Card, 16.f);
	}

	// 0. Verify the phone: rewards (welcome grant, first post bonus, plays) wait for it.
	{
		UBorder* Card = MakeCard(T, FMargin(18.f), &Accent);
		UVerticalBox* Col = MakeColumn(T);
		Card->AddChild(Col);
		AddV(Col, MakeText(T, LOCTEXT("VerifyTitle", "Verify your phone"), 18, Text, true), 6.f);
		AddV(Col, MakeText(T, LOCTEXT("VerifyWhy",
			"Unlocks the 100 GFC welcome grant, your first post bonus, and rewards for the creators you watch. One account per "
			"phone number. It opens a page in your browser; come back to GFC when it's done."), 12, TextDim), 12.f);
		VerifyPhoneButton = MakeButton(T, LOCTEXT("VerifyButton", "Verify phone"));
		AddV(Col, VerifyPhoneButton, 0.f);
		VerifyPanel = Card;
		VerifyPanel->SetVisibility(ESlateVisibility::Collapsed); // until the node says it's needed
		AddV(Left, Card, 16.f);
	}

	// 1. Create a wallet (or add one you own): the first thing a new user does
	{
		UVerticalBox* Col = Section(T, Left, LOCTEXT("More", "1. Your wallets"));
		NewWalletNameBox = MakeInput(T, LOCTEXT("NameHint", "new wallet name (optional)"));
		CreateWalletButton = MakeButton(T, LOCTEXT("Create", "Create wallet"));
		AddV(Col, NewWalletNameBox, 8.f);
		AddV(Col, CreateWalletButton, 16.f);
		AddWalletIdBox = MakeInput(T, LOCTEXT("AddHint", "wallet ID you already own on this node"));
		AddWalletButton = MakeButton(T, LOCTEXT("Add", "Add by ID"), PanelRaised);
		AddV(Col, AddWalletIdBox, 8.f);
		AddV(Col, AddWalletButton, 10.f);
		AddV(Col, MakeText(T, LOCTEXT("PhoneNote",
			"Your first wallet receives the 100 GFC welcome grant once your phone number is verified."),
			11, TextDim), 0.f);
	}

	// 2. Back it up: deliberately loud, right after creating.
	{
		UBorder* Card = MakeCard(T, FMargin(18.f), &Warning); // the one card outlined in colour: hard to miss
		UVerticalBox* Col = MakeColumn(T);
		Card->AddChild(Col);
		BackupTitleText = MakeText(T, LOCTEXT("BackupTitle", "⚠ Back up your wallet"), 18, Warning, true);
		AddV(Col, BackupTitleText, 6.f);
		AddV(Col, MakeText(T, LOCTEXT("BackupWhy",
			"Your wallet keys live encrypted on your node, unlocked by your password (or your recovery code after a reset). "
			"If you lose both, or the node loses its data, your GFC is gone. Export an encrypted backup and keep the file and "
			"its passphrase somewhere safe, apart from each other. Anyone with both can spend your GFC."), 12, Text), 12.f);
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

	// Account recovery: what a forgotten password can be reset with
	{
		UVerticalBox* Col = Section(T, Left, LOCTEXT("SecurityTitle", "Account recovery"));
		SecurityStatusText = MakeText(T, FText::GetEmpty(), 12, TextDim);
		AddV(Col, SecurityStatusText, 10.f);
		VerifyEmailButton = MakeButton(T, LOCTEXT("VerifyEmail", "Verify an email"), PanelRaised);
		AddV(Col, VerifyEmailButton, 6.f);
		AddV(Col, MakeText(T, LOCTEXT("EmailWhy",
			"A verified email can reset your password, or move your account to a new phone number. It opens a page in your "
			"browser."), 11, TextDim), 14.f);
		RecoveryPasswordBox = MakeInput(T, LOCTEXT("RecPwHint", "your account password"), true);
		AddV(Col, MakeField(T, LOCTEXT("RecPw", "New recovery code (the old one stops working)"), RecoveryPasswordBox), 8.f);
		NewRecoveryButton = MakeButton(T, LOCTEXT("NewCode", "Make a new recovery code"), PanelRaised);
		AddV(Col, NewRecoveryButton, 0.f);
	}

	// 3. Balance, and the ID to receive GFC
	{
		UVerticalBox* Col = Section(T, Left, LOCTEXT("YourWallet", "Balance"));
		WalletPicker = T->ConstructWidget<UComboBoxString>();
		StyleComboBox(WalletPicker);
		NoWalletText = MakeText(T, LOCTEXT("NoWallet", "You don't have a wallet on this device yet. Create one above."), 12, TextDim);
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

	// 4. Send
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

	// 5. History, last: rows are added to a plain box inside the page's scroll.
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
	if (VerifyPhoneButton) VerifyPhoneButton->OnClicked.AddDynamic(this, &ThisClass::HandleVerifyPhone);
	if (AddWalletButton) AddWalletButton->OnClicked.AddDynamic(this, &ThisClass::HandleAddWallet);
	if (CopyIdButton) CopyIdButton->OnClicked.AddDynamic(this, &ThisClass::HandleCopyId);
	if (RefreshButton) RefreshButton->OnClicked.AddDynamic(this, &ThisClass::HandleRefresh);
	if (ExportButton) ExportButton->OnClicked.AddDynamic(this, &ThisClass::HandleExport);
	if (SendButton) SendButton->OnClicked.AddDynamic(this, &ThisClass::HandleSend);
	if (CopyRecoveryButton) CopyRecoveryButton->OnClicked.AddDynamic(this, &ThisClass::HandleCopyRecovery);
	if (SavedRecoveryButton) SavedRecoveryButton->OnClicked.AddDynamic(this, &ThisClass::HandleSavedRecovery);
	if (VerifyEmailButton) VerifyEmailButton->OnClicked.AddDynamic(this, &ThisClass::HandleVerifyEmail);
	if (NewRecoveryButton) NewRecoveryButton->OnClicked.AddDynamic(this, &ThisClass::HandleNewRecovery);
}

void UGFCWalletScreen::ShowPendingRecoveryCode()
{
	const FString Code = Root.IsValid() ? Root->PendingRecoveryCode : FString();
	if (RecoveryCodeText) RecoveryCodeText->SetText(FText::FromString(Code));
	if (RecoveryCodePanel) RecoveryCodePanel->SetVisibility(Code.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UGFCWalletScreen::HandleCopyRecovery()
{
	if (Root.IsValid() && !Root->PendingRecoveryCode.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Root->PendingRecoveryCode);
		ShowStatus(LOCTEXT("CodeCopied", "Recovery code copied. Paste it somewhere safe, then clear your clipboard."));
	}
}

void UGFCWalletScreen::HandleSavedRecovery()
{
	if (Root.IsValid()) Root->PendingRecoveryCode.Reset();
	ShowPendingRecoveryCode();
	RefreshSecurity();
}

void UGFCWalletScreen::HandleVerifyEmail()
{
	UGFCApiClient* C = Client();
	if (!C) return;
	ShowStatus(LOCTEXT("OpeningEmail", "Opening email verification..."));
	TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
	C->CreateEmailLink([WeakThis](const FGFCApiResult& R, const FGFCVerifyLink& Link)
	{
		if (!WeakThis.IsValid()) return;
		if (!R.bOk)
		{
			WeakThis->ShowStatus(FText::FromString(R.Error), true);
			return;
		}
		FString Error;
		FPlatformProcess::LaunchURL(*Link.Url, nullptr, &Error);
		WeakThis->ShowStatus(Error.IsEmpty()
			? LOCTEXT("EmailOpened", "Verify your email in the browser, then come back to GFC.")
			: LOCTEXT("NoBrowserEmail", "Couldn't open the browser."), !Error.IsEmpty());
	});
}

void UGFCWalletScreen::HandleNewRecovery()
{
	UGFCApiClient* C = Client();
	const FString Password = RecoveryPasswordBox ? RecoveryPasswordBox->GetText().ToString() : FString();
	if (!C) return;
	if (Password.IsEmpty())
	{
		ShowStatus(LOCTEXT("NeedPwCode", "Enter your account password to make a new recovery code."), true);
		return;
	}
	if (NewRecoveryButton) NewRecoveryButton->SetIsEnabled(false);
	TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
	C->NewRecoveryCode(Password, [WeakThis](const FGFCApiResult& R, const FString& Code)
	{
		if (!WeakThis.IsValid()) return;
		if (WeakThis->NewRecoveryButton) WeakThis->NewRecoveryButton->SetIsEnabled(true);
		if (WeakThis->RecoveryPasswordBox) WeakThis->RecoveryPasswordBox->SetText(FText::GetEmpty());
		if (!R.bOk)
		{
			WeakThis->ShowStatus(R.StatusCode == 401 ? LOCTEXT("WrongPwCode", "That password isn't right.") : FText::FromString(R.Error), true);
			return;
		}
		if (WeakThis->Root.IsValid()) WeakThis->Root->PendingRecoveryCode = Code;
		WeakThis->ShowPendingRecoveryCode();
		WeakThis->ShowStatus(LOCTEXT("NewCodeMade", "New recovery code made: write it down. Your old one no longer works."));
	});
}

void UGFCWalletScreen::RefreshSecurity()
{
	UGFCApiClient* C = Client();
	if (!C || !C->IsLoggedIn() || !SecurityStatusText) return;
	TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
	C->GetAccountSecurity([WeakThis](const FGFCApiResult& R, const FGFCAccountSecurity& S)
	{
		if (!WeakThis.IsValid() || !WeakThis->SecurityStatusText) return;
		if (!R.bOk)
		{
			WeakThis->SecurityStatusText->SetText(LOCTEXT("SecurityUnknown", "This node doesn't offer account recovery yet."));
			return;
		}
		const FText Code = S.bRecoveryCode ? LOCTEXT("CodeSet", "Recovery code: made") : LOCTEXT("CodeNone", "Recovery code: none yet, make one below");
		const FText Email = S.bEmailVerified ? FText::Format(LOCTEXT("EmailSet", "Email: {0}"), FText::FromString(S.Email))
		                                     : LOCTEXT("EmailNone", "Email: not verified");
		const FText Phone = S.bPhoneVerified ? LOCTEXT("PhoneSet", "Phone: verified") : LOCTEXT("PhoneNone", "Phone: not verified");
		WeakThis->SecurityStatusText->SetText(FText::Format(LOCTEXT("SecurityLine", "{0}\n{1}\n{2}"), Code, Email, Phone));
	});
}

void UGFCWalletScreen::SetPhoneVerified(bool bVerified)
{
	if (VerifyPanel) VerifyPanel->SetVisibility(bVerified ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UGFCWalletScreen::OnWalletsLearned()
{
	OnWalletsChanged(SelectedWalletId);
}

void UGFCWalletScreen::OnWalletsChanged(const FString& SelectId)
{
	RebuildPicker(SelectId);
	if (Root.IsValid()) Root->RefreshRightRail();  // the "Your GFC" card counts this device's wallets too
}

void UGFCWalletScreen::HandleVerifyPhone()
{
	if (Root.IsValid()) Root->StartPhoneVerification();
}

void UGFCWalletScreen::OnScreenShown()
{
	RebuildPicker(SelectedWalletId);
	if (UGFCApiClient* C = Client(); C && C->IsLoggedIn())
	{
		TWeakObjectPtr<UGFCWalletScreen> WeakThis(this);
		C->GetVerifyStatus([WeakThis](const FGFCApiResult& R, const FGFCVerifyStatus& S)
		{
			if (!WeakThis.IsValid() || !R.bOk) return;
			WeakThis->SetPhoneVerified(S.bVerified);
			if (UGFCApiSubsystem* A = WeakThis->GetGameInstance()->GetSubsystem<UGFCApiSubsystem>(); A && A->LearnWallets(S.Wallets) > 0)
			{
				WeakThis->OnWalletsLearned();
			}
		});
	}
	if (BackupResultText) BackupResultText->SetText(FText::GetEmpty());
	ShowPendingRecoveryCode();
	RefreshSecurity();
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
		if (UGFCApiSubsystem* A = WeakThis->GetGameInstance()->GetSubsystem<UGFCApiSubsystem>())
		{
			// Seen here, so it isn't announced again at the next login. A rise while the user
			// wasn't looking (rewards, a bonus) still gets the burst.
			int64 LastSeen = -1;
			for (const FGFCKnownWallet& K : A->GetKnownWallets())
			{
				if (K.WalletId == Id) LastSeen = K.LastSeenCents;
			}
			const int64 Earned = UGFCApiSubsystem::EarnedSince(LastSeen, W.BalanceCents);
			A->NoteBalance(Id, W.BalanceCents);
			if (Earned > 0)
			{
				UGFCRewardBurst::Play(WeakThis.Get(), double(Earned) / 100.0, LOCTEXT("EarnedHere", "You earned GFC!"));
			}
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
		WeakThis->OnWalletsChanged(W.Id);
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
		WeakThis->OnWalletsChanged(W.Id);
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
