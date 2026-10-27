#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GFCMediaSession.generated.h"

class AActor;
class UImage;
class UMediaPlayer;
class UMediaSoundComponent;
class UMediaTexture;

/**
 * Plays one downloaded video or audio file in the app (Media Framework:
 * WMF on Windows, MediaPlayer on Android). Release() stops it and frees the
 * player, texture and sound component; the cache deletes the file.
 */
UCLASS(Transient)
class GFCCLIENT_API UGFCMediaSession : public UObject
{
	GENERATED_BODY()

public:
	/** Opens and plays a local file. Returns false if no media player could open it. */
	bool Open(UWorld* World, const FString& FilePath);

	/** For video: the texture to show (nullptr for audio). */
	UMediaTexture* GetVideoTexture() const { return Texture; }
	/**
	 * Gives Image's brush the video's real frame size once the first frame is decoded, so a ScaleToFit box
	 * letterboxes it at the right aspect (the brush otherwise keeps the thumbnail's size, or 32x32).
	 * Call each tick while the video shows; it's cheap when nothing changed.
	 */
	void FitBrush(UImage* Image) const;

	float GetPositionSeconds() const;
	float GetDurationSeconds() const;
	bool IsPlaying() const;
	void TogglePause();
	/** Jumps to Seconds into the media. */
	void Seek(float Seconds);
	void SetLooping(bool bLoop);

	/** Video starts muted in the feed; the user turns sound on. */
	void SetMuted(bool bInMuted);
	bool IsMuted() const { return bMuted; }

	void Release();

private:
	UPROPERTY(Transient) TObjectPtr<UMediaPlayer> Player;
	UPROPERTY(Transient) TObjectPtr<UMediaTexture> Texture;
	UPROPERTY(Transient) TObjectPtr<UMediaSoundComponent> Sound;
	UPROPERTY(Transient) TObjectPtr<AActor> SoundOwner;
	bool bMuted = false;
};
