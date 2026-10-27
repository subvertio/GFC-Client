#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GFCImageCache.generated.h"

class UTexture2D;

/** Downloads and caches thumbnails as transient textures. Requests for the same URL share one download. */
UCLASS()
class GFCCLIENT_API UGFCImageCache : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FOnImage = TFunction<void(UTexture2D* Texture)>;

	/** Calls back with the texture, or nullptr if it couldn't be loaded. May call back immediately. */
	void Request(const FString& Url, FOnImage Done);

	static constexpr int64 MaxImageBytes = 8 * 1024 * 1024;
	static constexpr int32 MaxCachedImages = 256;

private:
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> Cache;

	TMap<FString, TArray<FOnImage>> Pending;
	TSet<FString> Failed;
};
