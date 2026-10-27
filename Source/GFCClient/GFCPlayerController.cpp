#include "GFCPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Debug/GFCDebugOverlay.h"
#include "UI/GFCRootWidget.h"

AGFCPlayerController::AGFCPlayerController()
{
	RootWidgetClass = UGFCRootWidget::StaticClass();
	bShowMouseCursor = true;
	bEnableClickEvents = true;
}

void AGFCPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController() || !RootWidgetClass)
	{
		return;
	}
	RootWidget = CreateWidget<UGFCRootWidget>(this, RootWidgetClass);
	if (RootWidget)
	{
		RootWidget->AddToViewport();
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	UGFCDebugOverlay::Install(this); // development builds only
}
