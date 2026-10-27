#include "UI/GFCUIStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Misc/DateTime.h"

#define LOCTEXT_NAMESPACE "GFCUI"

namespace
{
	FLinearColor Hex(const TCHAR* H)
	{
		return FLinearColor::FromSRGBColor(FColor::FromHex(H));
	}
}

const FGFCTheme& FGFCTheme::Get()
{
	static const FGFCTheme Theme = []()
	{
		FGFCTheme T;
		T.Background = Hex(TEXT("000000"));
		T.Surface = Hex(TEXT("16181C"));
		T.SurfacePressed = Hex(TEXT("1D1F23"));
		T.Border = Hex(TEXT("2F3336"));
		T.Text = Hex(TEXT("E7E9EA"));
		T.TextSecondary = Hex(TEXT("71767B"));
		T.Accent = Hex(TEXT("1D9BF0"));
		T.AccentPressed = Hex(TEXT("1A8CD8"));
		T.AccentText = T.Accent;
		T.AccentTint = T.Accent.CopyWithNewOpacity(0.1f);
		T.Success = Hex(TEXT("00BA7C"));
		T.Danger = Hex(TEXT("F4212E"));
		T.Warning = Hex(TEXT("FFAD1F"));
		T.Gold = Hex(TEXT("FFC83D"));
		T.GoldDark = Hex(TEXT("B07A12"));
		T.Like = Hex(TEXT("F91880"));
		T.SearchFill = Hex(TEXT("202327"));
		return T;
	}();
	return Theme;
}

namespace GFCUI
{
	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Content, int32 Size, const FLinearColor& Color, bool bBold)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(Content);
		FSlateFontInfo Font = T->GetFont();
		Font.Size = Size;
		Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		T->SetFont(Font);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetAutoWrapText(true);
		return T;
	}

	static FButtonStyle PillStyle(const FLinearColor& Fill, const FLinearColor& Pressed, const FLinearColor& Outline, float OutlineWidth)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		FButtonStyle S;
		S.SetNormal(FSlateRoundedBoxBrush(Fill, Th.PillRadius, Outline, OutlineWidth));
		S.SetHovered(FSlateRoundedBoxBrush(Pressed, Th.PillRadius, Outline, OutlineWidth));
		S.SetPressed(FSlateRoundedBoxBrush(Pressed, Th.PillRadius, Outline, OutlineWidth));
		S.SetDisabled(FSlateRoundedBoxBrush(Fill.CopyWithNewOpacity(0.45f), Th.PillRadius, Outline.CopyWithNewOpacity(0.45f), OutlineWidth));
		S.SetNormalPadding(FMargin(0.f));
		S.SetPressedPadding(FMargin(0.f));
		return S;
	}

	FButtonStyle FlatButtonStyle()
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		FButtonStyle S;
		S.SetNormal(FSlateColorBrush(Th.Background));
		S.SetHovered(FSlateColorBrush(Th.SurfacePressed));
		S.SetPressed(FSlateColorBrush(Th.SurfacePressed));
		S.SetDisabled(FSlateColorBrush(Th.Background));
		S.SetNormalPadding(FMargin(0.f));
		S.SetPressedPadding(FMargin(0.f));
		return S;
	}

	FButtonStyle GhostButtonStyle(const FLinearColor& Hover, float Radius)
	{
		FButtonStyle S;
		S.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, Radius));
		S.SetHovered(FSlateRoundedBoxBrush(Hover, Radius));
		S.SetPressed(FSlateRoundedBoxBrush(Hover, Radius));
		S.SetDisabled(FSlateRoundedBoxBrush(FLinearColor::Transparent, Radius));
		S.SetNormalPadding(FMargin(0.f));
		S.SetPressedPadding(FMargin(0.f));
		return S;
	}

	UWidget* MakeLogo(UWidgetTree* Tree, float Size)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
		UBorder* Circle = Tree->ConstructWidget<UBorder>();
		Circle->SetBrush(FSlateRoundedBoxBrush(Th.Accent, Size * 0.5f));
		Circle->SetHorizontalAlignment(HAlign_Center);
		Circle->SetVerticalAlignment(VAlign_Center);
		Circle->SetPadding(FMargin(0.f));
		UTextBlock* Mark = MakeText(Tree, LOCTEXT("LogoMark", "GFC"), FMath::Max(8, int32(Size * 0.3f)), FLinearColor::White, true);
		Mark->SetAutoWrapText(false);
		Circle->AddChild(Mark);
		Box->AddChild(Circle);
		return Box;
	}

	UEditableTextBox* MakeSearchBox(UWidgetTree* Tree, const FText& Hint)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		UEditableTextBox* E = MakeInput(Tree, Hint);
		FEditableTextBoxStyle Style = E->GetWidgetStyle();
		FSlateFontInfo Font = Style.TextStyle.Font;
		Font.Size = 15;
		Style.SetFont(Font);
		Style.SetPadding(FMargin(20.f, 11.f));
		Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Th.SearchFill, Th.PillRadius));
		Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Th.SearchFill, Th.PillRadius));
		Style.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Th.Background, Th.PillRadius, Th.Accent, 1.f));
		E->SetWidgetStyle(Style);
		return E;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FText& Label, const FLinearColor& Color, UTextBlock** OutLabel)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		UButton* B = Tree->ConstructWidget<UButton>();
		FLinearColor LabelColor = Th.Text;
		if (Color == Th.Accent)
		{
			B->SetStyle(PillStyle(Th.Accent, Th.AccentPressed, Th.Accent, 0.f));
		}
		else if (Color == Th.Warning || Color == Th.Danger)
		{
			B->SetStyle(PillStyle(Color, Color * 0.8f, Color, 0.f));
			LabelColor = Th.Background;
		}
		else
		{
			B->SetStyle(PillStyle(Th.Background, Th.SurfacePressed, Th.Border, 1.f));
		}
		B->SetBackgroundColor(FLinearColor::White); // the style carries the colours
		// Touch: a drag that starts on a button scrolls its list instead of clicking.
		B->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		B->SetClickMethod(EButtonClickMethod::PreciseClick);
		UTextBlock* T = MakeText(Tree, Label, 15, LabelColor, true);
		T->SetAutoWrapText(false);
		T->SetJustification(ETextJustify::Center);
		B->AddChild(T);
		if (UButtonSlot* S = Cast<UButtonSlot>(T->Slot))
		{
			S->SetPadding(FMargin(18.f, 13.f)); // >= 48 dp tall with the label
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (OutLabel)
		{
			*OutLabel = T;
		}
		return B;
	}

	UEditableTextBox* MakeInput(UWidgetTree* Tree, const FText& Hint, bool bPassword)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		UEditableTextBox* E = Tree->ConstructWidget<UEditableTextBox>();
		E->SetHintText(Hint);
		E->SetIsPassword(bPassword);
		FEditableTextBoxStyle Style = E->GetWidgetStyle();
		FSlateFontInfo Font = Style.TextStyle.Font;
		Font.Size = 16; // 16 so phones don't zoom in
		Style.SetFont(Font);
		Style.SetPadding(FMargin(14.f, 12.f));
		Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Th.Background, 8.f, Th.Border, 1.f));
		Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Th.Background, 8.f, Th.TextSecondary, 1.f));
		Style.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Th.Background, 8.f, Th.Accent, 2.f));
		Style.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Th.Surface, 8.f, Th.Border, 1.f));
		Style.SetForegroundColor(FSlateColor(Th.Text));
		Style.SetFocusedForegroundColor(FSlateColor(Th.Text));
		Style.SetBackgroundColor(FSlateColor(FLinearColor::White));
		E->SetWidgetStyle(Style);
		return E;
	}

	UMultiLineEditableTextBox* MakeMultiLineInput(UWidgetTree* Tree, const FText& Hint)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		UMultiLineEditableTextBox* E = Tree->ConstructWidget<UMultiLineEditableTextBox>();
		E->SetHintText(Hint);
		FEditableTextBoxStyle Style = E->WidgetStyle;
		FSlateFontInfo Font = Style.TextStyle.Font;
		Font.Size = 17;
		Style.SetFont(Font);
		Style.SetPadding(FMargin(14.f, 12.f));
		Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Th.Background, 8.f, Th.Border, 1.f));
		Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Th.Background, 8.f, Th.TextSecondary, 1.f));
		Style.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Th.Background, 8.f, Th.Accent, 2.f));
		Style.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Th.Surface, 8.f, Th.Border, 1.f));
		Style.SetForegroundColor(FSlateColor(Th.Text));
		Style.SetFocusedForegroundColor(FSlateColor(Th.Text));
		Style.SetBackgroundColor(FSlateColor(FLinearColor::White));
		E->WidgetStyle = Style;
		E->SetAutoWrapText(true);
		return E;
	}

	void StyleComboBox(UComboBoxString* Combo)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		FComboBoxStyle Style = Combo->GetWidgetStyle();
		FComboButtonStyle CB = Style.ComboButtonStyle;
		FButtonStyle BS = CB.ButtonStyle;
		BS.SetNormal(FSlateRoundedBoxBrush(Th.Background, 8.f, Th.Border, 1.f));
		BS.SetHovered(FSlateRoundedBoxBrush(Th.SurfacePressed, 8.f, Th.Border, 1.f));
		BS.SetPressed(FSlateRoundedBoxBrush(Th.SurfacePressed, 8.f, Th.Border, 1.f));
		CB.SetButtonStyle(BS);
		CB.SetMenuBorderBrush(FSlateRoundedBoxBrush(Th.Surface, 8.f, Th.Border, 1.f));
		Style.SetComboButtonStyle(CB);
		Combo->SetWidgetStyle(Style);

		FTableRowStyle Rows = Combo->GetItemStyle();
		Rows.SetEvenRowBackgroundBrush(FSlateColorBrush(Th.Surface));
		Rows.SetOddRowBackgroundBrush(FSlateColorBrush(Th.Surface));
		Rows.SetActiveBrush(FSlateColorBrush(Th.SurfacePressed));
		Rows.SetActiveHoveredBrush(FSlateColorBrush(Th.SurfacePressed));
		Rows.SetInactiveHoveredBrush(FSlateColorBrush(Th.SurfacePressed));
		Rows.SetTextColor(FSlateColor(Th.Text));
		Rows.SetSelectedTextColor(FSlateColor(Th.Text));
		Combo->SetItemStyle(Rows);
		// Only read when the Slate widget is built, which hasn't happened yet for a freshly constructed combo.
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Combo->ForegroundColor = FSlateColor(Th.Text);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>();
		B->SetBrush(FSlateColorBrush(Color));
		B->SetPadding(Padding);
		return B;
	}

	UBorder* MakeCard(UWidgetTree* Tree, const FMargin& Padding, const FLinearColor* Outline)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		UBorder* B = Tree->ConstructWidget<UBorder>();
		B->SetBrush(FSlateRoundedBoxBrush(Th.Surface, Th.CornerRadius, Outline ? *Outline : Th.Border, Outline ? 2.f : 1.f));
		B->SetPadding(Padding);
		return B;
	}

	UWidget* MakeDivider(UWidgetTree* Tree)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetHeightOverride(1.f);
		UBorder* Line = MakePanel(Tree, FGFCTheme::Get().Border, FMargin(0.f));
		Box->AddChild(Line);
		return Box;
	}

	UWidget* MakeAvatar(UWidgetTree* Tree, const FString& Name, float Size)
	{
		const FGFCTheme& Th = FGFCTheme::Get();
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
		UBorder* Circle = Tree->ConstructWidget<UBorder>();
		Circle->SetBrush(FSlateRoundedBoxBrush(Th.SurfacePressed, Size * 0.5f, Th.Border, 1.f));
		Circle->SetHorizontalAlignment(HAlign_Center);
		Circle->SetVerticalAlignment(VAlign_Center);
		const FString Initial = Name.IsEmpty() ? FString(TEXT("?")) : Name.Left(1).ToUpper();
		UTextBlock* Letter = MakeText(Tree, FText::FromString(Initial), int32(Size * 0.42f), Th.Text, true);
		Letter->SetAutoWrapText(false);
		Circle->AddChild(Letter);
		Box->AddChild(Circle);
		return Box;
	}

	UVerticalBox* MakeColumn(UWidgetTree* Tree)
	{
		return Tree->ConstructWidget<UVerticalBox>();
	}

	UHorizontalBox* MakeRow(UWidgetTree* Tree)
	{
		return Tree->ConstructWidget<UHorizontalBox>();
	}

	void AddV(UVerticalBox* Box, UWidget* Child, float Spacing, bool bFill)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Child);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, Spacing));
		if (bFill)
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	void AddH(UHorizontalBox* Box, UWidget* Child, float Spacing, bool bFill)
	{
		UHorizontalBoxSlot* S = Box->AddChildToHorizontalBox(Child);
		S->SetPadding(FMargin(0.f, 0.f, Spacing, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
		if (bFill)
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	UVerticalBox* MakeField(UWidgetTree* Tree, const FText& Label, UEditableTextBox* Input)
	{
		UVerticalBox* Col = MakeColumn(Tree);
		AddV(Col, MakeText(Tree, Label, 12, TextDim), 4.f);
		AddV(Col, Input, 0.f);
		return Col;
	}

	USizeBox* MinHeight(UWidgetTree* Tree, UWidget* Child, float Height)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetMinDesiredHeight(Height);
		Box->AddChild(Child);
		return Box;
	}

	FString ShortId(const FString& Id)
	{
		return Id.Len() <= 16 ? Id : Id.Left(8) + TEXT("…") + Id.Right(6);
	}

	FText RelativeTime(const FString& IsoTimestamp)
	{
		FDateTime When;
		FString Iso = IsoTimestamp;
		if (!Iso.IsEmpty() && !Iso.EndsWith(TEXT("Z")) && !Iso.Contains(TEXT("+")))
		{
			Iso += TEXT("Z"); // the node writes UTC without a zone
		}
		if (!FDateTime::ParseIso8601(*Iso, When))
		{
			return FText::GetEmpty();
		}
		const FTimespan Age = FDateTime::UtcNow() - When;
		if (Age.GetTotalMinutes() < 1) return LOCTEXT("Now", "now");
		if (Age.GetTotalHours() < 1) return FText::Format(LOCTEXT("Min", "{0}m"), FText::AsNumber(int32(Age.GetTotalMinutes())));
		if (Age.GetTotalDays() < 1) return FText::Format(LOCTEXT("Hour", "{0}h"), FText::AsNumber(int32(Age.GetTotalHours())));
		if (Age.GetTotalDays() < 7) return FText::Format(LOCTEXT("Day", "{0}d"), FText::AsNumber(int32(Age.GetTotalDays())));
		return FText::AsDate(When, EDateTimeStyle::Medium, TEXT("UTC"));
	}
}

#undef LOCTEXT_NAMESPACE
