#include "UI/GFCRewardBurst.h"

#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GFCApi.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "Styling/SlateBrush.h"
#include "UI/GFCUIStyle.h"

#define LOCTEXT_NAMESPACE "GFCReward"

using namespace GFCUI;

namespace
{
	uint32 U32(const TArray<uint8>& B, int32 At) { return uint32(B[At]) | uint32(B[At + 1]) << 8 | uint32(B[At + 2]) << 16 | uint32(B[At + 3]) << 24; }
	uint16 U16(const TArray<uint8>& B, int32 At) { return uint16(B[At] | B[At + 1] << 8); }

	float EaseOutBack(float X)
	{
		const float C1 = 1.70158f, C3 = C1 + 1.f;
		return 1.f + C3 * FMath::Pow(X - 1.f, 3.f) + C1 * FMath::Pow(X - 1.f, 2.f);
	}
}

bool UGFCRewardBurst::ParseWav(const TArray<uint8>& Bytes, TArray<uint8>& OutPcm, int32& OutSampleRate, int32& OutChannels)
{
	OutPcm.Reset();
	if (Bytes.Num() < 12 || FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) != 0)
	{
		return false;
	}
	bool bFormatOk = false;
	for (int32 At = 12; At + 8 <= Bytes.Num();)
	{
		const uint32 Size = U32(Bytes, At + 4);
		const int32 Body = At + 8;
		if (Size > uint32(Bytes.Num() - Body))
		{
			return false; // truncated
		}
		if (FMemory::Memcmp(Bytes.GetData() + At, "fmt ", 4) == 0 && Size >= 16)
		{
			const uint16 Tag = U16(Bytes, Body), Bits = U16(Bytes, Body + 14);
			OutChannels = U16(Bytes, Body + 2);
			OutSampleRate = int32(U32(Bytes, Body + 4));
			bFormatOk = Tag == 1 && Bits == 16 && OutChannels >= 1 && OutChannels <= 2 && OutSampleRate >= 8000 && OutSampleRate <= 96000;
		}
		else if (FMemory::Memcmp(Bytes.GetData() + At, "data", 4) == 0)
		{
			if (!bFormatOk)
			{
				return false;
			}
			const int32 Frame = 2 * OutChannels;
			OutPcm.Append(Bytes.GetData() + Body, int32(Size) - int32(Size) % Frame);
			return OutPcm.Num() > 0;
		}
		At = Body + int32(Size) + (Size & 1); // chunks are word-aligned
	}
	return false;
}

void UGFCRewardBurst::PlayKaChing(UObject* WorldContext)
{
	static TArray<uint8> Pcm;
	static int32 Rate = 0, Channels = 0;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		TArray<uint8> Bytes;
		const FString Path = FPaths::ProjectContentDir() / TEXT("GFC/Sounds/ka_ching.wav");
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent) || !ParseWav(Bytes, Pcm, Rate, Channels))
		{
			UE_LOG(LogGFCApi, Warning, TEXT("Ka-ching sound missing or not 16-bit PCM: %s"), *Path);
			Pcm.Reset();
		}
	}
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || Pcm.IsEmpty())
	{
		return;
	}
	// A procedural wave is used up as it plays, so each ka-ching gets a fresh one.
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>();
	Wave->SetSampleRate(Rate);
	Wave->NumChannels = Channels;
	Wave->Duration = float(Pcm.Num()) / float(Rate * Channels * 2);
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	if (UAudioComponent* Audio = UGameplayStatics::SpawnSound2D(World, Wave))
	{
		// Procedural sound doesn't end by itself: stop it when the samples run out.
		TWeakObjectPtr<UAudioComponent> Weak(Audio);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, [Weak]() { if (Weak.IsValid()) Weak->Stop(); }, Wave->Duration + 0.1f, false);
	}
}

UGFCRewardBurst* UGFCRewardBurst::Play(UObject* WorldContext, double InAmount, const FText& InCaption)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !World->GetGameInstance())
	{
		return nullptr;
	}
	UGFCRewardBurst* Burst = CreateWidget<UGFCRewardBurst>(World->GetGameInstance(), UGFCRewardBurst::StaticClass());
	Burst->Amount = InAmount;
	Burst->Caption = InCaption;
	if (Burst->AmountText)
	{
		FNumberFormattingOptions O;
		O.MaximumFractionalDigits = 2;
		Burst->AmountText->SetText(FText::Format(LOCTEXT("Amount", "+{0} GFC"), FText::AsNumber(InAmount, &O)));
	}
	if (Burst->CaptionText) Burst->CaptionText->SetText(InCaption);
	Burst->AddToViewport(1000); // above every screen and the debug overlay
	PlayKaChing(WorldContext);
	return Burst;
}

UWidget* UGFCRewardBurst::MakeCoin(float Size)
{
	// A gold coin: a bright face, a darker rim, and the G stamped in the middle.
	const FLinearColor Gold = FGFCTheme::Get().Gold, Rim = FGFCTheme::Get().GoldDark;
	UBorder* Coin = WidgetTree->ConstructWidget<UBorder>();
	Coin->SetBrush(FSlateRoundedBoxBrush(Gold, Size * 0.5f, Rim, FMath::Max(2.f, Size * 0.09f)));
	Coin->SetPadding(FMargin(0.f));
	UTextBlock* G = MakeText(WidgetTree, FText::FromString(TEXT("G")), FMath::RoundToInt(Size * 0.42f), Rim, true);
	G->SetAutoWrapText(false);
	G->SetJustification(ETextJustify::Center);
	Coin->SetContent(G);
	if (UBorderSlot* S = Cast<UBorderSlot>(G->Slot))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	return Coin;
}

UWidget* UGFCRewardBurst::MakeSparkle(float Size)
{
	// A twinkle: a small white diamond (a rotated square) with a soft gold edge.
	UBorder* Spark = WidgetTree->ConstructWidget<UBorder>();
	Spark->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, Size * 0.18f, FGFCTheme::Get().Gold, 1.5f));
	return Spark;
}

void UGFCRewardBurst::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible); // taps go straight through to the app
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	AmountText = MakeText(WidgetTree, FText::GetEmpty(), 46, FGFCTheme::Get().Gold, true);
	AmountText->SetAutoWrapText(false);
	AmountText->SetJustification(ETextJustify::Center);
	AmountText->SetShadowOffset(FVector2D(0.f, 3.f));
	AmountText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	CaptionText = MakeText(WidgetTree, FText::GetEmpty(), 20, GFCUI::Text, true);
	CaptionText->SetJustification(ETextJustify::Center);
	CaptionText->SetShadowOffset(FVector2D(0.f, 2.f));
	CaptionText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	for (UTextBlock* T : {AmountText.Get(), CaptionText.Get()})
	{
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(T))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
	}
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f, 0.38f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
		S->SetZOrder(10);
	}
	Banner = Col;
	Banner->SetRenderOpacity(0.f);
}

void UGFCRewardBurst::Launch(const FVector2D& Area)
{
	bLaunched = true;
	// Coins burst from the bottom bar's + Post button and rise past the top, fanning out.
	const FVector2D Origin(Area.X * 0.5f, Area.Y * 0.92f);
	for (int32 i = 0; i < 22; ++i)
	{
		FParticle P;
		P.Size = FMath::FRandRange(30.f, 52.f);
		P.Widget = MakeCoin(P.Size);
		P.Pos = Origin + FVector2D(FMath::FRandRange(-40.f, 40.f), 0.f);
		P.Vel = FVector2D(FMath::FRandRange(-260.f, 260.f), -FMath::FRandRange(Area.Y * 0.9f, Area.Y * 1.35f));
		P.Delay = i * 0.045f + FMath::FRandRange(0.f, 0.05f);
		P.Life = FMath::FRandRange(1.5f, 2.2f);
		P.Spin = FMath::FRandRange(7.f, 13.f) * (FMath::RandBool() ? 1.f : -1.f);
		P.Phase = FMath::FRandRange(0.f, PI);
		Particles.Add(P);
	}
	for (int32 i = 0; i < 46; ++i)
	{
		FParticle P;
		P.bSparkle = true;
		P.Size = FMath::FRandRange(8.f, 16.f);
		P.Widget = MakeSparkle(P.Size);
		// Twinkles are scattered along the coins' path and around the amount.
		P.Pos = FVector2D(Area.X * FMath::FRandRange(0.12f, 0.88f), Area.Y * FMath::FRandRange(0.12f, 0.85f));
		P.Vel = FVector2D(0.f, -FMath::FRandRange(20.f, 60.f));
		P.Delay = FMath::FRandRange(0.15f, Duration - 0.9f);
		P.Life = FMath::FRandRange(0.45f, 0.8f);
		P.Phase = FMath::FRandRange(0.f, 2.f * PI);
		Particles.Add(P);
	}
	for (FParticle& P : Particles)
	{
		Owned.Add(P.Widget);
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(P.Widget))
		{
			S->SetSize(FVector2D(P.Size, P.Size));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetZOrder(P.bSparkle ? 5 : 1);
		}
		P.Widget->SetRenderOpacity(0.f);
		P.Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UGFCRewardBurst::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Dt = FMath::Min(InDeltaTime, 1.f / 20.f); // a hitch mustn't teleport the coins
	Time += Dt;
	FVector2D Area = MyGeometry.GetLocalSize();
	if (Area.X < 10.f || Area.Y < 10.f)
	{
		return; // not laid out yet
	}
	if (!bLaunched)
	{
		Launch(Area);
	}

	for (FParticle& P : Particles)
	{
		const float T = Time - P.Delay;
		if (T < 0.f)
		{
			continue;
		}
		if (T > P.Life)
		{
			P.Widget->SetRenderOpacity(0.f);
			continue;
		}
		const float K = T / P.Life;
		if (P.bSparkle)
		{
			// Grow, flash and shrink away while turning a little.
			const float Twinkle = FMath::Sin(K * PI);
			P.Pos += P.Vel * Dt;
			P.Widget->SetRenderTranslation(P.Pos);
			P.Widget->SetRenderTransformAngle(45.f + K * 90.f);
			P.Widget->SetRenderScale(FVector2D(Twinkle * 1.2f, Twinkle * 1.2f));
			P.Widget->SetRenderOpacity(Twinkle);
		}
		else
		{
			// Up fast, slowed by a light gravity; a coin flip is a squash on X.
			P.Vel.Y += 900.f * Dt;
			P.Vel.X *= FMath::Pow(0.6f, Dt);
			P.Pos += P.Vel * Dt;
			const float Flip = FMath::Cos(T * P.Spin + P.Phase);
			P.Widget->SetRenderTranslation(P.Pos);
			P.Widget->SetRenderScale(FVector2D(FMath::Max(0.08f, FMath::Abs(Flip)), 1.f));
			P.Widget->SetRenderTransformAngle(FMath::Sin(T * 3.f + P.Phase) * 12.f);
			P.Widget->SetRenderOpacity(K < 0.08f ? K / 0.08f : (K > 0.7f ? (1.f - K) / 0.3f : 1.f));
		}
	}

	if (Banner)
	{
		// "+1,000 GFC" pops in with a little overshoot, holds, then fades.
		const float In = FMath::Clamp((Time - 0.1f) / 0.45f, 0.f, 1.f);
		const float Out = FMath::Clamp((Duration - Time) / 0.5f, 0.f, 1.f);
		const float Scale = In > 0.f ? EaseOutBack(In) : 0.f;
		Banner->SetRenderScale(FVector2D(Scale, Scale));
		Banner->SetRenderOpacity(FMath::Min(In * 2.f, 1.f) * Out);
	}

	if (Time >= Duration)
	{
		RemoveFromParent();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GGFCCelebrate(
	TEXT("GFC.Celebrate"),
	TEXT("Previews the GFC reward burst and ka-ching. GFC.Celebrate [amount], default 1000."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const double Amount = Args.Num() ? FCString::Atod(*Args[0]) : 1000.0;
		UGFCRewardBurst::Play(World, Amount > 0.0 ? Amount : 1000.0, LOCTEXT("Preview", "First post bonus!"));
	}));

#undef LOCTEXT_NAMESPACE
