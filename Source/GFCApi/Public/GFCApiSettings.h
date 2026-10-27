#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GFCApiSettings.generated.h"

/**
 * Project Settings > Game > GFC API. Stored in Config/DefaultGame.ini under
 * [/Script/GFCApi.GFCApiSettings].
 *
 * Override the node at launch with -GFCNode=http://127.0.0.1:8000 (handy for a
 * local test node), or per user from the login screen.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GFC API"))
class GFCAPI_API UGFCApiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGFCApiSettings();

	/** The node the client talks to. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Node")
	FString NodeBaseUrl;

	/** Where public share pages live (/p/{id}). Point people at the gateway, not a home node. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sharing")
	FString ShareBaseUrl;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (ClampMin = "2", ClampMax = "300"))
	float RequestTimeoutSeconds = 30.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** NodeBaseUrl, unless -GFCNode= is on the command line. Falls back to the built-in default if the ini value isn't a URL. */
	static FString GetEffectiveNodeBaseUrl();

	/** ShareBaseUrl, or the built-in default if the ini value isn't a URL. */
	static FString GetEffectiveShareBaseUrl();

	static const TCHAR* DefaultNodeBaseUrl;
	static const TCHAR* DefaultShareBaseUrl;
};
