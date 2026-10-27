#include "UI/GFCDPIScalingRule.h"

#include "Engine/UserInterfaceSettings.h"
#include "HAL/PlatformApplicationMisc.h"

float UGFCDPIScalingRule::GetDPIScaleBasedOnSize(FIntPoint Size) const
{
#if PLATFORM_DESKTOP
	return ScaleFor(Size, true, FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(0.f, 0.f));
#else
	return ScaleFor(Size, false, 1.f);
#endif
}

float UGFCDPIScalingRule::PhoneScale(FIntPoint Size)
{
	const FRichCurve* Curve = GetDefault<UUserInterfaceSettings>()->UIScaleCurve.GetRichCurveConst();
	const float Shortest = float(FMath::Min(Size.X, Size.Y));
	return Curve && Curve->GetNumKeys() > 0 ? Curve->Eval(Shortest, 1.f) : 1.f;
}

float UGFCDPIScalingRule::ScaleFor(FIntPoint Size, bool bDesktop, float MonitorScale)
{
	if (Size.X <= 0 || Size.Y <= 0)
	{
		return 1.f;
	}
	if (!bDesktop || Size.Y > Size.X)
	{
		return PhoneScale(Size);
	}
	return FMath::Clamp(MonitorScale, 1.f, 4.f);
}
