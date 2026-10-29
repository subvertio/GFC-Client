#include "Viewer/GFCMediaSession.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Components/Image.h"

bool UGFCMediaSession::Open(UWorld* World, const FString& FilePath)
{
	Release();
	if (!World)
	{
		return false;
	}
	Player = NewObject<UMediaPlayer>(this);
	Player->PlayOnOpen = true;
	Player->SetLooping(false);

	Texture = NewObject<UMediaTexture>(this);
	Texture->AutoClear = true;
	Texture->ClearColor = FLinearColor::Black;
	Texture->SetMediaPlayer(Player);
	Texture->UpdateResource();

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	SoundOwner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (SoundOwner)
	{
		Sound = NewObject<UMediaSoundComponent>(SoundOwner);
		Sound->SetMediaPlayer(Player);
		Sound->bIsUISound = true;
		Sound->RegisterComponent();
		Sound->SetVolumeMultiplier(bMuted ? 0.f : 1.f);
		Sound->Start();
	}
	return Player->OpenFile(FilePath);
}

float UGFCMediaSession::GetPositionSeconds() const
{
	return Player ? float(Player->GetTime().GetTotalSeconds()) : 0.f;
}

float UGFCMediaSession::GetDurationSeconds() const
{
	return Player ? float(Player->GetDuration().GetTotalSeconds()) : 0.f;
}

bool UGFCMediaSession::IsPlaying() const
{
	return Player && Player->IsPlaying();
}

void UGFCMediaSession::TogglePause()
{
	if (!Player)
	{
		return;
	}
	if (Player->IsPlaying())
	{
		Player->Pause();
	}
	else
	{
		Player->Play();
	}
}

void UGFCMediaSession::Seek(float Seconds)
{
	if (Player && Player->SupportsSeeking())
	{
		Player->Seek(FTimespan::FromSeconds(FMath::Max(0.f, Seconds)));
	}
}
void UGFCMediaSession::SetLooping(bool bLoop)
{
	if (Player) Player->SetLooping(bLoop);
}

void UGFCMediaSession::FitBrush(UImage* Image) const
{
	if (!Image || !Texture)
	{
		return;
	}
	const FVector2D Size(Texture->GetWidth(), Texture->GetHeight());
	if (Size.X < 2.0 || Size.Y < 2.0 || Image->GetBrush().ImageSize == Size)
	{
		return; // no frame yet, or already right
	}
	FSlateBrush Brush = Image->GetBrush();
	Brush.ImageSize = Size;
	Image->SetBrush(Brush);
}

void UGFCMediaSession::SetMuted(bool bInMuted)
{
	bMuted = bInMuted;
	if (Sound)
	{
		Sound->SetVolumeMultiplier(bMuted ? 0.f : 1.f);
	}
}

void UGFCMediaSession::Release()
{
	if (Player)
	{
		Player->Close();
	}
	if (Sound)
	{
		Sound->Stop();
		Sound->SetMediaPlayer(nullptr);
		Sound->DestroyComponent();
	}
	if (SoundOwner)
	{
		SoundOwner->Destroy();
	}
	if (Texture)
	{
		Texture->SetMediaPlayer(nullptr);
		Texture->ReleaseResource();
	}
	Player = nullptr;
	Texture = nullptr;
	Sound = nullptr;
	SoundOwner = nullptr;
}
