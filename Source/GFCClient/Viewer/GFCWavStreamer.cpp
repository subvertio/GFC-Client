#include "Viewer/GFCWavStreamer.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	constexpr int64 HeaderProbeBytes = 64 * 1024;
	constexpr double QueueAheadSeconds = 2.0;   // keep this much decoded audio ahead of the speaker
	constexpr double ChunkSeconds = 0.25;       // decode in steps of this much
	constexpr double StartAfterSeconds = 0.5;   // begin playing once this much is downloaded

	uint32 U32(const uint8* P) { return uint32(P[0]) | uint32(P[1]) << 8 | uint32(P[2]) << 16 | uint32(P[3]) << 24; }
	uint16 U16(const uint8* P) { return uint16(P[0] | P[1] << 8); }
}

bool UGFCWavStreamer::LooksLikeWav(const uint8* Data, int64 Size)
{
	return Size >= 12 && FMemory::Memcmp(Data, "RIFF", 4) == 0 && FMemory::Memcmp(Data + 8, "WAVE", 4) == 0;
}

EGFCWavParse UGFCWavStreamer::ParseHeader(const uint8* Data, int64 Size, FGFCWavFormat& Out, FString& OutError)
{
	if (Size < 12)
	{
		return EGFCWavParse::NeedMore;
	}
	if (!LooksLikeWav(Data, Size))
	{
		OutError = TEXT("This isn't a WAV file.");
		return EGFCWavParse::Unsupported;
	}
	bool bFormat = false;
	for (int64 At = 12; ; )
	{
		if (At + 8 > Size)
		{
			return At > HeaderProbeBytes ? (OutError = TEXT("The WAV header is too long."), EGFCWavParse::Unsupported) : EGFCWavParse::NeedMore;
		}
		const uint32 ChunkSize = U32(Data + At + 4);
		const int64 Body = At + 8;
		if (FMemory::Memcmp(Data + At, "fmt ", 4) == 0)
		{
			if (Body + 16 > Size) return EGFCWavParse::NeedMore;
			uint16 Tag = U16(Data + Body);
			Out.Channels = U16(Data + Body + 2);
			Out.SampleRate = int32(U32(Data + Body + 4));
			Out.BlockAlign = U16(Data + Body + 12);
			Out.BitsPerSample = U16(Data + Body + 14);
			if (Tag == 0xFFFE && ChunkSize >= 40) // WAVE_FORMAT_EXTENSIBLE: the real format is the sub-format's first two bytes
			{
				if (Body + 26 > Size) return EGFCWavParse::NeedMore;
				Tag = U16(Data + Body + 24);
			}
			Out.bFloat = Tag == 3;
			const bool bPcm = Tag == 1 && (Out.BitsPerSample == 8 || Out.BitsPerSample == 16 || Out.BitsPerSample == 24 || Out.BitsPerSample == 32);
			const bool bFloat32 = Tag == 3 && Out.BitsPerSample == 32;
			if ((!bPcm && !bFloat32) || Out.Channels < 1 || Out.Channels > 2 || Out.SampleRate < 8000 || Out.SampleRate > 192000
				|| Out.BlockAlign != Out.Channels * Out.BitsPerSample / 8)
			{
				OutError = FString::Printf(TEXT("This WAV format isn't supported (format %d, %d-bit, %d channels)."), Tag, Out.BitsPerSample, Out.Channels);
				return EGFCWavParse::Unsupported;
			}
			bFormat = true;
		}
		else if (FMemory::Memcmp(Data + At, "data", 4) == 0)
		{
			if (!bFormat)
			{
				OutError = TEXT("The WAV file has no format chunk before its samples.");
				return EGFCWavParse::Unsupported;
			}
			Out.DataOffset = Body;
			// Some encoders write 0 or 0xFFFFFFFF while streaming: then the samples run to the end of the file.
			Out.DataBytes = (ChunkSize == 0 || ChunkSize == 0xFFFFFFFF) ? MAX_int64 : int64(ChunkSize);
			Out.DataBytes -= Out.DataBytes % Out.BlockAlign;
			return EGFCWavParse::Ok;
		}
		At = Body + int64(ChunkSize) + (ChunkSize & 1);
	}
}

void UGFCWavStreamer::ToPcm16(const uint8* In, int32 Frames, const FGFCWavFormat& F, TArray<uint8>& Out)
{
	const int32 Samples = Frames * F.Channels;
	Out.SetNumUninitialized(Samples * 2);
	int16* Dst = reinterpret_cast<int16*>(Out.GetData());
	switch (F.BitsPerSample)
	{
	case 8: // unsigned
		for (int32 i = 0; i < Samples; ++i) Dst[i] = int16((int32(In[i]) - 128) << 8);
		break;
	case 16:
		FMemory::Memcpy(Dst, In, Samples * 2);
		break;
	case 24:
		for (int32 i = 0; i < Samples; ++i) Dst[i] = int16(In[i * 3 + 1] | (In[i * 3 + 2] << 8)); // keep the top 16 bits
		break;
	case 32:
		for (int32 i = 0; i < Samples; ++i)
		{
			if (F.bFloat)
			{
				float V;
				FMemory::Memcpy(&V, In + i * 4, 4);
				Dst[i] = int16(FMath::Clamp(V, -1.f, 1.f) * 32767.f);
			}
			else
			{
				int32 V;
				FMemory::Memcpy(&V, In + i * 4, 4);
				Dst[i] = int16(V >> 16);
			}
		}
		break;
	default:
		FMemory::Memzero(Dst, Samples * 2);
	}
}

void UGFCWavStreamer::Begin(UWorld* InWorld, const FString& InPath)
{
	Release();
	World = InWorld;
	Path = InPath;
}

void UGFCWavStreamer::SetBytesOnDisk(int64 Bytes, bool bInComplete)
{
	OnDisk = FMath::Max(OnDisk, Bytes);
	bComplete = bComplete || bInComplete;
}

bool UGFCWavStreamer::ReadAt(int64 Offset, int64 Count, TArray<uint8>& Out)
{
	// The file grows while it downloads, and a file handle only reads up to the size it saw when it
	// was opened: reopen it whenever the download has gone past that.
	if (File.IsValid() && Offset + Count > HandleSize && OnDisk > HandleSize)
	{
		File.Reset();
	}
	if (!File.IsValid())
	{
		File.Reset(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Path, true));
		if (!File.IsValid()) return false;
		HandleSize = File->Size();
	}
	if (Offset + Count > HandleSize) return false; // not downloaded yet
	Out.SetNumUninitialized(Count);
	return File->Seek(Offset) && File->Read(Out.GetData(), Count);
}

bool UGFCWavStreamer::IsBuffering() const
{
	return Wave && !bEnded && !bPaused && Wave->GetAvailableAudioByteCount() == 0 && ReadPos < Format.DataOffset + Format.DataBytes;
}

bool UGFCWavStreamer::Tick()
{
	if (!Error.IsEmpty() || Path.IsEmpty())
	{
		return Error.IsEmpty();
	}
	TArray<uint8> Buf;
	if (!bHeader)
	{
		const int64 Probe = FMath::Min(OnDisk, HeaderProbeBytes);
		if (Probe < 12 || !ReadAt(0, Probe, Buf)) return true;
		switch (ParseHeader(Buf.GetData(), Buf.Num(), Format, Error))
		{
		case EGFCWavParse::NeedMore:
			if (bComplete || OnDisk >= HeaderProbeBytes) { Error = TEXT("The WAV header is incomplete."); return false; }
			return true;
		case EGFCWavParse::Unsupported:
			return false;
		default:
			break;
		}
		if (bComplete && Format.DataBytes == MAX_int64) Format.DataBytes = OnDisk - Format.DataOffset;
		bHeader = true;
		ReadPos = Format.DataOffset;
	}
	if (bComplete && Format.DataBytes == MAX_int64)
	{
		Format.DataBytes = (OnDisk - Format.DataOffset) - (OnDisk - Format.DataOffset) % Format.BlockAlign;
	}
	const int64 BytesPerSecondIn = int64(Format.SampleRate) * Format.BlockAlign;
	if (!Wave)
	{
		// Start once there's a little audio, so the first second doesn't stutter.
		const int64 Need = Format.DataOffset + FMath::Min<int64>(Format.DataBytes, int64(StartAfterSeconds * BytesPerSecondIn));
		if (OnDisk < Need && !bComplete) return true;
		StartSound();
		if (!Wave) return false;
	}
	if (bPaused || bEnded) return true;

	const int64 DataEnd = Format.DataOffset + Format.DataBytes;
	const int64 OutBytesPerSecond = int64(Format.SampleRate) * Format.Channels * 2;
	const int64 Available = FMath::Min<int64>(OnDisk, DataEnd);
	while (Wave->GetAvailableAudioByteCount() < QueueAheadSeconds * OutBytesPerSecond && ReadPos < Available)
	{
		int64 Count = FMath::Min<int64>(Available - ReadPos, int64(ChunkSeconds * BytesPerSecondIn));
		Count -= Count % Format.BlockAlign;
		if (Count <= 0 || !ReadAt(ReadPos, Count, Buf)) break;
		TArray<uint8> Pcm;
		ToPcm16(Buf.GetData(), int32(Count / Format.BlockAlign), Format, Pcm);
		Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
		QueuedSinceSeek += Pcm.Num();
		ReadPos += Count;
	}
	if (ReadPos >= DataEnd && Wave->GetAvailableAudioByteCount() == 0)
	{
		if (bLoop) Seek(0.0);
		else { bEnded = true; if (Audio) Audio->SetPaused(true); }
	}
	return true;
}

void UGFCWavStreamer::StartSound()
{
	UWorld* W = World.Get();
	if (!W)
	{
		Error = TEXT("No world to play in.");
		return;
	}
	Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(Format.SampleRate);
	Wave->NumChannels = Format.Channels;
	Wave->Duration = INDEFINITELY_LOOPING_DURATION; // ends when we stop feeding it
	Wave->SoundGroup = SOUNDGROUP_Music;
	Wave->bLooping = false;
	// Keep pulling samples even when silent (unfocused window, volume 0): a virtualized procedural sound stops its clock.
	Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	Audio = UGameplayStatics::CreateSound2D(W, Wave, 1.f, 1.f, 0.f, nullptr, false, false);
	if (!Audio)
	{
		Error = TEXT("Couldn't start the audio.");
		Wave = nullptr;
		return;
	}
	Audio->bIsUISound = true;
	Audio->Play();
	if (bPaused) Audio->SetPaused(true);
}

void UGFCWavStreamer::SetPaused(bool bInPaused)
{
	if (bEnded && !bInPaused)
	{
		Seek(0.0); // Play after the end starts again
	}
	bPaused = bInPaused;
	if (Audio) Audio->SetPaused(bPaused);
}

void UGFCWavStreamer::Seek(double Seconds)
{
	if (!bHeader) return;
	const int64 Frames = int64(FMath::Clamp(Seconds, 0.0, Format.DurationSeconds()) * Format.SampleRate);
	ReadPos = Format.DataOffset + Frames * Format.BlockAlign;
	SeekBase = double(Frames) / Format.SampleRate;
	QueuedSinceSeek = 0;
	bEnded = false;
	if (Wave) Wave->ResetAudio();
	if (Audio) Audio->SetPaused(bPaused);
}

double UGFCWavStreamer::GetPosition() const
{
	if (!Wave || Format.SampleRate <= 0) return SeekBase;
	const int64 Played = FMath::Max<int64>(0, QueuedSinceSeek - Wave->GetAvailableAudioByteCount());
	return FMath::Min(SeekBase + double(Played) / (double(Format.SampleRate) * Format.Channels * 2), Format.DurationSeconds());
}

FString UGFCWavStreamer::Describe() const
{
	return FString::Printf(TEXT("wav %.1f/%.1fs, queued %d B, read %lld, on disk %lld%s, audio %s%s"), GetPosition(), GetDuration(),
		Wave ? Wave->GetAvailableAudioByteCount() : -1, ReadPos, OnDisk, bComplete ? TEXT(" (complete)") : TEXT(""),
		Audio ? (Audio->IsPlaying() ? TEXT("playing") : TEXT("stopped")) : TEXT("none"), bPaused ? TEXT(", paused") : TEXT(""));
}

void UGFCWavStreamer::Release()
{
	if (Audio)
	{
		Audio->Stop();
		Audio->DestroyComponent();
	}
	Audio = nullptr;
	Wave = nullptr;
	File.Reset();
	Path.Reset();
	Error.Reset();
	Format = FGFCWavFormat();
	bHeader = bComplete = bPaused = bEnded = false;
	OnDisk = ReadPos = QueuedSinceSeek = HandleSize = 0;
	SeekBase = 0.0;
}
