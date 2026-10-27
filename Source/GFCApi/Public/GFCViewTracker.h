#pragma once

#include "CoreMinimal.h"
#include "GFCApiTypes.h"

/**
 * Decides when a view is "qualified" and should be reported to the node with
 * POST /seeds/{id}/played (which may pay the creator). See docs/roadmap.md.
 *
 *   Image:       on screen at full size for ImageSeconds.
 *   3D model:    loaded and on screen for ModelSeconds, or the user orbited/zoomed.
 *   Video/audio: played for MediaPlaySeconds, or ShortClipFraction of a clip
 *                shorter than ShortClipSeconds.
 *
 * On-screen time only accumulates while the item is visible and loaded, and
 * resets when it leaves the screen, so a card flashing past never qualifies.
 * Each seed is reported at most once per app session. Pure logic: unit-tested.
 */
struct GFCAPI_API FGFCViewRules
{
	float ImageSeconds = 2.f;
	float ModelSeconds = 3.f;
	float MediaSeconds = 5.f;
	float ShortClipSeconds = 10.f;
	float ShortClipFraction = 0.5f;

	/** From UGFCApiSettings. */
	static FGFCViewRules FromSettings();
};

class GFCAPI_API FGFCViewTracker
{
public:
	explicit FGFCViewTracker(const FGFCViewRules& InRules = FGFCViewRules()) : Rules(InRules) {}

	/** Starts watching a seed. Own seeds are never reported. */
	void Begin(const FString& SeedId, EGFCMediaKind Kind, bool bOwnSeed);

	/** Stops watching (card closed or scrolled away). Accumulated time is discarded. */
	void End();

	/**
	 * Advances time. bVisible: the item is on screen at full size. bLoaded: the
	 * full content (image, model) is displayed, not just the thumbnail.
	 * Returns true exactly once, when the view becomes qualified.
	 */
	bool Tick(float DeltaSeconds, bool bVisible, bool bLoaded);

	/** Orbit, zoom or pan on a loaded model. Returns true if this qualifies the view. */
	bool NotifyInteraction();

	/** Media playback position. Returns true if this qualifies the view. */
	bool NotifyPlayback(float PositionSeconds, float DurationSeconds);

	bool HasReported(const FString& Id) const { return Reported.Contains(Id); }
	const FString& GetSeedId() const { return SeedId; }

	FGFCViewRules Rules;

private:
	bool Qualify();

	FString SeedId;
	EGFCMediaKind Kind = EGFCMediaKind::Other;
	bool bActive = false;
	bool bLoadedOnce = false;
	float VisibleSeconds = 0.f;
	TSet<FString> Reported;
};
