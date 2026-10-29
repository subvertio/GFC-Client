#pragma once

#include "CoreMinimal.h"

class UWidget;
class UWidgetTree;
struct FGeometry;

/**
 * Keeps the text field you're typing in visible above the on-screen keyboard.
 *
 * On phones the window shrinks when the keyboard opens, so a field that was on screen
 * when you tapped it can end up below the visible part of its scroll box. Screens turn
 * off scroll-on-focus (it made fields jump when tapped), so nothing brought it back.
 *
 * Call Tick from a widget's NativeTick. When the focused field or the widget's height
 * changes, it waits for layout to settle, then scrolls the field's nearest scroll box
 * just enough to show it, with a little room above the keyboard.
 */
class GFCCLIENT_API FGFCKeyboardScroll
{
public:
	void Tick(UWidgetTree* Tree, const FGeometry& MyGeometry);

private:
	static UWidget* FindFocusedInput(UWidgetTree* Tree);
	static void ShowInScrollBox(UWidget* Input);

	TWeakObjectPtr<UWidget> LastFocused;
	float LastHeight = 0.f;
	/** Frames left to check: geometry from the resize takes a frame or two to arrive. */
	int32 FramesToCheck = 0;
};
