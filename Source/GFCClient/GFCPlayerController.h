#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GFCPlayerController.generated.h"

class UGFCRootWidget;

/** Shows the GFC UI with a mouse cursor and UI-only input. */
UCLASS()
class GFCCLIENT_API AGFCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGFCPlayerController();

	/** Replace with a Blueprint subclass of UGFCRootWidget to restyle the app shell. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GFC")
	TSubclassOf<UGFCRootWidget> RootWidgetClass;

	UFUNCTION(BlueprintPure, Category = "GFC")
	UGFCRootWidget* GetRootWidget() const { return RootWidget; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGFCRootWidget> RootWidget;
};
