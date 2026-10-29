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
	/** While a file downloads: where it's going and how many bytes are on disk so far (readable now). */
	using FOnFileProgress = TFunction<void(const FString& Path, int64 BytesOnDisk)>;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** A thumbnail texture (memory only). Calls back with nullptr on failure. May call back immediately. */
	void RequestThumbnail(const FString& Url, FOnTexture Done);

	/** Thumbnails of the cards on screen: kept on background/low memory and evicted last. */
	void SetVisibleThumbnails(const TSet<FString>& Urls);

	/**
	 * Downloads full media or a model to a temp file owned by Owner (e.g. "seed:<id>"). The body is
	 * streamed to disk as it arrives (never held in memory), and Progress reports the bytes written so
	 * a player can start reading the file before the download ends.
	 * bHeavy: this becomes the one live heavy item; the previous one is released first.
	 * Fails without filling the device if the file is over MaxBytes or space is low.
	 */
	void FetchFile(const FString& Url, const FString& Owner, int64 MaxBytes, bool bHeavy, FOnFile Done,
		FOnFileProgress Progress = nullptr);

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

	/**
	 * The extension a download of UrlExtension is saved under. Unreal's Android media plugin refuses files
	 * by name (only mp4, aac, 3gpp, webm, m3u8), while Android's player itself reads the content, so on
	 * Android the other formats it plays (m4a, mp3, ogg, opus, flac, mov, m4v, mkv, 3gp) are saved as .mp4.
	 * WAV keeps its name: it plays in the engine, not the platform player.
	 */
	static FString CacheExtensionFor(const FString& UrlExtension, bool bAndroidMedia = PLATFORM_ANDROID != 0);

private:
	void StartFetch(const FString& Url, const FString& Owner, int64 Cap, FOnFile Done, FOnFileProgress Progress, bool bRetry);
	void DropTextures(const TArray<FString>& Keys);
	void HandleSessionChanged();
	void HandleBackground();
	/** Routine engine memory trim: drops off-screen thumbnails only, keeps the live item (a playing track or video). */
	void HandleMemoryTrim();
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
