#include "Debug/GFCDebugOverlay.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "GFCCacheSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/GFCUIStyle.h"
#include "Viewer/GFCModelViewer.h"

/** Sees every touch before widgets do; three fingers down at once toggles the overlay. */
class FGFCDebugGesture : public IInputProcessor
{
public:
	TWeakObjectPtr<UGFCDebugOverlay> Overlay;
	TSet<uint32> Down;

	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
	{
		if (Event.IsTouchEvent())
		{
			Down.Add(Event.GetPointerIndex());
			if (Down.Num() == 3 && Overlay.IsValid())
			{
				Overlay->Toggle();
			}
		}
		return false; // never swallow input
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent& Event) override
	{
		if (Event.IsTouchEvent())
		{
			Down.Remove(Event.GetPointerIndex());
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("GFCDebugGesture"); }
};

namespace
{
	TWeakObjectPtr<UGFCDebugOverlay> GOverlay;

	FAutoConsoleCommand GToggleCommand(TEXT("gfc.debug"), TEXT("Shows or hides the GFC debug overlay (also: three-finger tap)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (GOverlay.IsValid())
			{
				GOverlay->Toggle();
			}
		}));
}

void UGFCDebugOverlay::Install(APlayerController* Owner)
{
#if !UE_BUILD_SHIPPING
	if (!Owner)
	{
		return;
	}
	UGFCDebugOverlay* Overlay = CreateWidget<UGFCDebugOverlay>(Owner, UGFCDebugOverlay::StaticClass());
	Overlay->AddToViewport(1000);
	Overlay->SetVisibility(FParse::Param(FCommandLine::Get(), TEXT("GFCDebug")) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	GOverlay = Overlay;
#endif
}

void UGFCDebugOverlay::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	using namespace GFCUI;
	UBorder* Bg = WidgetTree->ConstructWidget<UBorder>();
	Bg->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.78f));
	Bg->SetPadding(FMargin(8.f));
	Lines = MakeText(WidgetTree, FText::GetEmpty(), 10, Text);
	Bg->AddChild(Lines);
	// A box sized to its text at the top, not a tint over the whole screen.
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->AddChildToVerticalBox(Bg);
	WidgetTree->RootWidget = Root;
	SetPadding(FMargin(6.f, 120.f, 6.f, 0.f)); // below the top bar
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (FSlateApplication::IsInitialized())
	{
		Gesture = MakeShared<FGFCDebugGesture>();
		Gesture->Overlay = this;
		FSlateApplication::Get().RegisterInputPreProcessor(Gesture);
	}
}

void UGFCDebugOverlay::NativeDestruct()
{
	if (Gesture.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Gesture);
	}
	Gesture.Reset();
	Super::NativeDestruct();
}

void UGFCDebugOverlay::Toggle()
{
	SetVisibility(GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Refresh();
}

void UGFCDebugOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SmoothedDelta = FMath::Lerp(SmoothedDelta, FMath::Max(InDeltaTime, 0.0001f), 0.1f);
	SinceRefresh += InDeltaTime;
	if (SinceRefresh >= 0.25f)
	{
		SinceRefresh = 0.f;
		Refresh();
	}
}

void UGFCDebugOverlay::Refresh()
{
	if (!Lines || GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	TArray<FString> Out;
	const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
	Out.Add(FString::Printf(TEXT("GFC debug   %.0f fps (%.1f ms)   mem %.0f MB"), 1.f / SmoothedDelta, SmoothedDelta * 1000.f, Mem.UsedPhysical / 1048576.0));

	if (const UGFCCacheSubsystem* Cache = GetGameInstance()->GetSubsystem<UGFCCacheSubsystem>())
	{
		Out.Add(TEXT("cache  ") + Cache->GetStats().ToString());
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AGFCModelViewer> It(World); It; ++It)
		{
			Out.Add(FString::Printf(TEXT("3D     %d tris, ~%.1f MB GPU/RAM"), It->GetTriangleCount(), It->GetEstimatedBytes() / 1048576.0));
		}
	}

	if (const UGFCApiClient* C = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient())
	{
		const TArray<FGFCApiTrace>& Calls = C->GetRecentCalls();
		int32 Errors = 0;
		for (const FGFCApiTrace& T : Calls)
		{
			Errors += (T.Status == 0 || T.Status >= 400) ? 1 : 0;
		}
		Out.Add(FString::Printf(TEXT("api    %s  in flight %d  errors %d/%d"), *C->GetBaseUrl(), C->GetInFlightCount(), Errors, Calls.Num()));
		for (int32 i = Calls.Num() - 1, Shown = 0; i >= 0 && Shown < 8; --i, ++Shown)
		{
			const FGFCApiTrace& T = Calls[i];
			const FString Status = T.Status < 0 ? FString(TEXT("...")) : T.Status == 0 ? FString(TEXT("NET!")) : FString::FromInt(T.Status);
			const TCHAR* Mark = (T.Status == 0 || T.Status >= 400) ? TEXT("!") : TEXT(" ");
			Out.Add(FString::Printf(TEXT(" %s %-6s %-4s %5.0fms %s"), Mark, *T.Verb, *Status, T.Millis, *T.Path.Left(48)));
		}
	}
	Out.Add(TEXT("3-finger tap or `gfc.debug` hides this. Console: stat fps, stat unit, gfc.cache"));
	Lines->SetText(FText::FromString(FString::Join(Out, TEXT("\n"))));
}
