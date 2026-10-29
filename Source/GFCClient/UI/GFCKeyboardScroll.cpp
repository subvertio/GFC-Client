#include "UI/GFCKeyboardScroll.h"

#include "Blueprint/WidgetTree.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableText.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Layout/Geometry.h"

namespace
{
	/** Room between the field and the keyboard (or the scroll box's edge), in slate units. */
	constexpr float KeyboardGap = 16.f;
	constexpr int32 SettleFrames = 4;

	bool IsTextInput(const UWidget* W)
	{
		return W && (W->IsA<UEditableTextBox>() || W->IsA<UMultiLineEditableTextBox>()
			|| W->IsA<UEditableText>() || W->IsA<UMultiLineEditableText>());
	}
}

void FGFCKeyboardScroll::Tick(UWidgetTree* Tree, const FGeometry& MyGeometry)
{
	UWidget* Focused = FindFocusedInput(Tree);
	const float Height = MyGeometry.GetLocalSize().Y;
	if (Focused != LastFocused.Get() || !FMath::IsNearlyEqual(Height, LastHeight, 1.f))
	{
		FramesToCheck = SettleFrames;
	}
	LastFocused = Focused;
	LastHeight = Height;

	if (FramesToCheck > 0)
	{
		--FramesToCheck;
		if (Focused)
		{
			ShowInScrollBox(Focused);
		}
	}
}

UWidget* FGFCKeyboardScroll::FindFocusedInput(UWidgetTree* Tree)
{
	UWidget* Found = nullptr;
	if (Tree)
	{
		Tree->ForEachWidget([&Found](UWidget* W)
		{
			if (!Found && IsTextInput(W) && (W->HasKeyboardFocus() || W->HasFocusedDescendants()))
			{
				Found = W;
			}
		});
	}
	return Found;
}

void FGFCKeyboardScroll::ShowInScrollBox(UWidget* Input)
{
	UScrollBox* Scroll = nullptr;
	for (UPanelWidget* P = Input->GetParent(); P && !Scroll; P = P->GetParent())
	{
		Scroll = Cast<UScrollBox>(P);
	}
	if (!Scroll)
	{
		return;
	}

	// Compare in absolute (window) space: both geometries come from the last layout pass.
	const FGeometry& InputGeo = Input->GetCachedGeometry();
	const FGeometry& ScrollGeo = Scroll->GetCachedGeometry();
	const float Scale = FMath::Max(ScrollGeo.Scale, KINDA_SMALL_NUMBER);
	const float InputTop = InputGeo.GetAbsolutePosition().Y;
	const float InputBottom = InputTop + InputGeo.GetAbsoluteSize().Y;
	const float ViewTop = ScrollGeo.GetAbsolutePosition().Y;
	const float ViewBottom = ViewTop + ScrollGeo.GetAbsoluteSize().Y;
	if (ScrollGeo.GetAbsoluteSize().Y <= 0.f || InputGeo.GetAbsoluteSize().Y <= 0.f)
	{
		return; // not laid out yet
	}

	const float Gap = KeyboardGap * Scale;
	if (InputBottom > ViewBottom - Gap)
	{
		// Below the visible part (typically under the keyboard): bring its bottom just above it.
		Scroll->ScrollWidgetIntoView(Input, false, EDescendantScrollDestination::BottomOrRight, KeyboardGap);
	}
	else if (InputTop < ViewTop)
	{
		Scroll->ScrollWidgetIntoView(Input, false, EDescendantScrollDestination::TopOrLeft, KeyboardGap);
	}
}
