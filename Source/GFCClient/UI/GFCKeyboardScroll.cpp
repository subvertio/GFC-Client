#include "UI/GFCKeyboardScroll.h"

#include "Blueprint/WidgetTree.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableText.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Framework/Application/SlateApplication.h"
#include "GFCNativeTextInput.h"
#include "Layout/Geometry.h"
#include "UI/GFCUIStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

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

	// One native text box for the whole app: whichever widget's tree holds the focused field owns it.
	TWeakObjectPtr<UWidget> GNativeOwner;
	FGFCNativeTextParams GNativeLast;
	bool bGNativePlaced = false;
	int32 GNativeFramesSinceSend = 0;
	/** Re-send now and then: Unreal moves its EditText when it (re)shows the keyboard. */
	constexpr int32 NativeResendFrames = 10;

	bool HasFocus(const UWidget* W)
	{
		return W && (W->HasKeyboardFocus() || W->HasFocusedDescendants());
	}

	/** Where and how to draw the native box over Input, in Slate window pixels. False if it isn't on screen. */
	bool BuildNativeParams(UWidget* Input, FGFCNativeTextParams& Out)
	{
		const FEditableTextBoxStyle* Style = nullptr;
		if (const UEditableTextBox* E = Cast<UEditableTextBox>(Input))
		{
			Style = &E->GetWidgetStyle();
			Out.Hint = E->GetHintText().ToString();
		}
		else if (const UMultiLineEditableTextBox* M = Cast<UMultiLineEditableTextBox>(Input))
		{
			Style = &M->WidgetStyle;
			Out.Hint = M->GetHintText().ToString();
			Out.bMultiLine = true;
		}
		const TSharedPtr<SWidget> Slate = Input->GetCachedWidget();
		if (!Style || !Slate.IsValid())
		{
			return false;
		}
		const FGeometry& Geo = Input->GetCachedGeometry();
		const FVector2D Pos = Geo.GetAbsolutePosition();
		const FVector2D Size = Geo.GetAbsoluteSize();
		const float Scale = Geo.Scale;
		if (Size.X <= 1.f || Size.Y <= 1.f || Scale <= 0.f)
		{
			return false;
		}
		// Not while it's scrolled out of its scroll box (the box would float over other things)
		for (UPanelWidget* P = Input->GetParent(); P; P = P->GetParent())
		{
			if (const UScrollBox* Scroll = Cast<UScrollBox>(P))
			{
				const FGeometry& SG = Scroll->GetCachedGeometry();
				const float Top = SG.GetAbsolutePosition().Y, Bottom = Top + SG.GetAbsoluteSize().Y;
				if (Pos.Y < Top - 1.f || Pos.Y + Size.Y > Bottom + 1.f)
				{
					return false;
				}
				break;
			}
		}
		const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Slate.ToSharedRef());
		if (!Window.IsValid())
		{
			return false;
		}

		// Inside the focused border, so the accent outline stays visible around the native box
		const FSlateBrush& Brush = Style->BackgroundImageFocused;
		const float Border = Brush.OutlineSettings.Width;
		const float Inset = Border * Scale;
		Out.Rect = FIntRect(FMath::RoundToInt(Pos.X + Inset), FMath::RoundToInt(Pos.Y + Inset),
			FMath::RoundToInt(Pos.X + Size.X - Inset), FMath::RoundToInt(Pos.Y + Size.Y - Inset));
		Out.WindowWidth = FMath::RoundToInt(Window->GetSizeInScreen().X);
		// Slate font sizes are points at 96 dpi
		Out.TextPx = Style->TextStyle.Font.Size * (96.f / 72.f) * Scale;
		const FMargin& Pad = Style->Padding;
		Out.Padding = FIntRect(FMath::RoundToInt(FMath::Max(0.f, Pad.Left - Border) * Scale), FMath::RoundToInt(FMath::Max(0.f, Pad.Top - Border) * Scale),
			FMath::RoundToInt(FMath::Max(0.f, Pad.Right - Border) * Scale), FMath::RoundToInt(FMath::Max(0.f, Pad.Bottom - Border) * Scale));
		Out.TextColor = Style->FocusedForegroundColor.GetSpecifiedColor().ToFColor(true);
		Out.HintColor = FGFCTheme::Get().TextSecondary.ToFColor(true);
		Out.Background = Brush.TintColor.GetSpecifiedColor().ToFColor(true);
		Out.Background.A = 255; // opaque: it hides the app's copy of the text underneath
		Out.Accent = Brush.OutlineSettings.Color.GetSpecifiedColor().ToFColor(true);
		Out.CornerRadius = FMath::Max(0.f, float(Brush.OutlineSettings.CornerRadii.X) - Border) * Scale;
		return true;
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
	UpdateNativeText(Focused);
}

void FGFCKeyboardScroll::UpdateNativeText(UWidget* Focused)
{
	if (!GFCNativeTextInput::IsSupported())
	{
		return;
	}
	FGFCNativeTextParams Params;
	if (Focused && BuildNativeParams(Focused, Params))
	{
		++GNativeFramesSinceSend;
		if (!bGNativePlaced || GNativeOwner.Get() != Focused || !(Params == GNativeLast) || GNativeFramesSinceSend >= NativeResendFrames)
		{
			GFCNativeTextInput::Place(Params);
			GNativeLast = Params;
			GNativeOwner = Focused;
			bGNativePlaced = true;
			GNativeFramesSinceSend = 0;
		}
		return;
	}
	// This tree has no field being edited (or it's off screen). Release only what this tree placed
	// or what nobody is editing any more: another widget's tree may own the box this frame.
	if (bGNativePlaced && (Focused == GNativeOwner.Get() || !HasFocus(GNativeOwner.Get())))
	{
		UWidget* Owner = GNativeOwner.Get();
		GFCNativeTextInput::Release();
		bGNativePlaced = false;
		GNativeOwner = nullptr;
		// Done editing a single-line field: show the start of its text again (it stayed scrolled to
		// where the cursor was), as phones do. Not while it's still being edited, just scrolled away.
		if (Cast<UEditableTextBox>(Owner) && !HasFocus(Owner))
		{
			const TSharedPtr<SWidget> Slate = Owner->GetCachedWidget();
			if (Slate.IsValid() && Slate->GetType() == FName(TEXT("SEditableTextBox")))
			{
				StaticCastSharedPtr<SEditableTextBox>(Slate)->ScrollTo(ETextLocation::BeginningOfDocument);
			}
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
