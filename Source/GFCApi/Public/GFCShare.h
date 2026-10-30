#pragma once

#include "CoreMinimal.h"

/** Sharing a GFC Exclusive's teaser on X: the user posts it themselves (the app has no X account). */
struct GFCAPI_API FGFCShare
{
	enum class EResult : uint8
	{
		/** Android: the share sheet opened with the clip attached and the text filled in. */
		SheetOpened,
		/** Windows: the clip was saved (SavedPath, shown in Explorer) and the X post page opened. */
		SavedAndOpened,
		/** Only the X post page opened (no clip to attach, or it couldn't be saved). */
		OpenedLink,
		Failed,
	};

	/**
	 * Shares VideoPath (a downloaded .mp4, may be empty) with Text. Android: ACTION_SEND video/mp4 through
	 * the share sheet, after copying the clip to Movies/GFC (where X can read it). Windows: copies the clip
	 * to Videos\GFC\SaveName and opens XIntentUrl. With no clip, just opens XIntentUrl.
	 */
	static EResult ShareVideo(const FString& VideoPath, const FString& SaveName, const FString& Text, const FString& XIntentUrl,
		FString* OutSavedPath = nullptr);

	/** Where Windows keeps shared clips: <Videos>\GFC (falls back to the project's Saved folder). */
	static FString WindowsVideosDir();

	/** "<title>-teaser.mp4" with characters files can't have replaced. */
	static FString TeaserFileName(const FString& Title, const FString& SeedId);
};
