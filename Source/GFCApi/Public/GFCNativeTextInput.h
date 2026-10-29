#pragma once

#include "CoreMinimal.h"

/**
 * Phone text editing with the platform's own text box (Android; a no-op elsewhere).
 *
 * Unreal types through a hidden Android EditText and copies its text into the app's text box,
 * but the cursor only syncs from Android to the app, and Slate has no phone selection handles,
 * so tapping a field couldn't move the cursor or select. While a field is being edited, Place
 * lays that EditText exactly over it, styled like it (GFCApi_UPL.xml), so taps, drags,
 * long-press selection and copy/paste are Android's. Text and selection still flow back.
 *
 * All positions and sizes are in Slate window pixels; Java scales them to its views.
 */
struct FGFCNativeTextParams
{
	/** The editing area: the field inside its border. */
	FIntRect Rect;
	/** The Slate window's width, for scaling to Android views. */
	int32 WindowWidth = 0;
	float TextPx = 0.f;
	/** Text padding inside Rect. */
	FIntRect Padding;
	FColor TextColor = FColor::White;
	FColor HintColor = FColor(128, 128, 128);
	FColor Background = FColor::Black;
	FColor Accent = FColor(59, 76, 202);
	float CornerRadius = 0.f;
	FString Hint;
	/** Wraps onto new lines (the post text); single-line fields scroll sideways while typing. */
	bool bMultiLine = false;

	bool operator==(const FGFCNativeTextParams& O) const
	{
		return Rect == O.Rect && WindowWidth == O.WindowWidth && FMath::IsNearlyEqual(TextPx, O.TextPx, 0.25f)
			&& Padding == O.Padding && TextColor == O.TextColor && HintColor == O.HintColor
			&& Background == O.Background && Accent == O.Accent
			&& FMath::IsNearlyEqual(CornerRadius, O.CornerRadius, 0.25f) && Hint == O.Hint && bMultiLine == O.bMultiLine;
	}
};

namespace GFCNativeTextInput
{
	/** True where Place does something (Android). */
	GFCAPI_API bool IsSupported();
	GFCAPI_API void Place(const FGFCNativeTextParams& Params);
	GFCAPI_API void Release();
}
