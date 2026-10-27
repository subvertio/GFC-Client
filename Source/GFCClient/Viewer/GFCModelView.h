#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCModelView.generated.h"

class AGFCModelViewer;
class UImage;

DECLARE_DELEGATE(FGFCOnModelInteract);

/**
 * Displays an AGFCModelViewer and turns gestures into camera moves:
 *   one finger / left mouse drag: orbit
 *   two-finger pinch / mouse wheel: zoom
 *   two-finger drag / right mouse drag: pan
 *   double tap / double click: reset
 */
UCLASS()
class GFCCLIENT_API UGFCModelView : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetViewer(AGFCModelViewer* InViewer);
	AGFCModelViewer* GetViewer() const { return Viewer.Get(); }

	/** Fired on the first gesture of each drag/pinch (counts toward a qualified view). */
	FGFCOnModelInteract OnInteract;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& Geometry, const FPointerEvent& Event) override;

	UPROPERTY(BlueprintReadOnly, Category = "GFC", meta = (BindWidget)) TObjectPtr<UImage> ViewImage;

private:
	void Interacted();

	TWeakObjectPtr<AGFCModelViewer> Viewer;
	TMap<uint32, FVector2D> Touches;
	FVector2D LastMouse = FVector2D::ZeroVector;
	FKey DragButton;
	float LastPinch = 0.f;
	FVector2D LastMid = FVector2D::ZeroVector;
	bool bGestureCounted = false;
};
