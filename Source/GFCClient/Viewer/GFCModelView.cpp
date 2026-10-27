#include "Viewer/GFCModelView.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "InputCoreTypes.h"
#include "Viewer/GFCModelViewer.h"

namespace
{
	constexpr float OrbitDegreesPerViewWidth = 360.f;
}

void UGFCModelView::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		ViewImage = WidgetTree->ConstructWidget<UImage>();
		WidgetTree->RootWidget = ViewImage;
	}
	SetVisibility(ESlateVisibility::Visible); // receives input
}

void UGFCModelView::SetViewer(AGFCModelViewer* InViewer)
{
	Viewer = InViewer;
	if (ViewImage)
	{
		if (InViewer && InViewer->GetRenderTarget())
		{
			ViewImage->SetBrushResourceObject(InViewer->GetRenderTarget());
			ViewImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			ViewImage->SetBrushResourceObject(nullptr);
			ViewImage->SetColorAndOpacity(FLinearColor::Transparent);
		}
	}
	Touches.Reset();
	bGestureCounted = false;
}

void UGFCModelView::Interacted()
{
	if (!bGestureCounted)
	{
		bGestureCounted = true;
		OnInteract.ExecuteIfBound();
	}
}

FReply UGFCModelView::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.IsTouchEvent() || !Viewer.IsValid())
	{
		return FReply::Unhandled();
	}
	DragButton = Event.GetEffectingButton();
	LastMouse = Event.GetScreenSpacePosition();
	bGestureCounted = false;
	Viewer->SetHeld(true); // the turntable waits while the model is in hand
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UGFCModelView::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (HasMouseCapture())
	{
		DragButton = FKey();
		if (Viewer.IsValid()) Viewer->SetHeld(false);
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply UGFCModelView::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!HasMouseCapture() || !Viewer.IsValid() || Event.IsTouchEvent())
	{
		return FReply::Unhandled();
	}
	const FVector2D Pos = Event.GetScreenSpacePosition();
	const FVector2D Size = Geometry.GetAbsoluteSize();
	const FVector2D Delta = (Pos - LastMouse) / FVector2D(FMath::Max(1.0, Size.X), FMath::Max(1.0, Size.Y));
	LastMouse = Pos;
	if (Delta.IsNearlyZero())
	{
		return FReply::Handled();
	}
	if (DragButton == EKeys::LeftMouseButton)
	{
		Viewer->Orbit(Delta.X * OrbitDegreesPerViewWidth, -Delta.Y * OrbitDegreesPerViewWidth * 0.5f);
	}
	else
	{
		Viewer->Pan(Delta.X, Delta.Y);
	}
	Interacted();
	return FReply::Handled();
}

FReply UGFCModelView::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// The wheel scrolls the page (the view sits in a scroll box); Ctrl + wheel zooms. Phones pinch.
	if (!Viewer.IsValid() || !Event.IsControlDown())
	{
		return FReply::Unhandled();
	}
	Viewer->Zoom(Event.GetWheelDelta() > 0 ? 0.88f : 1.f / 0.88f);
	bGestureCounted = false;
	Interacted();
	return FReply::Handled();
}

FReply UGFCModelView::NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Viewer.IsValid())
	{
		Viewer->ResetView();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UGFCModelView::NativeOnTouchStarted(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!Viewer.IsValid())
	{
		return FReply::Unhandled();
	}
	Touches.Add(Event.GetPointerIndex(), Event.GetScreenSpacePosition());
	bGestureCounted = false;
	Viewer->SetHeld(true);
	if (Touches.Num() == 2)
	{
		TArray<FVector2D> P;
		Touches.GenerateValueArray(P);
		LastPinch = float(FVector2D::Distance(P[0], P[1]));
		LastMid = (P[0] + P[1]) * 0.5;
	}
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UGFCModelView::NativeOnTouchMoved(const FGeometry& Geometry, const FPointerEvent& Event)
{
	FVector2D* Prev = Touches.Find(Event.GetPointerIndex());
	if (!Viewer.IsValid() || !Prev)
	{
		return FReply::Unhandled();
	}
	const FVector2D Size = Geometry.GetAbsoluteSize();
	const FVector2D Norm(FMath::Max(1.0, Size.X), FMath::Max(1.0, Size.Y));
	const FVector2D Pos = Event.GetScreenSpacePosition();
	if (Touches.Num() == 1)
	{
		const FVector2D Delta = (Pos - *Prev) / Norm;
		*Prev = Pos;
		Viewer->Orbit(Delta.X * OrbitDegreesPerViewWidth, -Delta.Y * OrbitDegreesPerViewWidth * 0.5f);
	}
	else
	{
		*Prev = Pos;
		TArray<FVector2D> P;
		Touches.GenerateValueArray(P);
		const float Pinch = float(FVector2D::Distance(P[0], P[1]));
		const FVector2D Mid = (P[0] + P[1]) * 0.5;
		if (LastPinch > 1.f && Pinch > 1.f)
		{
			Viewer->Zoom(LastPinch / Pinch);
		}
		const FVector2D MidDelta = (Mid - LastMid) / Norm;
		Viewer->Pan(MidDelta.X, MidDelta.Y);
		LastPinch = Pinch;
		LastMid = Mid;
	}
	Interacted();
	return FReply::Handled();
}

FReply UGFCModelView::NativeOnTouchEnded(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Touches.Remove(Event.GetPointerIndex());
	LastPinch = 0.f;
	if (Touches.Num() == 0)
	{
		if (Viewer.IsValid()) Viewer->SetHeld(false);
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}
