#pragma once

#include "CoreMinimal.h"
#include "GFCApiTypes.h"
#include "Serialization/Archive.h"

class IFileHandle;

/** What to post with POST /seeds/upload. */
struct GFCAPI_API FGFCUploadRequest
{
	/** The media file on disk (a camera capture, a picked file, ...). */
	FString FilePath;
	/** The name the node sees; its extension decides the content type. */
	FString FileName;
	FString MimeType = TEXT("application/octet-stream");
	FString Title;
	FString Description;
	FString Tags;
	/** Optional JPEG thumbnail made on the device. */
	TArray<uint8> ThumbnailJpeg;
	/** "Exclusive to GFC Network" (videos and text posts only): sent as exclusive=true. */
	bool bExclusive = false;
};

/**
 * A multipart/form-data body streamed from disk: the form fields and thumbnail
 * are small and held in memory, the media file is read in chunks while sending,
 * so a 200 MB video never needs a second copy in RAM or on disk.
 */
struct GFCAPI_API FGFCMultipart
{
	FString Boundary;
	TArray<uint8> Prefix; // fields, thumbnail part, and the file part's header
	FString FilePath;
	int64 FileSize = 0;
	TArray<uint8> Suffix; // "\r\n--boundary--\r\n"

	int64 TotalSize() const { return Prefix.Num() + FileSize + Suffix.Num(); }
	FString ContentType() const { return TEXT("multipart/form-data; boundary=") + Boundary; }

	/** Returns false if the file can't be read. */
	static bool Build(const FGFCUploadRequest& Request, FGFCMultipart& Out);

	/** The whole body in memory (tests and small files only). */
	TArray<uint8> ToBytes() const;
};

/** FArchive that reads a FGFCMultipart as one continuous stream, for IHttpRequest::SetContentFromStream. */
class GFCAPI_API FGFCMultipartArchive : public FArchive
{
public:
	explicit FGFCMultipartArchive(const FGFCMultipart& InBody);
	virtual ~FGFCMultipartArchive() override;

	virtual void Serialize(void* Data, int64 Num) override;
	virtual int64 Tell() override { return Pos; }
	virtual int64 TotalSize() override { return Body.TotalSize(); }
	virtual void Seek(int64 InPos) override;
	virtual FString GetArchiveName() const override { return TEXT("GFCMultipart"); }

private:
	FGFCMultipart Body;
	TUniquePtr<IFileHandle> File;
	int64 Pos = 0;
};

/** Handle to an upload in progress. */
class GFCAPI_API FGFCUploadJob
{
public:
	void Cancel();
	bool IsCancelled() const { return bCancelled; }
	int32 GetAttempt() const { return Attempt; }

private:
	friend class UGFCApiClient;
	TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> Request;
	bool bCancelled = false;
	int32 Attempt = 0;
};

/**
 * Feed ordering that doesn't let one creator take over: no more than MaxRun
 * consecutive posts from the same creator. Otherwise keeps the node's order
 * (newest first): when a run would get too long, the next post by someone
 * else is pulled forward.
 */
struct GFCAPI_API FGFCFeedMixer
{
	static void MixCreators(TArray<FGFCSeed>& Seeds, int32 MaxRun = 2);
};
