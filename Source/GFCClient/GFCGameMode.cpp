#include "GFCGameMode.h"

#include "GFCPlayerController.h"

AGFCGameMode::AGFCGameMode()
{
	PlayerControllerClass = AGFCPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
