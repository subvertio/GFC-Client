#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "GFCCacheStore.h"
#include "Interfaces/IHttpRequest.h"
#include "GFCCacheSubsystem.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EGFCFetchResult : uint8
{
	Ok,
	Cancelled,
	TooBig,
	LowDiskSpace,
	NetworkError,
	CacheFull,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FGFCOnOwnerReleased, const FString& /*Owner*/);

/**
 * The single owner of everything the app downloads (docs/cache-policy.md).
 * No other code fetches media or writes cache files.
 *
 * - Thumbnails: decoded textures in a bounded in-memory LRU, never on disk.
 * - Full media and 3D models: session-only temp files in the app's private
 *   cache folder, owned by a card. Only one heavy item is live at a time.
 * - Releases: card scrolled away (ReleaseOwner), app backgrounded or low memory
 *   (drops everything but visible thumbnails), app start (deletes orphans),
 *   logout (wipes everything; the token is cleared by UGFCApiSubsystem).
 *
 * Console: gfc.cache (usage), gfc.cache.overlay 0|1 (on-screen usage), gfc.cache.wipe.
 */
UCLASS()
class GFCAPI_API UGFCCacheSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FOnTexture = TFunction<void(UTexture2D* Texture)>;
	using FOnFile = TFunction<void(EGFCFetchResult Result, const FString& Path, int64 Bytes)>;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** A thumbnail texture (memory only). Calls back with nullptr on failure. May call back immediately. */
	void RequestThumbnail(const FString& Url, FOnTexture Done);

	/** Thumbnails of the cards on screen: kept on background/low memory and evicted last. */
	void SetVisibleThumbnails(const TSet<FString>& Urls);

	/**
	 * Downloads full media or a model to a temp file owned by Owner (e.g. "seed:<id>").
	 * bHeavy: this becomes the one live heavy item; the previous one is released first.
	 * Fails without filling the device if the file is over MaxBytes or space is low.
	 */
	void FetchFile(const FString& Url, const FString& Owner, int64 MaxBytes, bool bHeavy, FOnFile Done);

	/** Size of a download from a HEAD request (-1 if unknown), to ask before big downloads on mobile data. */
	void QuerySize(const FString& Url, TFunction<void(int64 Bytes)> Done);

	/** Cancels the owner's downloads, deletes its files and memory entries, and tells listeners to free GPU objects. */
	void ReleaseOwner(const FString& Owner);

	/** Logout: everything goes. */
	void WipeAll();

	/** Background or low memory: drop everything except visible thumbnails. */
	void TrimForBackground();

	/** Decoded thumbnail textures loaded via RequestThumbnail and still cached. */
	UTexture2D* FindThumbnail(const FString& Url) const;

	/** Builds a transient texture from encoded image bytes, downscaled to MaxSize (power of two not required). */
	static UTexture2D* DecodeTexture(const TArray<uint8>& Bytes, int32 MaxSize, int64* OutDecodedBytes = nullptr);

	/** True on mobile data (ask before big downloads). */
	static bool IsOnCellular();

	FGFCCacheStats GetStats() const;
	FGFCCacheStore& GetStore() { return *Store; }

	/** Fired after an owner's resources were released; viewers destroy their actors and textures. */
	FGFCOnOwnerReleased OnOwnerReleased;

	/** The app's private, OS-clearable cache folder ("…/GFCCache"). */
	static FString GetCacheRoot();

private:
	void DropTextures(const TArray<FString>& Keys);
	void HandleSessionChanged();
	void HandleBackground();
	bool TickOverlay(float DeltaTime);

	TUniquePtr<FGFCCacheStore> Store;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> Thumbnails;

	TMap<FString, TArray<FOnTexture>> PendingThumbnails;
	TSet<FString> FailedThumbnails;
	TMultiMap<FString, TSharedRef<IHttpRequest, ESPMode::ThreadSafe>> InFlight;

	FDelegateHandle BackgroundHandle, MemoryTrimHandle, LowMemoryHandle;
	FTSTicker::FDelegateHandle OverlayTicker;
	TArray<IConsoleObject*> ConsoleCommands;
	bool bWasLoggedIn = false;
};
