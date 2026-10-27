#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class UBorder;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UHorizontalBox;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetTree;

/**
 * The app's only theme: pure black (easy on the eyes, and black pixels are off
 * on the OLED screens most phones have, which saves battery). Every colour the
 * code-built UI uses comes from here; tune them in this one place.
 * Values are sRGB hex, as a designer would give them. See docs/ui-spec.md.
 */
struct GFCCLIENT_API FGFCTheme
{
	FLinearColor Background;      // #000000  app background
	FLinearColor Surface;         // #16181C  raised surfaces and cards
	FLinearColor SurfacePressed;  // #1D1F23  hover and pressed states
	FLinearColor Border;          // #2F3336  dividers and borders
	FLinearColor Text;            // #E7E9EA  primary text
	FLinearColor TextSecondary;   // #71767B  secondary text
	FLinearColor Accent;          // #3B4CCA  GFC accent (the share pages' colour), primary actions
	FLinearColor AccentPressed;   // #2F3DA3
	FLinearColor AccentText;      // Accent lightened for text on black (derived; #3B4CCA alone is too dark to read)
	FLinearColor Success;         // #00BA7C  success, rewards, verified
	FLinearColor Danger;          // #F4212E  errors
	FLinearColor Warning;         // #FFAD1F  the wallet backup prompt

	float CornerRadius = 16.f;    // cards
	float PillRadius = 24.f;      // buttons and inputs

	static const FGFCTheme& Get();
};

/**
 * Widget factories for the code-built default UI. Designer Blueprints don't need
 * them; they exist so milestone 1 runs without editor-made assets.
 */
namespace GFCUI
{
	// Short aliases into FGFCTheme, used throughout the screens.
	inline const FLinearColor& Background = FGFCTheme::Get().Background;
	inline const FLinearColor& Panel = FGFCTheme::Get().Surface;
	inline const FLinearColor& PanelRaised = FGFCTheme::Get().Surface;
	inline const FLinearColor& Accent = FGFCTheme::Get().Accent;
	inline const FLinearColor& AccentText = FGFCTheme::Get().AccentText;
	inline const FLinearColor& Warning = FGFCTheme::Get().Warning;
	inline const FLinearColor& Danger = FGFCTheme::Get().Danger;
	inline const FLinearColor& Success = FGFCTheme::Get().Success;
	inline const FLinearColor& Text = FGFCTheme::Get().Text;
	inline const FLinearColor& TextDim = FGFCTheme::Get().TextSecondary;
	inline const FLinearColor& BorderColor = FGFCTheme::Get().Border;

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Content, int32 Size = 14, const FLinearColor& Color = Text, bool bBold = false);

	/**
	 * A pill button. Color picks the kind: Accent = primary (filled), Warning or
	 * Danger = filled in that colour, anything else = secondary (outlined).
	 */
	UButton* MakeButton(UWidgetTree* Tree, const FText& Label, const FLinearColor& Color = Accent, UTextBlock** OutLabel = nullptr);

	/** A borderless button for whole rows and cards: black, #1D1F23 when pressed. */
	FButtonStyle FlatButtonStyle();

	UEditableTextBox* MakeInput(UWidgetTree* Tree, const FText& Hint, bool bPassword = false);
	void StyleComboBox(UComboBoxString* Combo);

	/** A flat area (no corners), e.g. the page background. */
	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color = Panel, const FMargin& Padding = FMargin(16.f));
	/** A rounded card on Surface with a Border outline; Outline overrides the outline colour. */
	UBorder* MakeCard(UWidgetTree* Tree, const FMargin& Padding = FMargin(16.f), const FLinearColor* Outline = nullptr);
	/** A 1 dp horizontal divider. */
	UWidget* MakeDivider(UWidgetTree* Tree);
	/** A round avatar with the first letter of Name (no image download needed). */
	UWidget* MakeAvatar(UWidgetTree* Tree, const FString& Name, float Size = 40.f);

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

	/** "now", "5m", "3h", "2d", or a date for older timestamps (node ISO 8601, UTC). */
	FText RelativeTime(const FString& IsoTimestamp);
}
