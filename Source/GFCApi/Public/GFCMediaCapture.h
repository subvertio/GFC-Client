#pragma once

#include "CoreMinimal.h"

enum class EGFCCaptureSource : uint8
{
	Photo = 0,   // system camera, photo
	Video = 1,   // system camera, video
	Gallery = 2, // system photo picker (photos and videos)
	File = 3,    // system file browser, any type (3D models, audio)
};

/** Media the user captured or picked, ready to post. */
struct GFCAPI_API FGFCCapturedMedia
{
	/** A readable local file: the app's copy in its cache folder (Android) or the original (PC). */
	FString Path;
	FString MimeType;
	FString FileName;
	int64 Size = 0;
	/** JPEG thumbnail made on the device (video frame on Android), or empty. */
	FString ThumbnailPath;
	/** True when the capture was saved to the phone's gallery (it stays the user's, outside the app). */
	bool bSavedToGallery = false;
	/** True when Path is a temporary copy the app must delete after posting. */
	bool bIsAppCopy = false;
};

/**
 * Camera and gallery through the platform's own apps (GFCApi_UPL.xml on Android):
 *
 * - Photo / Video: ACTION_IMAGE_CAPTURE / ACTION_VIDEO_CAPTURE writing straight into
 *   MediaStore (Pictures/GFC, Movies/GFC), so the capture is in the user's gallery.
 *   No CAMERA permission is declared or needed; WRITE_EXTERNAL_STORAGE is requested
 *   only on Android 9 and older, and only when the user taps Photo or Video.
 * - Gallery: the system photo picker (Android 13+) or ACTION_GET_CONTENT; no storage
 *   permission needed.
 * - PC: Gallery opens a file dialog; Photo and Video aren't available.
 *
 * One request at a time. The callback runs on the game thread.
 */
struct GFCAPI_API FGFCMediaCapture
{
	using FOnCaptured = TFunction<void(bool bOk, const FGFCCapturedMedia& Media, const FString& Error)>;

	static bool IsSupported(EGFCCaptureSource Source);
	static void Start(EGFCCaptureSource Source, FOnCaptured Done);

	/** Called by the platform layer (any thread). Empty Path means cancelled or failed (see Error). */
	static void Complete(const FGFCCapturedMedia& Media, const FString& Error);

	/** Content type from a file extension, for the multipart part. */
	static FString MimeForFile(const FString& FileName);

	/** A JPEG thumbnail (longest side MaxSize) of an image file; empty if it isn't a decodable image. */
	static TArray<uint8> MakeImageThumbnail(const FString& ImagePath, int32 MaxSize = 512);
};
