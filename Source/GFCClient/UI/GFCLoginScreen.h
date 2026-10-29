#pragma once

#include "CoreMinimal.h"
#include "UI/GFCScreenBase.h"
#include "GFCLoginScreen.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/** Log in or register. See docs/ui-spec.md#login. */
UCLASS()
class GFCCLIENT_API UGFCLoginScreen : public UGFCScreenBase
{
	GENERATED_BODY()

public:
	virtual void OnScreenShown() override;

protected:
	virtual void BuildDefaultLayout() override;
	virtual void BindEvents() override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> UsernameBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UEditableTextBox> PasswordBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> LoginButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UButton> RegisterButton;
	/** Opens the node's /reset page (phone or email, plus the recovery code) in the browser. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> ForgotButton;
	/** Optional e-mail for registering. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> EmailBox;
	/** Optional node picker. */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> NodeUrlBox;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> UseNodeButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NodeText;
	/** Node presets: the live network, and a test node on the PC reached over USB (adb reverse tcp:8100 tcp:8100). */
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LiveNodeButton;
	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidgetOptional)) TObjectPtr<UButton> LocalNodeButton;

private:
	UFUNCTION() void HandleLogin();
	UFUNCTION() void HandleRegister();
	UFUNCTION() void HandleForgot();
	UFUNCTION() void HandleUseNode();
	UFUNCTION() void HandleLiveNode();
	UFUNCTION() void HandleLocalNode();
	void SwitchNode(const FString& Url);
	UFUNCTION() void HandlePasswordCommitted(const FText& Text, ETextCommit::Type Method);

	void SetBusy(bool bBusy);
	bool ReadCredentials(FString& OutUser, FString& OutPassword) const;
	void RefreshNodeText();
};
