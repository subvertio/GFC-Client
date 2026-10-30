#include "GFCUpload.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace
{
	void Append(TArray<uint8>& Out, const FString& S)
	{
		const FTCHARToUTF8 U(*S);
		Out.Append(reinterpret_cast<const uint8*>(U.Get()), U.Length());
	}

	/** Form field values can't contain the boundary or raw CR/LF in names; quotes in file names are dropped. */
	FString SafeHeaderValue(const FString& V)
	{
		FString Out = V.Replace(TEXT("\""), TEXT("")).Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(""));
		return Out.IsEmpty() ? FString(TEXT("upload.bin")) : Out;
	}
}

bool FGFCMultipart::Build(const FGFCUploadRequest& R, FGFCMultipart& Out)
{
	Out = FGFCMultipart();
	Out.FileSize = IFileManager::Get().FileSize(*R.FilePath);
	if (Out.FileSize < 0)
	{
		return false;
	}
	Out.FilePath = R.FilePath;
	Out.Boundary = TEXT("----GFCBoundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);

	auto Field = [&Out](const TCHAR* Name, const FString& Value)
	{
		Append(Out.Prefix, FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n"), *Out.Boundary, Name));
		Append(Out.Prefix, Value);
		Append(Out.Prefix, TEXT("\r\n"));
	};
	Field(TEXT("title"), R.Title);
	Field(TEXT("description"), R.Description);
	Field(TEXT("tags"), R.Tags);
	if (R.bExclusive)
	{
		Field(TEXT("exclusive"), TEXT("true"));
	}
	if (R.ThumbnailJpeg.Num() > 0)
	{
		Append(Out.Prefix, FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"thumbnail\"; filename=\"thumb.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n"), *Out.Boundary));
		Out.Prefix.Append(R.ThumbnailJpeg);
		Append(Out.Prefix, TEXT("\r\n"));
	}
	const FString Name = SafeHeaderValue(R.FileName.IsEmpty() ? FPaths::GetCleanFilename(R.FilePath) : R.FileName);
	Append(Out.Prefix, FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"),
		*Out.Boundary, *Name, *SafeHeaderValue(R.MimeType)));
	Append(Out.Suffix, FString::Printf(TEXT("\r\n--%s--\r\n"), *Out.Boundary));
	return true;
}

TArray<uint8> FGFCMultipart::ToBytes() const
{
	TArray<uint8> Bytes = Prefix;
	TArray<uint8> File;
	if (FileSize > 0)
	{
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath));
		if (Reader)
		{
			File.SetNumUninitialized(int32(FileSize));
			Reader->Serialize(File.GetData(), FileSize);
		}
	}
	Bytes.Append(File);
	Bytes.Append(Suffix);
	return Bytes;
}

FGFCMultipartArchive::FGFCMultipartArchive(const FGFCMultipart& InBody)
	: Body(InBody)
{
	SetIsLoading(true);
	File.Reset(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Body.FilePath));
	if (!File && Body.FileSize > 0)
	{
		SetError();
	}
}

FGFCMultipartArchive::~FGFCMultipartArchive() = default;

void FGFCMultipartArchive::Seek(int64 InPos)
{
	Pos = FMath::Clamp<int64>(InPos, 0, Body.TotalSize());
}

void FGFCMultipartArchive::Serialize(void* Data, int64 Num)
{
	uint8* Dest = static_cast<uint8*>(Data);
	while (Num > 0 && !IsError())
	{
		const int64 PrefixEnd = Body.Prefix.Num();
		const int64 FileEnd = PrefixEnd + Body.FileSize;
		const int64 End = FileEnd + Body.Suffix.Num();
		if (Pos >= End)
		{
			SetError();
			return;
		}
		int64 Chunk = 0;
		if (Pos < PrefixEnd)
		{
			Chunk = FMath::Min(Num, PrefixEnd - Pos);
			FMemory::Memcpy(Dest, Body.Prefix.GetData() + Pos, Chunk);
		}
		else if (Pos < FileEnd)
		{
			Chunk = FMath::Min(Num, FileEnd - Pos);
			if (!File || !File->Seek(Pos - PrefixEnd) || !File->Read(Dest, Chunk))
			{
				SetError();
				return;
			}
		}
		else
		{
			Chunk = FMath::Min(Num, End - Pos);
			FMemory::Memcpy(Dest, Body.Suffix.GetData() + (Pos - FileEnd), Chunk);
		}
		Pos += Chunk;
		Dest += Chunk;
		Num -= Chunk;
	}
}

void FGFCUploadJob::Cancel()
{
	bCancelled = true;
	if (Request.IsValid())
	{
		Request->CancelRequest();
	}
}

void FGFCFeedMixer::MixCreators(TArray<FGFCSeed>& Seeds, int32 MaxRun)
{
	MaxRun = FMath::Max(1, MaxRun);
	for (int32 i = 0; i < Seeds.Num(); ++i)
	{
		// Length of the run of this creator ending just before i.
		int32 Run = 0;
		for (int32 k = i - 1; k >= 0 && Seeds[k].Owner.Equals(Seeds[i].Owner, ESearchCase::IgnoreCase); --k)
		{
			++Run;
		}
		if (Run < MaxRun)
		{
			continue;
		}
		// Too many in a row: pull the next post by someone else forward.
		for (int32 j = i + 1; j < Seeds.Num(); ++j)
		{
			if (!Seeds[j].Owner.Equals(Seeds[i].Owner, ESearchCase::IgnoreCase))
			{
				FGFCSeed Moved = MoveTemp(Seeds[j]);
				Seeds.RemoveAt(j);
				Seeds.Insert(MoveTemp(Moved), i);
				break;
			}
		}
		// If nobody else is left, the tail stays as it is: better a run than hiding posts.
	}
}
