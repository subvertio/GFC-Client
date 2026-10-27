#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GFCGameMode.generated.h"

/** Milestone 1 is all UI: no pawn, just the GFC player controller. */
UCLASS()
class GFCCLIENT_API AGFCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGFCGameMode();
};
