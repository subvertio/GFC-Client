#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetTree;

/**
 * Colours and widget factories for the code-built default UI. A designer's
 * Blueprint screens don't use any of this; it only exists so milestone 1 runs
 * without editor-made assets.
 */
namespace GFCUI
{
	inline const FLinearColor Background(0.010f, 0.014f, 0.030f);
	inline const FLinearColor Panel(0.030f, 0.040f, 0.075f);
	inline const FLinearColor PanelRaised(0.050f, 0.064f, 0.115f);
	inline const FLinearColor Accent(0.020f, 0.560f, 0.910f);
	inline const FLinearColor Warning(0.910f, 0.380f, 0.020f);
	inline const FLinearColor Danger(0.800f, 0.090f, 0.070f);
	inline const FLinearColor Success(0.110f, 0.650f, 0.250f);
	inline const FLinearColor Text(0.900f, 0.920f, 0.960f);
	inline const FLinearColor TextDim(0.450f, 0.490f, 0.560f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Content, int32 Size = 14, const FLinearColor& Color = Text, bool bBold = false);
	UButton* MakeButton(UWidgetTree* Tree, const FText& Label, const FLinearColor& Color = Accent, UTextBlock** OutLabel = nullptr);
	UEditableTextBox* MakeInput(UWidgetTree* Tree, const FText& Hint, bool bPassword = false);
	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color = Panel, const FMargin& Padding = FMargin(16.f));
	UVerticalBox* MakeColumn(UWidgetTree* Tree);
	UHorizontalBox* MakeRow(UWidgetTree* Tree);

	/** Adds to a vertical box with spacing below; bFill makes the child take the remaining height. */
	void AddV(UVerticalBox* Box, UWidget* Child, float Spacing = 8.f, bool bFill = false);
	/** Adds to a horizontal box with spacing to the right; bFill makes the child take the remaining width. */
	void AddH(UHorizontalBox* Box, UWidget* Child, float Spacing = 8.f, bool bFill = false);

	/** A labelled input: "Label" above the box. */
	UVerticalBox* MakeField(UWidgetTree* Tree, const FText& Label, UEditableTextBox* Input);

	/** Wraps a widget so it is at least Height dp tall (touch targets). */
	USizeBox* MinHeight(UWidgetTree* Tree, UWidget* Child, float Height = 48.f);

	/** Shortens a wallet id or hash for display: "abcd1234…9f0e". */
	FString ShortId(const FString& Id);
}
