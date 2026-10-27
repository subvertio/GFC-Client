#include "GFCMediaCapture.h"

#include "Async/Async.h"
#include "GFCApi.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace
{
	FCriticalSection GLock;
	FGFCMediaCapture::FOnCaptured GPending;
}

bool FGFCMediaCapture::IsSupported(EGFCCaptureSource Source)
{
#if PLATFORM_ANDROID
	return true;
#elif PLATFORM_WINDOWS
	return Source == EGFCCaptureSource::Gallery;
#else
	return false;
#endif
}

FString FGFCMediaCapture::MimeForFile(const FString& FileName)
{
	const FString Ext = FPaths::GetExtension(FileName).ToLower();
	static const TMap<FString, FString> Types = {
		{TEXT("jpg"), TEXT("image/jpeg")}, {TEXT("jpeg"), TEXT("image/jpeg")}, {TEXT("png"), TEXT("image/png")},
		{TEXT("gif"), TEXT("image/gif")}, {TEXT("webp"), TEXT("image/webp")},
		{TEXT("mp4"), TEXT("video/mp4")}, {TEXT("mov"), TEXT("video/quicktime")}, {TEXT("webm"), TEXT("video/webm")},
		{TEXT("mp3"), TEXT("audio/mpeg")}, {TEXT("wav"), TEXT("audio/wav")}, {TEXT("ogg"), TEXT("audio/ogg")}, {TEXT("m4a"), TEXT("audio/mp4")},
		{TEXT("glb"), TEXT("model/gltf-binary")}, {TEXT("gltf"), TEXT("model/gltf+json")},
	};
	const FString* T = Types.Find(Ext);
	return T ? *T : FString(TEXT("application/octet-stream"));
}

TArray<uint8> FGFCMediaCapture::MakeImageThumbnail(const FString& ImagePath, int32 MaxSize)
{
	TArray<uint8> Bytes;
	if (IFileManager::Get().FileSize(*ImagePath) > 40ll * 1024 * 1024 || !FFileHelper::LoadFileToArray(Bytes, *ImagePath))
	{
		return {};
	}
	IImageWrapperModule& IW = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const EImageFormat Format = IW.DetectImageFormat(Bytes.GetData(), Bytes.Num());
	TSharedPtr<IImageWrapper> In = Format != EImageFormat::Invalid ? IW.CreateImageWrapper(Format) : nullptr;
	TArray64<uint8> Raw;
	if (!In.IsValid() || !In->SetCompressed(Bytes.GetData(), Bytes.Num()) || In->GetWidth() <= 0 || In->GetWidth() > 16384
		|| In->GetHeight() <= 0 || In->GetHeight() > 16384 || !In->GetRaw(ERGBFormat::BGRA, 8, Raw))
	{
		return {};
	}
	const int32 W = int32(In->GetWidth()), H = int32(In->GetHeight());
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(W * H);
	FMemory::Memcpy(Pixels.GetData(), Raw.GetData(), FMath::Min<int64>(Raw.Num(), int64(W) * H * 4));
	const float Scale = FMath::Min(1.f, float(MaxSize) / FMath::Max(W, H));
	const int32 NW = FMath::Max(1, FMath::RoundToInt(W * Scale)), NH = FMath::Max(1, FMath::RoundToInt(H * Scale));
	TArray<FColor> Small;
	Small.SetNumUninitialized(NW * NH);
	FImageUtils::ImageResize(W, H, Pixels, NW, NH, Small, true, true);
	TSharedPtr<IImageWrapper> Out = IW.CreateImageWrapper(EImageFormat::JPEG);
	if (!Out.IsValid() || !Out->SetRaw(Small.GetData(), int64(Small.Num()) * 4, NW, NH, ERGBFormat::BGRA, 8))
	{
		return {};
	}
	const TArray64<uint8>& Jpeg = Out->GetCompressed(82);
	return TArray<uint8>(Jpeg.GetData(), int32(Jpeg.Num()));
}

void FGFCMediaCapture::Complete(const FGFCCapturedMedia& Media, const FString& Error)
{
	AsyncTask(ENamedThreads::GameThread, [Media, Error]()
	{
		FOnCaptured Done;
		{
			FScopeLock Lock(&GLock);
			Done = MoveTemp(GPending);
			GPending = nullptr;
		}
		if (Done)
		{
			Done(!Media.Path.IsEmpty(), Media, Error);
		}
	});
}

void FGFCMediaCapture::Start(EGFCCaptureSource Source, FOnCaptured Done)
{
	if (!IsSupported(Source))
	{
		Done(false, FGFCCapturedMedia(), TEXT("The camera is available in the phone app."));
		return;
	}
	{
		FScopeLock Lock(&GLock);
		if (GPending)
		{
			Done(false, FGFCCapturedMedia(), TEXT("Already choosing media."));
			return;
		}
		GPending = MoveTemp(Done);
	}

#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GFCCapture", "(I)V", false);
		if (Method)
		{
			FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, int32(Source));
			return;
		}
	}
	Complete(FGFCCapturedMedia(), TEXT("Media capture isn't available."));
#elif PLATFORM_WINDOWS
	TCHAR FileBuffer[4096] = {0};
	OPENFILENAMEW Ofn = {};
	Ofn.lStructSize = sizeof(Ofn);
	Ofn.hwndOwner = GetActiveWindow();
	Ofn.lpstrFilter = TEXT("Photos, videos, audio, 3D\0*.jpg;*.jpeg;*.png;*.gif;*.webp;*.mp4;*.mov;*.webm;*.mp3;*.wav;*.ogg;*.m4a;*.glb;*.gltf\0All files\0*.*\0");
	Ofn.lpstrFile = FileBuffer;
	Ofn.nMaxFile = UE_ARRAY_COUNT(FileBuffer);
	Ofn.lpstrTitle = TEXT("Choose something to post");
	Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	FGFCCapturedMedia Media;
	if (GetOpenFileNameW(&Ofn))
	{
		Media.Path = FileBuffer;
		Media.FileName = FPaths::GetCleanFilename(Media.Path);
		Media.MimeType = MimeForFile(Media.FileName);
		Media.Size = IFileManager::Get().FileSize(*Media.Path);
		Media.bIsAppCopy = false; // the user's own file: never deleted
	}
	Complete(Media, Media.Path.IsEmpty() ? FString(TEXT("cancelled")) : FString());
#else
	Complete(FGFCCapturedMedia(), TEXT("Not supported on this platform."));
#endif
}

#if PLATFORM_ANDROID
// GameActivity.gfcFinish (GFCApi_UPL.xml): the capture or pick finished.
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeGFCOnCaptured(JNIEnv* Env, jobject Thiz, jstring JPath, jstring JMime,
	jstring JName, jlong Size, jstring JThumb, jboolean bSavedToGallery, jstring JError)
{
	FGFCCapturedMedia Media;
	Media.Path = FJavaHelper::FStringFromParam(Env, JPath);
	Media.MimeType = FJavaHelper::FStringFromParam(Env, JMime);
	Media.FileName = FJavaHelper::FStringFromParam(Env, JName);
	Media.Size = int64(Size);
	Media.ThumbnailPath = FJavaHelper::FStringFromParam(Env, JThumb);
	Media.bSavedToGallery = bSavedToGallery == JNI_TRUE;
	Media.bIsAppCopy = !Media.Path.IsEmpty();
	FGFCMediaCapture::Complete(Media, FJavaHelper::FStringFromParam(Env, JError));
}
#endif
