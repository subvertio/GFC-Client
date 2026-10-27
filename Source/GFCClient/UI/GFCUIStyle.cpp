#include "UI/GFCUIStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

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

	UButton* MakeButton(UWidgetTree* Tree, const FText& Label, const FLinearColor& Color, UTextBlock** OutLabel)
	{
		UButton* B = Tree->ConstructWidget<UButton>();
		B->SetBackgroundColor(Color);
		// Touch: a drag that starts on a button scrolls its list instead of clicking.
		B->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		B->SetClickMethod(EButtonClickMethod::PreciseClick);
		UTextBlock* T = MakeText(Tree, Label, 15, FLinearColor::White, true);
		T->SetAutoWrapText(false);
		T->SetJustification(ETextJustify::Center);
		B->AddChild(T);
		if (UButtonSlot* S = Cast<UButtonSlot>(T->Slot))
		{
			S->SetPadding(FMargin(16.f, 13.f)); // >= 48 dp tall with the label
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
		UEditableTextBox* E = Tree->ConstructWidget<UEditableTextBox>();
		E->SetHintText(Hint);
		E->SetIsPassword(bPassword);
		FEditableTextBoxStyle Style = E->GetWidgetStyle();
		FSlateFontInfo Font = Style.TextStyle.Font;
		Font.Size = 16;
		Style.SetFont(Font);
		Style.SetPadding(FMargin(12.f, 12.f));
		E->SetWidgetStyle(Style);
		return E;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>();
		B->SetBrushColor(Color);
		B->SetPadding(Padding);
		return B;
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
		AddV(Col, MakeText(Tree, Label, 11, TextDim), 2.f);
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
}
