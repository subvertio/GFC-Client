#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class UBorder;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UHorizontalBox;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetTree;

/**
 * The app's only theme, shared by the phone and desktop layouts: pure black (easy on
 * the eyes, and black pixels are off on the OLED screens most phones have, which saves
 * battery) with a sky-blue accent. Every colour the code-built UI uses comes from here;
 * tune them in this one place. Values are sRGB hex, as a designer would give them.
 * See docs/ui-spec.md.
 */
struct GFCCLIENT_API FGFCTheme
{
	FLinearColor Background;      // #000000  app background
	FLinearColor Surface;         // #16181C  raised surfaces and cards
	FLinearColor SurfacePressed;  // #1D1F23  hover and pressed states
	FLinearColor Border;          // #2F3336  dividers and borders
	FLinearColor Text;            // #E7E9EA  primary text
	FLinearColor TextSecondary;   // #71767B  secondary text
	FLinearColor Accent;          // #1D9BF0  primary actions, links, the active tab
	FLinearColor AccentPressed;   // #1A8CD8  primary buttons when hovered or pressed
	FLinearColor AccentText;      // #1D9BF0  accent-coloured text (readable on black as it is)
	FLinearColor AccentTint;      // Accent at 10%: hover behind accent-coloured icons
	FLinearColor Success;         // #00BA7C  success, rewards, verified
	FLinearColor Danger;          // #F4212E  errors
	FLinearColor Warning;         // #FFAD1F  the wallet backup prompt
	FLinearColor Gold;            // #FFC83D  GFC coins and earnings
	FLinearColor GoldDark;        // #B07A12  a coin's rim and stamp
	FLinearColor Like;            // #F91880  a liked post's heart
	FLinearColor SearchFill;      // #202327  the rounded search box
	FLinearColor Exclusive;       // #FFD400  GFC Exclusives (the web's badge yellow)

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
	/** The wide-window layout's sizes, in dp. Below Breakpoint the app uses the phone layout. */
	namespace Desktop
	{
		constexpr float Breakpoint = 1000.f;     // window width for the three-column layout
		constexpr float RightRailFrom = 1240.f;  // ...and room for the right rail too
		constexpr float LeftRailWidth = 260.f;
		constexpr float CenterWidth = 600.f;
		constexpr float RightRailWidth = 350.f;
		constexpr float Gutter = 24.f;           // between the columns
	}

	/**
	 * Icon glyphs (Unicode, drawn with the engine's fonts: no icon assets or third-party
	 * icon sets). Swap them here for an icon font later. Only glyphs the engine's Roboto
	 * and DroidSansFallback actually draw: many symbols (a house, a smiley) come out as boxes.
	 */
	namespace Icon
	{
		inline const TCHAR* Home = TEXT("✦");       // four-pointed star: your feed
		inline const TCHAR* Explore = TEXT("#");
		inline const TCHAR* Bookmark = TEXT("☆");   // outline star
		inline const TCHAR* Bookmarked = TEXT("★");
		inline const TCHAR* Wallet = TEXT("¤");     // currency sign: GFC
		inline const TCHAR* Profile = TEXT("@");
		inline const TCHAR* Reply = TEXT("↩");
		inline const TCHAR* Like = TEXT("♡");
		inline const TCHAR* Liked = TEXT("♥");
		inline const TCHAR* Views = TEXT("⊙");      // circled dot: an eye
		inline const TCHAR* Share = TEXT("↗");
		inline const TCHAR* Media = TEXT("⊕");
		inline const TCHAR* Model3D = TEXT("✧");
		inline const TCHAR* More = TEXT("•••");
	}

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
	/** Transparent until hovered, then a rounded Hover fill: nav items, icon buttons. */
	FButtonStyle GhostButtonStyle(const FLinearColor& Hover, float Radius = 24.f);
	/** Small dim text that's tappable on its own (e.g. "Edited" after a post's time). */
	UButton* MakeTextLink(UWidgetTree* Tree, const FText& Label, int32 Size = 14);
	/** "★ Exclusive to the GFC Network" in the Exclusive yellow, on a pill. */
	UWidget* MakeExclusiveBadge(UWidgetTree* Tree);
	/** A yellow text button for "Log in / Verify your phone to watch the full clip"; set the text through OutLabel. */
	UButton* MakeExclusiveLink(UWidgetTree* Tree, UTextBlock** OutLabel);

	/** The GFC logo: "GFC" in white on an accent circle (the same mark as the Windows app icon). */
	UWidget* MakeLogo(UWidgetTree* Tree, float Size = 40.f);
	/** A rounded search box (SearchFill, accent outline when focused). */
	UEditableTextBox* MakeSearchBox(UWidgetTree* Tree, const FText& Hint);

	UEditableTextBox* MakeInput(UWidgetTree* Tree, const FText& Hint, bool bPassword = false);
	void StyleComboBox(UComboBoxString* Combo);
	/** A dark multi-line text box for writing posts. */
	UMultiLineEditableTextBox* MakeMultiLineInput(UWidgetTree* Tree, const FText& Hint);

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
