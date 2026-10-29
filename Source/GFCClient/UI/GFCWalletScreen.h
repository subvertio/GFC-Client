#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCApiTypes.h"
#include "Types/SlateEnums.h"
#include "GFCWalletScreen.generated.h"

class UButton;
class UComboBoxString;
class UEditableTextBox;
class UPanelWidget;
class UTextBlock;
class UWidget;

/**
 * Balance, send GFC, history, and the wallet backup prompt. See docs/ui-spec.md#wallet.
 *
 * The node has no "list my wallets" endpoint, so the screen shows wallets this
 * device created or was told about ("Add wallet by ID"), stored per user and node.
 */
UCLASS()
class GFCCLIENT_API UGFCWalletScreen : public UGFCScreenBase
{
	GENERATED_BODY()

public:
	virtual void OnScreenShown() override;

	/** Shows or hides the "Verify your phone" card. */
	void SetPhoneVerified(bool bVerified);

	/** Wallets were learned from the node: rebuild the list and show the balance. */
	void OnWalletsLearned();

	UFUNCTION(BlueprintCallable, Category = "GFC")
	void RefreshSelected();

	UFUNCTION(BlueprintPure, Category = "GFC")
	FString GetSelectedWalletId() const { return SelectedWalletId; }

protected:
	virtual void BuildDefaultLayout() override;
	virtual void BindEvents() override;

	// Wallet picker and balance
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UComboBoxString> WalletPicker;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> BalanceText;
	/** Rewards need a verified phone: this card opens the node's verification page. Hidden once verified. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UWidget> VerifyPanel;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> VerifyPhoneButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WalletIdText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CopyIdButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> RefreshButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NoWalletText;

	// Create / add
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> CreateWalletButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> NewWalletNameBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> AddWalletIdBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> AddWalletButton;

	// Backup prompt
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UWidget> BackupPanel;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> BackupPasswordBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> BackupPassphraseBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> BackupConfirmBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> ExportButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BackupTitleText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BackupResultText;

	// Send
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> RecipientBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> AmountBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> ReasonBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> SendButton;

	// History: rows are added as children of this panel.
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UPanelWidget> HistoryList;

private:
	UFUNCTION() void HandlePickerChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleCreateWallet();
	UFUNCTION() void HandleVerifyPhone();
	UFUNCTION() void HandleAddWallet();
	UFUNCTION() void HandleCopyId();
	UFUNCTION() void HandleRefresh();
	UFUNCTION() void HandleExport();
	UFUNCTION() void HandleSend();

	void RebuildPicker(const FString& SelectId);
	/** This device's wallets changed (created, added, learned from the node): the picker and the right rail. */
	void OnWalletsChanged(const FString& SelectId);
	void UpdateBackupPrompt();
	void ShowHistory(const TArray<FGFCTransaction>& Transactions);
	void SetSendBusy(bool bBusy);

	/** Picker labels map to wallet ids in the same order. */
	TArray<FString> PickerWalletIds;
	FString SelectedWalletId;
	bool bRebuildingPicker = false;
};
