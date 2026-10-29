#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GFCWavStreamer.generated.h"

class IFileHandle;
class UAudioComponent;
class USoundWaveProcedural;

/** What a WAV file holds, from its RIFF header. */
struct GFCCLIENT_API FGFCWavFormat
{
	int32 Channels = 0;
	int32 SampleRate = 0;
	int32 BitsPerSample = 0;
	bool bFloat = false;
	int32 BlockAlign = 0;   // bytes per frame (all channels)
	int64 DataOffset = 0;   // where the samples start in the file
	int64 DataBytes = 0;    // how many bytes of samples

	double DurationSeconds() const { return BlockAlign > 0 && SampleRate > 0 ? double(DataBytes / BlockAlign) / SampleRate : 0.0; }
};

enum class EGFCWavParse : uint8 { Ok, NeedMore, Unsupported };

/**
 * Plays a WAV file in the engine, without the platform media player (Android's doesn't do WAV in
 * Unreal). It reads the file while it's still downloading: the header first, then about two seconds
 * of samples at a time, converted to 16-bit and fed to a procedural sound wave. Memory stays near
 * 1 MB whatever the file size. PCM 8/16/24/32-bit and 32-bit float, mono or stereo, any sample rate.
 *
 * Seeking is instant inside what's downloaded; past it, playback waits for the download.
 */
UCLASS(Transient)
class GFCCLIENT_API UGFCWavStreamer : public UObject
{
	GENERATED_BODY()

public:
	/** Starts reading Path (which may still be growing). Playback begins once the header and a little audio are there. */
	void Begin(UWorld* World, const FString& Path);
	/** The download wrote more bytes (or finished). */
	void SetBytesOnDisk(int64 Bytes, bool bComplete);
	/** Feeds the sound; call every frame or so. Returns false after a fatal error (see GetError). */
	bool Tick();

	bool HasStarted() const { return Wave != nullptr; }
	bool IsPlaying() const { return Wave && !bPaused && !bEnded; }
	bool IsBuffering() const;
	bool HasEnded() const { return bEnded; }
	void SetPaused(bool bInPaused);
	void Seek(double Seconds);
	void SetLooping(bool bInLoop) { bLoop = bInLoop; }
	double GetPosition() const;
	double GetDuration() const { return Format.DurationSeconds(); }
	const FString& GetError() const { return Error; }
	/** One line for the log: position, queued audio, read and download progress. */
	FString Describe() const;
	void Release();

	/** Parses a RIFF/WAVE header from the start of a file. NeedMore: read more bytes and try again. */
	static EGFCWavParse ParseHeader(const uint8* Data, int64 Size, FGFCWavFormat& Out, FString& OutError);
	/** Converts whole frames of any supported format to interleaved 16-bit PCM. */
	static void ToPcm16(const uint8* In, int32 Frames, const FGFCWavFormat& Format, TArray<uint8>& Out);
	/** True when the bytes start like a WAV file ("RIFF....WAVE"). */
	static bool LooksLikeWav(const uint8* Data, int64 Size);

private:
	bool ReadAt(int64 Offset, int64 Count, TArray<uint8>& Out);
	void StartSound();

	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> Wave;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Audio;
	TWeakObjectPtr<UWorld> World;
	TUniquePtr<IFileHandle> File;
	FString Path;
	FString Error;
	FGFCWavFormat Format;
	bool bHeader = false;
	bool bComplete = false;
	bool bPaused = false;
	bool bEnded = false;
	bool bLoop = false;
	int64 OnDisk = 0;
	int64 HandleSize = 0;      // the file size the open handle can read up to
	int64 ReadPos = 0;          // next file byte to decode
	double SeekBase = 0.0;      // position (s) at the last seek
	int64 QueuedSinceSeek = 0;  // 16-bit bytes queued since the last seek
};
