#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GFCApiTypes.h"
#include "GFCViewTracker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GFCMusicPlayer.generated.h"

class UGFCMediaSession;
class UGFCWavStreamer;

DECLARE_MULTICAST_DELEGATE(FGFCOnMusicChanged);

enum class EGFCMusicState : uint8 { Idle, ConfirmCellular, Loading, Playing, Paused, Failed };

/**
 * The app's music player: one audio post at a time, playing across screens (the mini player above
 * the bottom bar and the full player on the post screen both drive it).
 *
 * Two engines: WAV plays in the engine itself (UGFCWavStreamer), from the file while it downloads,
 * because Android's platform player in Unreal doesn't do WAV; everything else (MP3, AAC/M4A, OGG,
 * Opus, FLAC) goes to the platform media player, falling back to the WAV engine for a file that is
 * really WAV. A big track on mobile data asks first. When the node offers a compressed streaming copy
 * (FGFCSeed::Stream), that's played instead of the original.
 *
 * The file is the cache's one live heavy item (owner "music"): opening a video or a 3D model stops
 * the music, and logging out stops it too. Qualified plays are reported to the node like inline
 * video, so creators earn from listens.
 */
UCLASS()
class GFCCLIENT_API UGFCMusicPlayer : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Plays this audio post from the start (downloads it first). The same post again just resumes. */
	void Play(const FGFCSeed& Seed);
	void TogglePause();
	/** Jumps to Seconds (clamped to the track). */
	void Seek(float Seconds);
	void Skip(float DeltaSeconds) { Seek(GetPosition() + DeltaSeconds); }
	void SetLooping(bool bInLoop);
	bool IsLooping() const { return bLoop; }
	/** Stops and frees the player and the file. */
	void Stop();

	EGFCMusicState GetState() const { return State; }
	bool IsActive() const { return State != EGFCMusicState::Idle; }
	const FGFCSeed& GetSeed() const { return Seed; }
	bool IsCurrent(const FString& SeedId) const { return IsActive() && Seed.Id == SeedId; }
	float GetPosition() const;
	float GetDuration() const;
	const FString& GetError() const { return Error; }

	/** Fired when the track, state or loop setting changes (not every tick: poll the position). */
	FGFCOnMusicChanged OnChanged;

	/** The message for Failed, or the mobile-data question for ConfirmCellular. */
	const FString& GetMessage() const { return Error; }

	/** "m:ss" (or "h:mm:ss") for the player's time labels. */
	static FString FormatTime(float Seconds);

	static constexpr const TCHAR* CacheOwner = TEXT("music");

private:
	bool Tick(float DeltaSeconds);
	void SetState(EGFCMusicState NewState, const FString& InError = FString());
	void OnOwnerReleased(const FString& Owner);
	void OpenWithPlatformPlayer(const FString& Path);
	void BeginListen();
	void Fail(const FString& Message);

	UPROPERTY(Transient) TObjectPtr<UGFCMediaSession> Session;
	UPROPERTY(Transient) TObjectPtr<UGFCWavStreamer> Wav;
	bool bWavBegun = false;
	float LogTimer = 0.f;
	FGFCSeed Seed;
	EGFCMusicState State = EGFCMusicState::Idle;
	FString Error;
	bool bLoop = false;
	bool bWasPlaying = false;
	uint32 LoadSerial = 0;
	FGFCViewTracker Tracker;
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle ReleasedHandle;
	FDelegateHandle SessionHandle;
};
