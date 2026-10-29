#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCApiTypes.h"
#include "Types/SlateEnums.h"
#include "GFCBridgeScreen.generated.h"

class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UWidget;

/**
 * Bridge to Base: moves GFC from a ledger wallet to the ERC-20 on Base (or another chain the
 * node bridges to). See docs/ui-spec.md#bridge-to-base.
 *
 * The node signs the escrow deposit; the app never builds the reason itself. Only the part of
 * the balance that isn't fresh free credit can go (GET /bridge/quote), and bridging is a two-step
 * Review / Confirm because it can't be undone. Tokens are minted later, after the bridge's
 * finality wait and attester checks.
 */
UCLASS()
class GFCCLIENT_API UGFCBridgeScreen : public UGFCScreenBase
{
	GENERATED_BODY()

public:
	virtual void OnScreenShown() override;

	/** The wallet to bridge from (the one selected on the wallet screen). */
	void SetWallet(const FString& WalletId);

protected:
	virtual void BuildDefaultLayout() override;
	virtual void BindEvents() override;

	/** Shown instead of the form when the node doesn't bridge (yet). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> UnavailableText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UWidget> FormPanel;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NoticeText;
	/** Hidden when the node bridges to a single chain. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UComboBoxString> ChainPicker;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UComboBoxString> WalletPicker;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UTextBlock> BridgeableText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HeldText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> AddressBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> AmountBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> MaxButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RulesText;
	/** The review: exactly what will happen, shown before the second tap. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SummaryText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> BridgeButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BridgeButtonLabel;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> CancelButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> BackButton;

private:
	UFUNCTION() void HandleChainChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleWalletChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleInputChanged(const FText& Text);
	UFUNCTION() void HandleMax();
	UFUNCTION() void HandleBridge();
	UFUNCTION() void HandleCancel();
	UFUNCTION() void HandleBack();

	void LoadInfo();
	void LoadQuote();
	void ShowInfo();
	void RebuildWalletPicker();
	/** Checks the form; on success fills the amount, and Error stays empty. */
	bool Validate(int64& OutCents, FText& OutError) const;
	void SetReviewing(bool bReview);
	void SetBusy(bool bBusy);
	const FGFCBridgeChain* SelectedChain() const;

	FGFCBridgeInfo Info;
	bool bInfoLoaded = false;
	FGFCBridgeQuote Quote;
	bool bQuoteLoaded = false;
	int32 ChainIndex = 0;
	TArray<FString> PickerWalletIds;
	FString SelectedWalletId;
	bool bReviewing = false;
	bool bBusy = false;
	bool bRebuilding = false;
};
