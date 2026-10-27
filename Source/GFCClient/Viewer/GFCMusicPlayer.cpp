#include "Viewer/GFCMusicPlayer.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GFCApi.h"
#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCApiSubsystem.h"
#include "GFCCacheSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Viewer/GFCMediaSession.h"

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
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGFCMusicPlayer::Tick), 0.25f);
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
	if (IsCurrent(InSeed.Id) && Session)
	{
		if (State == EGFCMusicState::Paused) TogglePause();
		return;
	}
	Stop();
	UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>();
	UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	if (!Cache || !C || InSeed.Full.IsEmpty())
	{
		return;
	}
	Seed = InSeed;
	SetState(EGFCMusicState::Loading);
	const uint32 Serial = ++LoadSerial;
	TWeakObjectPtr<UGFCMusicPlayer> WeakThis(this);
	// The one live heavy item: a video or 3D model opened later takes over (and stops the music).
	Cache->FetchFile(C->ResolveMediaUrl(Seed.Full), CacheOwner, int64(GetDefault<UGFCApiSettings>()->MaxDiskCacheMB) * 1024 * 1024 / 2, true,
		[WeakThis, Serial](EGFCFetchResult Result, const FString& Path, int64)
		{
			if (!WeakThis.IsValid() || WeakThis->LoadSerial != Serial)
			{
				return;
			}
			if (Result != EGFCFetchResult::Ok)
			{
				WeakThis->SetState(EGFCMusicState::Failed, TEXT("Couldn't download this track. Check your connection and try again."));
				return;
			}
			UGFCMediaSession* S = NewObject<UGFCMediaSession>(WeakThis.Get());
			S->SetMuted(false);
			if (!S->Open(WeakThis->GetGameInstance()->GetWorld(), Path))
			{
				S->Release();
				WeakThis->SetState(EGFCMusicState::Failed, TEXT("This phone can't play this audio format."));
				return;
			}
			S->SetLooping(WeakThis->bLoop);
			WeakThis->Session = S;
			const UGFCApiClient* Client = WeakThis->GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
			WeakThis->Tracker.Begin(WeakThis->Seed.Id, EGFCMediaKind::Audio,
				Client->IsLoggedIn() && WeakThis->Seed.Owner.Equals(Client->GetUsername(), ESearchCase::IgnoreCase));
			WeakThis->SetState(EGFCMusicState::Playing);
		});
}

void UGFCMusicPlayer::TogglePause()
{
	if (!Session)
	{
		if (State == EGFCMusicState::Failed && !Seed.Id.IsEmpty())
		{
			const FGFCSeed Retry = Seed;
			State = EGFCMusicState::Idle;
			Play(Retry);
		}
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
	if (Session)
	{
		const float Duration = Session->GetDurationSeconds();
		Session->Seek(FMath::Clamp(Seconds, 0.f, Duration > 0.f ? Duration - 0.25f : 0.f));
	}
}

void UGFCMusicPlayer::SetLooping(bool bInLoop)
{
	bLoop = bInLoop;
	if (Session) Session->SetLooping(bLoop);
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
	Tracker.End();
	if (State != EGFCMusicState::Idle)
	{
		if (UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
		{
			State = EGFCMusicState::Idle; // set first: ReleaseOwner calls back into OnOwnerReleased
			Cache->ReleaseOwner(CacheOwner);
		}
	}
	Seed = FGFCSeed();
	SetState(EGFCMusicState::Idle);
}

float UGFCMusicPlayer::GetPosition() const
{
	return Session ? Session->GetPositionSeconds() : 0.f;
}

float UGFCMusicPlayer::GetDuration() const
{
	return Session ? Session->GetDurationSeconds() : 0.f;
}

FString UGFCMusicPlayer::FormatTime(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
	const int32 H = Total / 3600, M = (Total / 60) % 60, S = Total % 60;
	return H > 0 ? FString::Printf(TEXT("%d:%02d:%02d"), H, M, S) : FString::Printf(TEXT("%d:%02d"), M, S);
}

bool UGFCMusicPlayer::Tick(float DeltaSeconds)
{
	if (!Session)
	{
		return true;
	}
	const bool bPlaying = Session->IsPlaying();
	// Reached the end (not looping): show it as paused at the start, ready to play again.
	if (State == EGFCMusicState::Playing && !bPlaying && bWasPlaying)
	{
		SetState(EGFCMusicState::Paused);
	}
	bWasPlaying = bPlaying;
	if (Tracker.NotifyPlayback(Session->GetPositionSeconds(), Session->GetDurationSeconds()))
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