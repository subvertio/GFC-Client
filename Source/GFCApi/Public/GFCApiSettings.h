#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GFCApiSettings.generated.h"

/**
 * Project Settings > Game > GFC API. Stored in Config/DefaultGame.ini under
 * [/Script/GFCApi.GFCApiSettings]. Keep URLs in quotes in the ini.
 *
 * Override the node at launch with -GFCNode=http://127.0.0.1:8000 (handy for a
 * local test node), or per user from the login screen.
 *
 * The budgets below are the client's half of the mobile limits in docs/roadmap.md;
 * the node should enforce matching limits on upload.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GFC API"))
class GFCAPI_API UGFCApiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGFCApiSettings();

	// ---- Node and links ----

	/** The node the client talks to. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Node")
	FString NodeBaseUrl;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (ClampMin = "2", ClampMax = "300"))
	float RequestTimeoutSeconds = 30.f;

	/** Where public share pages live: {ShareBaseUrl}/p/{id}. Its host must be in AppLinkHosts. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Links")
	FString ShareBaseUrl;

	/**
	 * Hosts whose https://{host}/p/{id} links open a seed in the app. The Android
	 * intent filter (GFCApi_UPL.xml) claims the first one with autoVerify.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Links")
	TArray<FString> AppLinkHosts;

	/** Custom scheme fallback: {scheme}://seed/{id}. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Links")
	FString AppLinkScheme = TEXT("gfc");

	// ---- Cache (docs/cache-policy.md) ----

	/** Hard cap for downloaded media and models on disk, in MB. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache", meta = (ClampMin = "10"))
	int32 MaxDiskCacheMB = 200;

	/** In-memory thumbnails, by count and by decoded size. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache", meta = (ClampMin = "4"))
	int32 MaxThumbnails = 60;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache", meta = (ClampMin = "4"))
	int32 MaxThumbnailMB = 48;

	/** Refuse downloads that would leave less than this free on the device, in MB. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache", meta = (ClampMin = "50"))
	int32 MinFreeDiskMB = 300;

	/** Ask before downloading more than this on mobile data, in MB. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache", meta = (ClampMin = "0"))
	int32 CellularConfirmMB = 10;

	/** Seconds a card may be off screen before its content is released (scroll jitter grace). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Cache", meta = (ClampMin = "0"))
	float OffscreenGraceSeconds = 0.75f;

	// ---- 3D and media budgets ----

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Budgets", meta = (ClampMin = "1"))
	int32 MaxModelMB = 50;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Budgets", meta = (ClampMin = "1000"))
	int32 MaxModelTriangles = 300000;

	/** Bigger textures are downscaled to fit. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Budgets", meta = (ClampMin = "64"))
	int32 MaxTextureSize = 2048;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Budgets", meta = (ClampMin = "5"))
	float ModelLoadTimeoutSeconds = 45.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Budgets", meta = (ClampMin = "1"))
	int32 MaxImageMB = 20;

	// ---- Qualified views (rewarded plays) ----

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Views", meta = (ClampMin = "0"))
	float ImageViewSeconds = 2.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Views", meta = (ClampMin = "0"))
	float ModelViewSeconds = 3.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Views", meta = (ClampMin = "0"))
	float MediaPlaySeconds = 5.f;

	/** Clips shorter than this count after ShortClipFraction of their length. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Views", meta = (ClampMin = "0"))
	float ShortClipSeconds = 10.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Views", meta = (ClampMin = "0", ClampMax = "1"))
	float ShortClipFraction = 0.5f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** NodeBaseUrl, unless -GFCNode= is on the command line. Falls back to the built-in default if the ini value isn't a URL. */
	static FString GetEffectiveNodeBaseUrl();

	/** ShareBaseUrl, or the built-in default if the ini value isn't a URL. */
	static FString GetEffectiveShareBaseUrl();

	static const TCHAR* DefaultNodeBaseUrl;
	static const TCHAR* DefaultShareBaseUrl;
};
