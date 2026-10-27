#include "GFCCacheStore.h"

#include "GFCApi.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

FString FGFCCacheStats::ToString() const
{
	return FString::Printf(TEXT("disk %.1f MB in %d files | memory %.1f MB in %d items (%d visible) | live: %s"),
		DiskBytes / 1048576.0, DiskFiles, MemoryBytes / 1048576.0, MemoryItems, VisibleItems,
		HeavyOwner.IsEmpty() ? TEXT("none") : *HeavyOwner);
}

FGFCCacheStore::FGFCCacheStore(const FString& InRoot, const FGFCCacheLimits& InLimits)
	: Root(FPaths::ConvertRelativePathToFull(InRoot))
	, Limits(InLimits)
{
	FPaths::NormalizeDirectoryName(Root);
	bRootValid = FPaths::GetCleanFilename(Root) == TEXT("GFCCache");
	if (bRootValid)
	{
		IFileManager::Get().MakeDirectory(*Root, true);
	}
	else
	{
		UE_LOG(LogGFCApi, Error, TEXT("Cache root must be a folder named GFCCache; disk caching is disabled."));
	}
	FreeSpaceProvider = [this]()
	{
		uint64 Total = 0, Free = 0;
		return FPlatformMisc::GetDiskTotalAndFreeSpace(Root, Total, Free) ? Free : MAX_uint64;
	};
}

bool FGFCCacheStore::DeleteFileInRoot(const FString& Path) const
{
	const FString Full = FPaths::ConvertRelativePathToFull(Path);
	if (!bRootValid || FPaths::GetPath(Full) != Root)
	{
		return false; // never touch anything outside the cache folder
	}
	return IFileManager::Get().Delete(*Full, false, true, true);
}

int32 FGFCCacheStore::DeleteOrphans()
{
	if (!bRootValid)
	{
		return 0;
	}
	TArray<FString> Found;
	IFileManager::Get().FindFiles(Found, *(Root / TEXT("*")), true, false);
	int32 Deleted = 0;
	for (const FString& Name : Found)
	{
		const FString Path = Root / Name;
		if (!Files.ContainsByPredicate([&](const FFile& F) { return F.Path == Path; }) && DeleteFileInRoot(Path))
		{
			++Deleted;
		}
	}
	return Deleted;
}

FGFCCacheStore::EAdmit FGFCCacheStore::CanAdmit(int64 Bytes) const
{
	if (!bRootValid || Bytes > Limits.MaxDiskBytes)
	{
		return EAdmit::TooBig;
	}
	const uint64 Free = FreeSpaceProvider ? FreeSpaceProvider() : MAX_uint64;
	if (Free != MAX_uint64 && static_cast<int64>(Free) - Bytes < Limits.MinFreeDiskBytes)
	{
		return EAdmit::LowDiskSpace;
	}
	return EAdmit::Ok;
}

FString FGFCCacheStore::MakeFilePath(const FString& Owner, const FString& Ext) const
{
	FString CleanExt;
	for (const TCHAR C : Ext.ToLower())
	{
		if (((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9')) && CleanExt.Len() < 8)
		{
			CleanExt.AppendChar(C);
		}
	}
	FString CleanOwner;
	for (const TCHAR C : Owner)
	{
		if (FChar::IsAlnum(C) && CleanOwner.Len() < 40)
		{
			CleanOwner.AppendChar(C);
		}
	}
	return Root / FString::Printf(TEXT("%s-%s.%s"), *CleanOwner, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12),
		CleanExt.IsEmpty() ? TEXT("bin") : *CleanExt);
}

bool FGFCCacheStore::AddFile(const FString& Owner, const FString& Path, int64 Bytes)
{
	if (!bRootValid || Bytes > Limits.MaxDiskBytes)
	{
		DeleteFileInRoot(Path);
		return false;
	}
	int64 Used = 0;
	for (const FFile& F : Files)
	{
		Used += F.Bytes;
	}
	// Make room by deleting other owners' files, oldest first.
	Files.Sort([](const FFile& A, const FFile& B) { return A.Seq < B.Seq; });
	for (int32 i = 0; i < Files.Num() && Used + Bytes > Limits.MaxDiskBytes;)
	{
		if (Files[i].Owner != Owner)
		{
			Used -= Files[i].Bytes;
			DeleteFileInRoot(Files[i].Path);
			Files.RemoveAt(i);
		}
		else
		{
			++i;
		}
	}
	if (Used + Bytes > Limits.MaxDiskBytes)
	{
		DeleteFileInRoot(Path);
		return false;
	}
	Files.Add({Owner, FPaths::ConvertRelativePathToFull(Path), Bytes, NextSeq++});
	return true;
}

TArray<FString> FGFCCacheStore::ReleaseOwner(const FString& Owner)
{
	TArray<FString> Evicted;
	if (Owner.IsEmpty())
	{
		return Evicted;
	}
	for (int32 i = Files.Num() - 1; i >= 0; --i)
	{
		if (Files[i].Owner == Owner)
		{
			DeleteFileInRoot(Files[i].Path);
			Files.RemoveAt(i);
		}
	}
	for (auto It = Memory.CreateIterator(); It; ++It)
	{
		if (It.Value().Owner == Owner)
		{
			Evicted.Add(It.Key());
			It.RemoveCurrent();
		}
	}
	if (HeavyOwner == Owner)
	{
		HeavyOwner.Reset();
	}
	return Evicted;
}

TArray<FString> FGFCCacheStore::AddMemory(const FString& Key, int64 Bytes, const FString& Owner)
{
	Memory.Add(Key, {Owner, Bytes, NextSeq++});
	return EnforceMemoryCaps();
}

void FGFCCacheStore::Touch(const FString& Key)
{
	if (FMem* M = Memory.Find(Key))
	{
		M->Seq = NextSeq++;
	}
}

void FGFCCacheStore::RemoveMemory(const FString& Key)
{
	Memory.Remove(Key);
}

TArray<FString> FGFCCacheStore::EnforceMemoryCaps()
{
	TArray<FString> Evicted;
	auto TotalBytes = [this]()
	{
		int64 B = 0;
		for (const auto& Pair : Memory) B += Pair.Value.Bytes;
		return B;
	};
	while (Memory.Num() > Limits.MaxMemoryItems || TotalBytes() > Limits.MaxMemoryBytes)
	{
		// Oldest non-visible first; visible ones only if nothing else is left.
		const FString* Victim = nullptr;
		uint64 Best = MAX_uint64;
		for (int32 Pass = 0; Pass < 2 && !Victim; ++Pass)
		{
			for (const auto& Pair : Memory)
			{
				const bool bVisible = Visible.Contains(Pair.Key);
				if ((Pass == 0 && bVisible) || Pair.Value.Seq >= Best)
				{
					continue;
				}
				Best = Pair.Value.Seq;
				Victim = &Pair.Key;
			}
		}
		if (!Victim)
		{
			break;
		}
		Evicted.Add(*Victim);
		Memory.Remove(FString(*Victim));
	}
	return Evicted;
}

TArray<FString> FGFCCacheStore::TrimToVisible()
{
	TArray<FString> Evicted;
	for (auto It = Memory.CreateIterator(); It; ++It)
	{
		if (!Visible.Contains(It.Key()))
		{
			Evicted.Add(It.Key());
			It.RemoveCurrent();
		}
	}
	return Evicted;
}

FString FGFCCacheStore::BeginHeavy(const FString& Owner)
{
	FString Previous = HeavyOwner != Owner ? HeavyOwner : FString();
	HeavyOwner = Owner;
	return Previous;
}

TArray<FString> FGFCCacheStore::WipeAll()
{
	TArray<FString> Evicted;
	Memory.GetKeys(Evicted);
	Memory.Reset();
	Visible.Reset();
	Files.Reset();
	HeavyOwner.Reset();
	DeleteOrphans(); // nothing is tracked now, so this deletes every file in the folder
	return Evicted;
}

FGFCCacheStats FGFCCacheStore::GetStats() const
{
	FGFCCacheStats S;
	for (const FFile& F : Files)
	{
		S.DiskBytes += F.Bytes;
	}
	S.DiskFiles = Files.Num();
	for (const auto& Pair : Memory)
	{
		S.MemoryBytes += Pair.Value.Bytes;
		S.VisibleItems += Visible.Contains(Pair.Key) ? 1 : 0;
	}
	S.MemoryItems = Memory.Num();
	S.HeavyOwner = HeavyOwner;
	return S;
}
