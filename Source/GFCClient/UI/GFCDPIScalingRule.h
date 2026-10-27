#pragma once

#include "CoreMinimal.h"
#include "Engine/DPICustomScalingRule.h"
#include "GFCDPIScalingRule.generated.h"

/**
 * The UI scale (dp per pixel), set as the project's custom DPI rule in DefaultEngine.ini.
 *
 * - Phones, and portrait windows on PC (the phone preview): UIScaleCurve on the screen's shortest side,
 *   exactly as before, so the target phone is 443 dp wide.
 * - Landscape windows on PC: the monitor's own scale (1 dp = 1 px at 100% Windows scaling), like a
 *   browser. A 1280x800 window is 1280 dp, wide enough for the desktop layout (GFCUI::Desktop).
 */
UCLASS()
class GFCCLIENT_API UGFCDPIScalingRule : public UDPICustomScalingRule
{
	GENERATED_BODY()

public:
	virtual float GetDPIScaleBasedOnSize(FIntPoint Size) const override;

	/** The phone curve's scale for a viewport, from UIScaleCurve (ShortestSide). */
	static float PhoneScale(FIntPoint Size);
	/** The rule without the platform check: bDesktop picks the PC behaviour. */
	static float ScaleFor(FIntPoint Size, bool bDesktop, float MonitorScale);
};
