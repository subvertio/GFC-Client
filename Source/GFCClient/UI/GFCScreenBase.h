#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCScreenBase.generated.h"

class UGFCApiClient;
class UGFCApiSubsystem;
class UGFCRootWidget;

/**
 * Base for every GFC screen.
 *
 * Designer seam: each screen declares its parts as BindWidget properties. A
 * Widget Blueprint that subclasses the screen and contains widgets with those
 * names replaces the code-built layout entirely. With no Blueprint (milestone 1),
 * BuildDefaultLayout() constructs the same named parts in code.
 */
UCLASS(Abstract)
class GFCCLIENT_API UGFCScreenBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRoot(UGFCRootWidget* InRoot);

	/** Called each time the screen becomes the active one. */
	virtual void OnScreenShown();

protected:
	virtual void NativeOnInitialized() override;

	/** Builds the default code layout. Only runs when no Blueprint supplied a widget tree. */
	virtual void BuildDefaultLayout() {}

	/** Hooks up events on the bound parts (whether built in code or by a Blueprint). */
	virtual void BindEvents() {}

	/** Blueprint hook, fired after OnScreenShown. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GFC", meta = (DisplayName = "On Screen Shown"))
	void K2_OnScreenShown();

	UGFCApiSubsystem* Api() const;
	UGFCApiClient* Client() const;
	void ShowStatus(const FText& Message, bool bError = false) const;

	TWeakObjectPtr<UGFCRootWidget> Root;
};
