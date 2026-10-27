#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCDebugOverlay.generated.h"

class UTextBlock;

/**
 * Development builds only: an on-screen panel with fps, API calls (in flight,
 * recent, errors), cache usage, the live 3D item's memory, and process memory.
 *
 * Toggle: a three-finger tap anywhere, the console command `gfc.debug`, or
 * start with -GFCDebug. Compiled out of Shipping builds.
 */
UCLASS()
class GFCCLIENT_API UGFCDebugOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Creates the overlay (hidden unless -GFCDebug) and hooks the toggle gesture. Non-Shipping only. */
	static void Install(APlayerController* Owner);

	void Toggle();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Refresh();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> Lines;
	float SmoothedDelta = 1.f / 60.f;
	float SinceRefresh = 0.f;
	TSharedPtr<class FGFCDebugGesture> Gesture;
};
