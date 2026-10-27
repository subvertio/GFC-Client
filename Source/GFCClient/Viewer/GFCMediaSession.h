#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GFCMediaSession.generated.h"

class AActor;
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

	float GetPositionSeconds() const;
	float GetDurationSeconds() const;
	bool IsPlaying() const;
	void TogglePause();

	void Release();

private:
	UPROPERTY(Transient) TObjectPtr<UMediaPlayer> Player;
	UPROPERTY(Transient) TObjectPtr<UMediaTexture> Texture;
	UPROPERTY(Transient) TObjectPtr<UMediaSoundComponent> Sound;
	UPROPERTY(Transient) TObjectPtr<AActor> SoundOwner;
};
