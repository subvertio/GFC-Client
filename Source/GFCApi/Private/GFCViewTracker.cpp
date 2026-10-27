#include "GFCViewTracker.h"

#include "GFCApiSettings.h"

FGFCViewRules FGFCViewRules::FromSettings()
{
	const UGFCApiSettings* S = GetDefault<UGFCApiSettings>();
	FGFCViewRules R;
	R.ImageSeconds = S->ImageViewSeconds;
	R.ModelSeconds = S->ModelViewSeconds;
	R.MediaSeconds = S->MediaPlaySeconds;
	R.ShortClipSeconds = S->ShortClipSeconds;
	R.ShortClipFraction = S->ShortClipFraction;
	return R;
}

void FGFCViewTracker::Begin(const FString& InSeedId, EGFCMediaKind InKind, bool bOwnSeed)
{
	End();
	if (InSeedId.IsEmpty() || bOwnSeed || Reported.Contains(InSeedId))
	{
		return;
	}
	SeedId = InSeedId;
	Kind = InKind;
	bActive = true;
}

void FGFCViewTracker::End()
{
	bActive = false;
	bLoadedOnce = false;
	VisibleSeconds = 0.f;
	SeedId.Reset();
}

bool FGFCViewTracker::Qualify()
{
	if (!bActive)
	{
		return false;
	}
	Reported.Add(SeedId);
	bActive = false;
	return true;
}

bool FGFCViewTracker::Tick(float DeltaSeconds, bool bVisible, bool bLoaded)
{
	if (!bActive)
	{
		return false;
	}
	if (!bVisible || !bLoaded)
	{
		VisibleSeconds = 0.f; // must be continuous: a flash past doesn't add up
		return false;
	}
	bLoadedOnce = true;
	VisibleSeconds += FMath::Max(0.f, DeltaSeconds);
	switch (Kind)
	{
	case EGFCMediaKind::Image:
		return VisibleSeconds >= Rules.ImageSeconds && Qualify();
	case EGFCMediaKind::Model3D:
		return VisibleSeconds >= Rules.ModelSeconds && Qualify();
	default:
		return false; // video and audio qualify by playback position
	}
}

bool FGFCViewTracker::NotifyInteraction()
{
	return bActive && Kind == EGFCMediaKind::Model3D && bLoadedOnce && Qualify();
}

bool FGFCViewTracker::NotifyPlayback(float PositionSeconds, float DurationSeconds)
{
	if (!bActive || (Kind != EGFCMediaKind::Video && Kind != EGFCMediaKind::Audio))
	{
		return false;
	}
	if (PositionSeconds >= Rules.MediaSeconds)
	{
		return Qualify();
	}
	const bool bShortClip = DurationSeconds > 0.f && DurationSeconds < Rules.ShortClipSeconds;
	return bShortClip && PositionSeconds >= DurationSeconds * Rules.ShortClipFraction && Qualify();
}
