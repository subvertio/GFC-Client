#pragma once

#include "CoreMinimal.h"

/** Caps for FGFCCacheStore. See docs/cache-policy.md. */
struct GFCAPI_API FGFCCacheLimits
{
	int64 MaxDiskBytes = 200ll * 1024 * 1024;
	int32 MaxMemoryItems = 60;
	int64 MaxMemoryBytes = 48ll * 1024 * 1024;
	int64 MinFreeDiskBytes = 300ll * 1024 * 1024;
};

struct GFCAPI_API FGFCCacheStats
{
	int64 DiskBytes = 0;
	int32 DiskFiles = 0;
	int64 MemoryBytes = 0;
	int32 MemoryItems = 0;
	int32 VisibleItems = 0;
	FString HeavyOwner;

	FString ToString() const;
};

/**
 * The bookkeeping half of the cache: what is on disk and in memory, for which
 * owner (a card), and the caps. No networking or textures, so it is unit-tested.
 *
 * Disk entries are session-only temp files in one dedicated folder. Memory
 * entries are keys whose payload (a texture) lives in UGFCCacheSubsystem; when
 * the store evicts a key the subsystem drops the payload.
 */
class GFCAPI_API FGFCCacheStore
{
public:
	enum class EAdmit : uint8 { Ok, TooBig, LowDiskSpace };

	/** Root must be a folder named "GFCCache" (a guard against wiping the wrong place). */
	FGFCCacheStore(const FString& InRoot, const FGFCCacheLimits& InLimits);

	const FString& GetRoot() const { return Root; }
	const FGFCCacheLimits& GetLimits() const { return Limits; }
	bool IsRootValid() const { return bRootValid; }

	/** Free bytes on the cache's volume. Tests can replace it. */
	TFunction<uint64()> FreeSpaceProvider;

	// ---- disk ----

	/** App start: the cache only lives for a session, so every file found is an orphan from a crash or kill. */
	int32 DeleteOrphans();

	/** Can a download of up to Bytes be stored? Checks the cap and the device's free space. */
	EAdmit CanAdmit(int64 Bytes) const;

	/** A fresh temp file path for an owner. Ext is reduced to [a-z0-9], max 8 chars. */
	FString MakeFilePath(const FString& Owner, const FString& Ext) const;

	/**
	 * Records a downloaded file. If the cap is exceeded, other owners' files are
	 * deleted oldest first; if that isn't enough, this file is deleted and false returned.
	 */
	bool AddFile(const FString& Owner, const FString& Path, int64 Bytes);

	/** Deletes an owner's files and memory entries. Returns the evicted memory keys. */
	TArray<FString> ReleaseOwner(const FString& Owner);

	// ---- memory ----

	/** Records a memory entry (LRU). Returns keys evicted to stay within caps; visible keys are evicted last. */
	TArray<FString> AddMemory(const FString& Key, int64 Bytes, const FString& Owner = FString());
	bool HasMemory(const FString& Key) const { return Memory.Contains(Key); }
	void Touch(const FString& Key);
	void RemoveMemory(const FString& Key);

	/** Keys currently on screen (thumbnails of visible cards). */
	void SetVisible(const TSet<FString>& Keys) { Visible = Keys; }

	/** Background / low memory: drops every memory entry that isn't visible. Returns the evicted keys. */
	TArray<FString> TrimToVisible();

	// ---- heavy item (one live 3D model or full media at a time) ----

	/** Makes Owner the live heavy item. Returns the previous one, which the caller must release. */
	FString BeginHeavy(const FString& Owner);
	const FString& GetHeavyOwner() const { return HeavyOwner; }

	/** Logout: deletes every file and forgets every memory entry. Returns the evicted memory keys. */
	TArray<FString> WipeAll();

	FGFCCacheStats GetStats() const;

private:
	struct FFile { FString Owner; FString Path; int64 Bytes = 0; uint64 Seq = 0; };
	struct FMem { FString Owner; int64 Bytes = 0; uint64 Seq = 0; };

	bool DeleteFileInRoot(const FString& Path) const;
	TArray<FString> EnforceMemoryCaps();

	FString Root;
	FGFCCacheLimits Limits;
	bool bRootValid = false;
	uint64 NextSeq = 1;
	TArray<FFile> Files;
	TMap<FString, FMem> Memory;
	TSet<FString> Visible;
	FString HeavyOwner;
};
