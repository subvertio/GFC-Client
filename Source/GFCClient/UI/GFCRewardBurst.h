#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GFCRewardBurst.generated.h"

class UCanvasPanel;
class UTextBlock;
class UWidget;

/**
 * The "you earned GFC" moment: a ka-ching and a burst of gold coins that fly up the
 * screen, spin and sparkle, under a big "+1,000 GFC". It floats over everything for
 * about three seconds, never takes a tap, then removes itself.
 *
 * Play it with UGFCRewardBurst::Play. In Development builds, `GFC.Celebrate [amount]`
 * previews it (on the phone: adb shell am broadcast -a android.intent.action.RUN -e cmd "GFC.Celebrate 1000").
 */
UCLASS()
class GFCCLIENT_API UGFCRewardBurst : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the burst over the whole viewport and plays the ka-ching. Returns the widget, or null without a world. */
	static UGFCRewardBurst* Play(UObject* WorldContext, double Amount, const FText& Caption);

	/** The ka-ching (Content/GFC/Sounds/ka_ching.wav, staged as a plain file). Safe to call when it's missing. */
	static void PlayKaChing(UObject* WorldContext);

	/** 16-bit PCM from a .wav file (walks the chunks; LIST and other chunks are skipped). */
	static bool ParseWav(const TArray<uint8>& Bytes, TArray<uint8>& OutPcm, int32& OutSampleRate, int32& OutChannels);

	/** How long the burst stays up, in seconds. */
	static constexpr float Duration = 3.f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FParticle
	{
		UWidget* Widget = nullptr;
		FVector2D Pos = FVector2D::ZeroVector;
		FVector2D Vel = FVector2D::ZeroVector;
		float Delay = 0.f, Life = 0.f, Age = 0.f, Spin = 0.f, Size = 1.f, Phase = 0.f;
		bool bSparkle = false;
	};

	void Launch(const FVector2D& Area);
	UWidget* MakeCoin(float Size);
	UWidget* MakeSparkle(float Size);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UWidget> Banner;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AmountText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CaptionText;
	/** Keeps the particle widgets alive (FParticle holds raw pointers into this). */
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> Owned;

	TArray<FParticle> Particles;
	double Amount = 0.0;
	FText Caption;
	float Time = 0.f;
	bool bLaunched = false;
};
