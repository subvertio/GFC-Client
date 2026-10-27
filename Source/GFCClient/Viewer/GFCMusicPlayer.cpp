#include "Viewer/GFCMusicPlayer.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GFCApi.h"
#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCApiSubsystem.h"
#include "GFCCacheSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Viewer/GFCMediaSession.h"
#include "Viewer/GFCWavStreamer.h"

void UGFCMusicPlayer::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGFCCacheSubsystem>();
	Collection.InitializeDependency<UGFCApiSubsystem>();
	Tracker.Rules = FGFCViewRules::FromSettings();
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		ReleasedHandle = Cache->OnOwnerReleased.AddUObject(this, &UGFCMusicPlayer::OnOwnerReleased);
	}
	if (UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>())
	{
		// Logged out: the music stops with everything else.
		SessionHandle = Api->GetClient()->OnSessionChanged.AddWeakLambda(this, [this]()
		{
			const UGFCApiSubsystem* A = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>();
			if (A && !A->GetClient()->IsLoggedIn()) Stop();
		});
	}
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGFCMusicPlayer::Tick), 0.1f);
}

void UGFCMusicPlayer::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		Cache->OnOwnerReleased.Remove(ReleasedHandle);
	}
	if (UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>())
	{
		Api->GetClient()->OnSessionChanged.Remove(SessionHandle);
	}
	if (Session) Session->Release();
	Session = nullptr;
	Super::Deinitialize();
}

void UGFCMusicPlayer::Play(const FGFCSeed& InSeed)
{
	if (IsCurrent(InSeed.Id) && (Session || Wav))
	{
		if (State == EGFCMusicState::Paused) TogglePause();
		return;
	}
	// A big track on mobile data waits for a second tap ("Play on mobile data").
	const int64 AskAbove = int64(GetDefault<UGFCApiSettings>()->CellularConfirmMB) * 1024 * 1024;
	const bool bConfirmed = State == EGFCMusicState::ConfirmCellular && Seed.Id == InSeed.Id;
	if (!bConfirmed && UGFCCacheSubsystem::IsOnCellular() && InSeed.FileSize > AskAbove)
	{
		Stop();
		Seed = InSeed;
		SetState(EGFCMusicState::ConfirmCellular, FString::Printf(TEXT("This track is %.0f MB. Tap Play again to stream it on mobile data."),
			double(InSeed.FileSize) / (1024.0 * 1024.0)));
		return;
	}
	Stop();
	UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>();
	UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	// A compressed streaming copy, when the node provides one, beats the original.
	const FString Source = InSeed.Stream.IsEmpty() ? InSeed.Full : InSeed.Stream;
	if (!Cache || !C || Source.IsEmpty())
	{
		return;
	}
	Seed = InSeed;
	SetState(EGFCMusicState::Loading);
	const uint32 Serial = ++LoadSerial;
	const FString Url = C->ResolveMediaUrl(Source);
	FString Ext = FPaths::GetExtension(Url.Left(Url.Find(TEXT("?")) >= 0 ? Url.Find(TEXT("?")) : Url.Len()), false).ToLower();
	const bool bWav = Ext == TEXT("wav") || Ext == TEXT("wave");
	if (bWav)
	{
		// The engine plays WAV itself, from the file while it downloads (the platform player can't on Android).
		Wav = NewObject<UGFCWavStreamer>(this);
		Wav->SetLooping(bLoop);
	}
	TWeakObjectPtr<UGFCMusicPlayer> WeakThis(this);
	auto Progress = [WeakThis, Serial](const FString& Path, int64 OnDisk)
	{
		if (!WeakThis.IsValid() || WeakThis->LoadSerial != Serial || !WeakThis->Wav) return;
		if (!WeakThis->bWavBegun)
		{
			WeakThis->Wav->Begin(WeakThis->GetGameInstance()->GetWorld(), Path);
			WeakThis->bWavBegun = true;
		}
		WeakThis->Wav->SetBytesOnDisk(OnDisk, false);
	};
	// The one live heavy item: a video or 3D model opened later takes over (and stops the music).
	Cache->FetchFile(Url, CacheOwner, int64(GetDefault<UGFCApiSettings>()->MaxDiskCacheMB) * 1024 * 1024 / 2, true,
		[WeakThis, Serial](EGFCFetchResult Result, const FString& Path, int64 Bytes)
		{
			if (!WeakThis.IsValid() || WeakThis->LoadSerial != Serial)
			{
				return;
			}
			if (Result != EGFCFetchResult::Ok)
			{
				WeakThis->Fail(Result == EGFCFetchResult::LowDiskSpace ? TEXT("Not enough free space on the phone for this track.")
					: Result == EGFCFetchResult::TooBig ? TEXT("This track is too big to play on a phone.")
					: TEXT("Couldn't download this track. Check your connection and try again."));
				return;
			}
			if (WeakThis->Wav)
			{
				if (!WeakThis->bWavBegun)
				{
					WeakThis->Wav->Begin(WeakThis->GetGameInstance()->GetWorld(), Path);
					WeakThis->bWavBegun = true;
				}
				WeakThis->Wav->SetBytesOnDisk(Bytes, true);
				return;
			}
			WeakThis->OpenWithPlatformPlayer(Path);
		}, bWav ? UGFCCacheSubsystem::FOnFileProgress(Progress) : nullptr);
}

void UGFCMusicPlayer::OpenWithPlatformPlayer(const FString& Path)
{
	UGFCMediaSession* S = NewObject<UGFCMediaSession>(this);
	S->SetMuted(false);
	if (!S->Open(GetGameInstance()->GetWorld(), Path))
	{
		S->Release();
		// Not something the phone's player knows: if it's really a WAV file, the engine plays it.
		TArray<uint8> Head;
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));
		if (Reader && Reader->TotalSize() >= 12)
		{
			Head.SetNumUninitialized(12);
			Reader->Serialize(Head.GetData(), 12);
		}
		Reader.Reset();
		if (Head.Num() == 12 && UGFCWavStreamer::LooksLikeWav(Head.GetData(), Head.Num()))
		{
			Wav = NewObject<UGFCWavStreamer>(this);
			Wav->SetLooping(bLoop);
			Wav->Begin(GetGameInstance()->GetWorld(), Path);
			Wav->SetBytesOnDisk(IFileManager::Get().FileSize(*Path), true);
			bWavBegun = true;
			return;
		}
		Fail(TEXT("This phone can't play this audio format."));
		return;
	}
	S->SetLooping(bLoop);
	Session = S;
	BeginListen();
	SetState(EGFCMusicState::Playing);
}

void UGFCMusicPlayer::BeginListen()
{
	const UGFCApiClient* Client = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	Tracker.Begin(Seed.Id, EGFCMediaKind::Audio, Client->IsLoggedIn() && Seed.Owner.Equals(Client->GetUsername(), ESearchCase::IgnoreCase));
}

void UGFCMusicPlayer::Fail(const FString& Message)
{
	if (Session) { Session->Release(); Session = nullptr; }
	if (Wav) { Wav->Release(); Wav = nullptr; }
	bWavBegun = false;
	UE_LOG(LogGFCApi, Warning, TEXT("Music: %s"), *Message);
	SetState(EGFCMusicState::Failed, Message);
}

void UGFCMusicPlayer::TogglePause()
{
	if (State == EGFCMusicState::ConfirmCellular || (State == EGFCMusicState::Failed && !Seed.Id.IsEmpty()))
	{
		const FGFCSeed Again = Seed;
		if (State == EGFCMusicState::Failed) State = EGFCMusicState::Idle;
		Play(Again);
		return;
	}
	if (Wav)
	{
		if (!Wav->HasStarted()) return; // still buffering the start
		const bool bNowPaused = Wav->IsPlaying();
		Wav->SetPaused(bNowPaused);
		SetState(bNowPaused ? EGFCMusicState::Paused : EGFCMusicState::Playing);
		return;
	}
	if (!Session)
	{
		return;
	}
	// Finished (not looping): Play starts the track again from the top.
	const float Duration = Session->GetDurationSeconds();
	if (!Session->IsPlaying() && Duration > 0.f && Session->GetPositionSeconds() >= Duration - 0.3f)
	{
		Session->Seek(0.f);
	}
	Session->TogglePause();
	SetState(Session->IsPlaying() ? EGFCMusicState::Playing : EGFCMusicState::Paused);
}

void UGFCMusicPlayer::Seek(float Seconds)
{
	const float Duration = GetDuration();
	const float To = FMath::Clamp(Seconds, 0.f, Duration > 0.f ? Duration - 0.25f : 0.f);
	if (Wav) Wav->Seek(To);
	else if (Session) Session->Seek(To);
}

void UGFCMusicPlayer::SetLooping(bool bInLoop)
{
	bLoop = bInLoop;
	if (Session) Session->SetLooping(bLoop);
	if (Wav) Wav->SetLooping(bLoop);
	OnChanged.Broadcast();
}

void UGFCMusicPlayer::Stop()
{
	++LoadSerial; // a download still in flight is ignored
	if (Session)
	{
		Session->Release();
		Session = nullptr;
	}
	if (Wav)
	{
		Wav->Release();
		Wav = nullptr;
	}
	bWavBegun = false;
	Tracker.End();
	if (State != EGFCMusicState::Idle)
	{
		if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
		{
			State = EGFCMusicState::Idle; // set first: ReleaseOwner calls back into OnOwnerReleased
			Cache->ReleaseOwner(CacheOwner); // cancels the download and deletes the file
		}
	}
	Seed = FGFCSeed();
	SetState(EGFCMusicState::Idle);
}

float UGFCMusicPlayer::GetPosition() const
{
	return Wav ? float(Wav->GetPosition()) : Session ? Session->GetPositionSeconds() : 0.f;
}

float UGFCMusicPlayer::GetDuration() const
{
	return Wav ? float(Wav->GetDuration()) : Session ? Session->GetDurationSeconds() : 0.f;
}

FString UGFCMusicPlayer::FormatTime(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
	const int32 H = Total / 3600, M = (Total / 60) % 60, S = Total % 60;
	return H > 0 ? FString::Printf(TEXT("%d:%02d:%02d"), H, M, S) : FString::Printf(TEXT("%d:%02d"), M, S);
}

bool UGFCMusicPlayer::Tick(float DeltaSeconds)
{
	if (Wav && bWavBegun)
	{
		const bool bWasStarted = Wav->HasStarted();
		if (!Wav->Tick())
		{
			Fail(Wav->GetError());
			return true;
		}
		if (!bWasStarted && Wav->HasStarted())
		{
			BeginListen();
			SetState(EGFCMusicState::Playing);
		}
		LogTimer += DeltaSeconds;
		if (LogTimer >= 2.f) { LogTimer = 0.f; UE_LOG(LogGFCApi, Log, TEXT("Music: %s"), *Wav->Describe()); }
		if (State == EGFCMusicState::Playing && Wav->HasEnded())
		{
			SetState(EGFCMusicState::Paused); // at the end: Play starts again
		}
	}
	else if (Session)
	{
		const bool bPlaying = Session->IsPlaying();
		// Reached the end (not looping): show it as paused, ready to play again.
		if (State == EGFCMusicState::Playing && !bPlaying && bWasPlaying)
		{
			SetState(EGFCMusicState::Paused);
		}
		bWasPlaying = bPlaying;
	}
	if ((Wav || Session) && Tracker.NotifyPlayback(GetPosition(), GetDuration()))
	{
		UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
		if (C->IsLoggedIn())
		{
			C->RecordPlay(Seed.Id, [](const FGFCApiResult& R, const FGFCPlayResult& Play)
			{
				UE_LOG(LogGFCApi, Log, TEXT("Listen counted: %s"), R.bOk ? (Play.bCredited ? TEXT("credited") : *Play.Reason) : *R.Error);
			});
		}
	}
	return true;
}

void UGFCMusicPlayer::SetState(EGFCMusicState NewState, const FString& InError)
{
	State = NewState;
	Error = InError;
	OnChanged.Broadcast();
}

void UGFCMusicPlayer::OnOwnerReleased(const FString& Owner)
{
	// Another heavy item took over, the app was backgrounded, or the cache was wiped.
	if (Owner == CacheOwner && State != EGFCMusicState::Idle)
	{
		Stop();
	}
}

// Development: GFC.Music.Play <seed id> plays a post as if its Play button was tapped; GFC.Music.Stop stops.
static FAutoConsoleCommandWithWorldAndArgs GGFCMusicPlay(
	TEXT("GFC.Music.Play"),
	TEXT("Plays an audio post in the music player: GFC.Music.Play <seed id>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		if (!GI || Args.Num() < 1) return;
		TWeakObjectPtr<UGameInstance> WeakGI(GI);
		GI->GetSubsystem<UGFCApiSubsystem>()->GetClient()->GetSeed(Args[0], [WeakGI](const FGFCApiResult& R, const FGFCSeed& S)
		{
			if (R.bOk && WeakGI.IsValid()) WeakGI->GetSubsystem<UGFCMusicPlayer>()->Play(S);
		});
	}));

static FAutoConsoleCommandWithWorld GGFCMusicStop(
	TEXT("GFC.Music.Stop"),
	TEXT("Stops the music player."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UGameInstance* GI = World ? World->GetGameInstance() : nullptr) GI->GetSubsystem<UGFCMusicPlayer>()->Stop();
	}));