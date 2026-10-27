#include "GFCCacheSubsystem.h"

#include <atomic>

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "GFCApi.h"
#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCApiSubsystem.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#endif

namespace
{
	constexpr int64 MaxThumbnailFileBytes = 4 * 1024 * 1024;
	constexpr int32 MaxThumbnailSize = 512;
	bool GShowOverlay = false;

	/**
	 * The HTTP body, written straight to a file (on the HTTP thread) and flushed per chunk, so the
	 * file can be read while it grows. Written() is safe from any thread.
	 */
	class FGFCFileSink : public FArchive
	{
	public:
		explicit FGFCFileSink(const FString& Path)
		{
			SetIsSaving(true);
			Handle.Reset(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Path, false, true));
		}
		bool IsOpen() const { return Handle.IsValid(); }
		int64 Written() const { return BytesWritten.load(); }
		virtual bool Close() override { FScopeLock Lock(&Mutex); Handle.Reset(); return !IsError(); }
		virtual void Serialize(void* Data, int64 Length) override
		{
			FScopeLock Lock(&Mutex);
			if (!Handle.IsValid() || !Handle->Write(static_cast<const uint8*>(Data), Length) || !Handle->Flush())
			{
				SetError();
				return;
			}
			BytesWritten += Length;
		}
		virtual int64 TotalSize() override { return BytesWritten.load(); }
		virtual int64 Tell() override { return BytesWritten.load(); }
		virtual FString GetArchiveName() const override { return TEXT("GFCFileSink"); }
	private:
		TUniquePtr<IFileHandle> Handle;
		FCriticalSection Mutex;
		std::atomic<int64> BytesWritten{0};
	};

	FString ExtensionOf(const FString& Url)
	{
		FString Path = Url;
		int32 Q;
		if (Path.FindChar(TEXT('?'), Q)) Path.LeftInline(Q);
		return FPaths::GetExtension(Path, false);
	}
}

FString UGFCCacheSubsystem::GetCacheRoot()
{
#if PLATFORM_ANDROID
	// Context.getCacheDir(): private to the app, and the OS may clear it when space runs low.
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GFCGetCacheDir", "()Ljava/lang/String;", false);
		if (Method)
		{
			jstring JDir = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, Method);
			const FString Dir = FJavaHelper::FStringFromLocalRef(Env, JDir);
			if (!Dir.IsEmpty())
			{
				return Dir / TEXT("GFCCache");
			}
		}
	}
#endif
	return FPaths::ProjectSavedDir() / TEXT("GFCCache");
}

void UGFCCacheSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UGFCApiSubsystem* Api = Collection.InitializeDependency<UGFCApiSubsystem>();

	const UGFCApiSettings* S = GetDefault<UGFCApiSettings>();
	FGFCCacheLimits Limits;
	Limits.MaxDiskBytes = int64(S->MaxDiskCacheMB) * 1024 * 1024;
	Limits.MaxMemoryItems = S->MaxThumbnails;
	Limits.MaxMemoryBytes = int64(S->MaxThumbnailMB) * 1024 * 1024;
	Limits.MinFreeDiskBytes = int64(S->MinFreeDiskMB) * 1024 * 1024;
	Store = MakeUnique<FGFCCacheStore>(GetCacheRoot(), Limits);

	const int32 Orphans = Store->DeleteOrphans();
	if (Orphans > 0)
	{
		UE_LOG(LogGFCApi, Log, TEXT("Cache: deleted %d orphaned files from a previous session."), Orphans);
	}

	if (Api && Api->GetClient())
	{
		bWasLoggedIn = Api->GetClient()->IsLoggedIn();
		Api->GetClient()->OnSessionChanged.AddUObject(this, &UGFCCacheSubsystem::HandleSessionChanged);
	}
	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UGFCCacheSubsystem::HandleBackground);
	// The engine asks for a memory trim routinely (about once a minute): that only drops what's off screen.
	// Leaving the app or a real low-memory warning releases everything, including what's playing.
	MemoryTrimHandle = FCoreDelegates::GetMemoryTrimDelegate().AddUObject(this, &UGFCCacheSubsystem::HandleMemoryTrim);
	LowMemoryHandle = FCoreDelegates::ApplicationShouldUnloadResourcesDelegate.AddUObject(this, &UGFCCacheSubsystem::HandleBackground);

	TWeakObjectPtr<UGFCCacheSubsystem> WeakThis(this);
	IConsoleManager& CM = IConsoleManager::Get();
	if (!CM.FindConsoleObject(TEXT("gfc.cache")))
	{
		ConsoleCommands.Add(CM.RegisterConsoleCommand(TEXT("gfc.cache"), TEXT("Shows GFC cache usage."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([WeakThis](const TArray<FString>&, UWorld*, FOutputDevice& Ar)
			{
				if (WeakThis.IsValid())
				{
					Ar.Logf(TEXT("GFC cache: %s (root %s)"), *WeakThis->GetStats().ToString(), *WeakThis->Store->GetRoot());
				}
			})));
		ConsoleCommands.Add(CM.RegisterConsoleCommand(TEXT("gfc.cache.overlay"), TEXT("gfc.cache.overlay 1 shows cache usage on screen, 0 hides it."),
			FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				GShowOverlay = Args.Num() == 0 ? !GShowOverlay : Args[0] != TEXT("0");
			})));
		ConsoleCommands.Add(CM.RegisterConsoleCommand(TEXT("gfc.cache.wipe"), TEXT("Deletes everything in the GFC cache."),
			FConsoleCommandDelegate::CreateLambda([WeakThis]() { if (WeakThis.IsValid()) WeakThis->WipeAll(); })));
	}
	OverlayTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGFCCacheSubsystem::TickOverlay), 0.5f);
}

void UGFCCacheSubsystem::Deinitialize()
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
	FCoreDelegates::GetMemoryTrimDelegate().Remove(MemoryTrimHandle);
	FCoreDelegates::ApplicationShouldUnloadResourcesDelegate.Remove(LowMemoryHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(OverlayTicker);
	for (IConsoleObject* C : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(C);
	}
	ConsoleCommands.Reset();
	for (auto& Pair : InFlight)
	{
		Pair.Value->OnProcessRequestComplete().Unbind();
		Pair.Value->CancelRequest();
	}
	InFlight.Reset();
	if (Store)
	{
		Store->WipeAll(); // session-only: nothing survives the app
	}
	Super::Deinitialize();
}

bool UGFCCacheSubsystem::TickOverlay(float)
{
	if (GShowOverlay && GEngine && Store)
	{
		GEngine->AddOnScreenDebugMessage(0x6FC0, 0.6f, FColor::Cyan, TEXT("GFC cache: ") + GetStats().ToString());
	}
	return true;
}

FGFCCacheStats UGFCCacheSubsystem::GetStats() const
{
	return Store ? Store->GetStats() : FGFCCacheStats();
}

UTexture2D* UGFCCacheSubsystem::FindThumbnail(const FString& Url) const
{
	const TObjectPtr<UTexture2D>* T = Thumbnails.Find(Url);
	return T ? T->Get() : nullptr;
}

void UGFCCacheSubsystem::DropTextures(const TArray<FString>& Keys)
{
	for (const FString& K : Keys)
	{
		Thumbnails.Remove(K); // the GC frees the texture once no widget shows it
	}
}

UTexture2D* UGFCCacheSubsystem::DecodeTexture(const TArray<uint8>& Bytes, int32 MaxSize, int64* OutDecodedBytes)
{
	IImageWrapperModule& IW = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const EImageFormat Format = IW.DetectImageFormat(Bytes.GetData(), Bytes.Num());
	if (Format == EImageFormat::Invalid)
	{
		return nullptr;
	}
	TSharedPtr<IImageWrapper> Wrapper = IW.CreateImageWrapper(Format);
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
	{
		return nullptr;
	}
	const int64 W = Wrapper->GetWidth(), H = Wrapper->GetHeight();
	if (W <= 0 || H <= 0 || W > 16384 || H > 16384)
	{
		return nullptr; // refuse decompression bombs
	}
	TArray64<uint8> Raw;
	if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
	{
		return nullptr;
	}
	int32 OutW = int32(W), OutH = int32(H);
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(OutW * OutH);
	FMemory::Memcpy(Pixels.GetData(), Raw.GetData(), FMath::Min<int64>(Raw.Num(), int64(Pixels.Num()) * 4));
	if (FMath::Max(OutW, OutH) > MaxSize)
	{
		const float Scale = float(MaxSize) / FMath::Max(OutW, OutH);
		const int32 NW = FMath::Max(1, FMath::RoundToInt(OutW * Scale)), NH = FMath::Max(1, FMath::RoundToInt(OutH * Scale));
		TArray<FColor> Resized;
		Resized.SetNumUninitialized(NW * NH);
		FImageUtils::ImageResize(OutW, OutH, Pixels, NW, NH, Resized, true, false);
		Pixels = MoveTemp(Resized);
		OutW = NW;
		OutH = NH;
	}
	UTexture2D* Tex = UTexture2D::CreateTransient(OutW, OutH, PF_B8G8R8A8);
	if (!Tex)
	{
		return nullptr;
	}
	void* Data = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), int64(OutW) * OutH * 4);
	Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
	Tex->SRGB = true;
	Tex->UpdateResource();
	if (OutDecodedBytes)
	{
		*OutDecodedBytes = int64(OutW) * OutH * 4;
	}
	return Tex;
}

void UGFCCacheSubsystem::RequestThumbnail(const FString& Url, FOnTexture Done)
{
	if (Url.IsEmpty() || FailedThumbnails.Contains(Url))
	{
		Done(nullptr);
		return;
	}
	if (UTexture2D* Hit = FindThumbnail(Url))
	{
		Store->Touch(Url);
		Done(Hit);
		return;
	}
	if (TArray<FOnTexture>* Waiting = PendingThumbnails.Find(Url))
	{
		Waiting->Add(MoveTemp(Done));
		return;
	}
	PendingThumbnails.Add(Url).Add(MoveTemp(Done));

	UGFCApiClient* Client = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>()->GetClient();
	TWeakObjectPtr<UGFCCacheSubsystem> WeakThis(this);
	Client->DownloadBytes(Url, MaxThumbnailFileBytes, [WeakThis, Url](bool bOk, const TArray<uint8>& Bytes)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		int64 Decoded = 0;
		UTexture2D* Tex = bOk ? DecodeTexture(Bytes, MaxThumbnailSize, &Decoded) : nullptr;
		if (Tex)
		{
			WeakThis->Thumbnails.Add(Url, Tex);
			WeakThis->DropTextures(WeakThis->Store->AddMemory(Url, Decoded));
		}
		else
		{
			WeakThis->FailedThumbnails.Add(Url);
		}
		TArray<FOnTexture> Callbacks;
		WeakThis->PendingThumbnails.RemoveAndCopyValue(Url, Callbacks);
		for (FOnTexture& Cb : Callbacks)
		{
			Cb(WeakThis->FindThumbnail(Url));
		}
	});
}

void UGFCCacheSubsystem::SetVisibleThumbnails(const TSet<FString>& Urls)
{
	Store->SetVisible(Urls);
}

void UGFCCacheSubsystem::FetchFile(const FString& Url, const FString& Owner, int64 MaxBytes, bool bHeavy, FOnFile Done,
	FOnFileProgress Progress)
{
	const int64 Cap = FMath::Min(MaxBytes, Store->GetLimits().MaxDiskBytes);
	switch (Store->CanAdmit(Cap))
	{
	case FGFCCacheStore::EAdmit::LowDiskSpace: Done(EGFCFetchResult::LowDiskSpace, FString(), 0); return;
	case FGFCCacheStore::EAdmit::TooBig: Done(EGFCFetchResult::CacheFull, FString(), 0); return;
	default: break;
	}
	if (bHeavy)
	{
		const FString Previous = Store->BeginHeavy(Owner);
		if (!Previous.IsEmpty())
		{
			ReleaseOwner(Previous);
			Store->BeginHeavy(Owner);
		}
	}
	StartFetch(Url, Owner, Cap, MoveTemp(Done), MoveTemp(Progress), true);
}

FString UGFCCacheSubsystem::CacheExtensionFor(const FString& UrlExtension, bool bAndroidMedia)
{
	static const TSet<FString> PlatformPlayerOnly = {TEXT("m4a"), TEXT("mp3"), TEXT("ogg"), TEXT("oga"), TEXT("opus"),
		TEXT("flac"), TEXT("mov"), TEXT("m4v"), TEXT("mkv"), TEXT("3gp")};
	const FString Ext = UrlExtension.ToLower();
	return bAndroidMedia && PlatformPlayerOnly.Contains(Ext) ? FString(TEXT("mp4")) : Ext;
}

void UGFCCacheSubsystem::StartFetch(const FString& Url, const FString& Owner, int64 Cap, FOnFile Done, FOnFileProgress Progress,
	bool bRetry)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(GetDefault<UGFCApiSettings>()->ModelLoadTimeoutSeconds);

	// The body goes straight to its file as it arrives: nothing big is ever held in memory.
	const FString Path = Store->MakeFilePath(Owner, CacheExtensionFor(ExtensionOf(Url)));
	TSharedRef<FGFCFileSink> Sink = MakeShared<FGFCFileSink>(Path);
	if (!Sink->IsOpen())
	{
		Done(EGFCFetchResult::CacheFull, FString(), 0);
		return;
	}
	Request->SetResponseBodyReceiveStream(Sink);

	// Abort as soon as the body grows past the cap, instead of downloading it all; report progress.
	TWeakPtr<IHttpRequest, ESPMode::ThreadSafe> WeakRequest = Request;
	TSharedRef<bool> bTooBig = MakeShared<bool>(false);
	Request->OnRequestProgress64().BindLambda([Cap, bTooBig, WeakRequest, Sink, Path, Progress](FHttpRequestPtr, uint64, uint64 Received)
	{
		if (int64(Received) > Cap && !*bTooBig)
		{
			*bTooBig = true;
			if (TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> R = WeakRequest.Pin())
			{
				R->CancelRequest();
			}
			return;
		}
		if (Progress && !*bTooBig)
		{
			Progress(Path, Sink->Written());
		}
	});

	TWeakObjectPtr<UGFCCacheSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, Url, Owner, Cap, Path, Sink, bTooBig, Done = MoveTemp(Done), Progress, bRetry](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected) mutable
		{
			Sink->Close();
			const int64 Bytes = Sink->Written();
			auto Fail = [&](EGFCFetchResult Result)
			{
				IFileManager::Get().Delete(*Path, false, true, true); // no half-written files left behind
				Done(Result, FString(), 0);
			};
			if (!WeakThis.IsValid())
			{
				IFileManager::Get().Delete(*Path, false, true, true);
				return;
			}
			WeakThis->InFlight.RemoveSingle(Owner, Req.ToSharedRef());
			if (*bTooBig || Bytes > Cap)
			{
				Fail(EGFCFetchResult::TooBig);
				return;
			}
			if (Req->GetStatus() == EHttpRequestStatus::Failed && !bConnected && !Resp.IsValid())
			{
				// A pooled connection that died while the phone slept or changed networks fails once
				// (a 30 s connect timeout); a fresh attempt connects straight away.
				if (bRetry && Bytes == 0 && Req->GetFailureReason() == EHttpFailureReason::ConnectionError)
				{
					IFileManager::Get().Delete(*Path, false, true, true);
					UE_LOG(LogGFCApi, Log, TEXT("Download couldn't connect; trying once more"));
					WeakThis->StartFetch(Url, Owner, Cap, MoveTemp(Done), MoveTemp(Progress), false);
					return;
				}
				Fail(Req->GetFailureReason() == EHttpFailureReason::Cancelled ? EGFCFetchResult::Cancelled : EGFCFetchResult::NetworkError);
				return;
			}
			if (!bConnected || !Resp.IsValid() || Resp->GetResponseCode() != 200 || Sink->IsError())
			{
				Fail(EGFCFetchResult::NetworkError);
				return;
			}
			FGFCCacheStore& S = *WeakThis->Store;
			if (S.CanAdmit(Bytes) == FGFCCacheStore::EAdmit::LowDiskSpace)
			{
				Fail(EGFCFetchResult::LowDiskSpace);
				return;
			}
			if (!S.AddFile(Owner, Path, Bytes))
			{
				Fail(EGFCFetchResult::CacheFull);
				return;
			}
			Done(EGFCFetchResult::Ok, Path, Bytes);
		});
	InFlight.Add(Owner, Request);
	Request->ProcessRequest();
}

void UGFCCacheSubsystem::QuerySize(const FString& Url, TFunction<void(int64)> Done)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("HEAD"));
	Request->SetTimeout(15.f);
	Request->OnProcessRequestComplete().BindLambda([Done = MoveTemp(Done)](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		int64 Bytes = -1;
		if (bConnected && Resp.IsValid() && Resp->GetResponseCode() == 200)
		{
			LexFromString(Bytes, *Resp->GetHeader(TEXT("Content-Length")));
			if (Bytes <= 0) Bytes = -1;
		}
		Done(Bytes);
	});
	Request->ProcessRequest();
}

void UGFCCacheSubsystem::ReleaseOwner(const FString& Owner)
{
	if (Owner.IsEmpty() || !Store)
	{
		return;
	}
	TArray<TSharedRef<IHttpRequest, ESPMode::ThreadSafe>> Requests;
	InFlight.MultiFind(Owner, Requests);
	InFlight.Remove(Owner);
	for (const auto& R : Requests)
	{
		R->CancelRequest(); // completes with Cancelled; the callback sees it
	}
	DropTextures(Store->ReleaseOwner(Owner));
	OnOwnerReleased.Broadcast(Owner);
}

void UGFCCacheSubsystem::TrimForBackground()
{
	if (!Store)
	{
		return;
	}
	TArray<FString> Owners;
	InFlight.GetKeys(Owners);
	if (!Store->GetHeavyOwner().IsEmpty())
	{
		Owners.AddUnique(Store->GetHeavyOwner());
	}
	for (const FString& O : Owners)
	{
		ReleaseOwner(O);
	}
	DropTextures(Store->TrimToVisible());
	FailedThumbnails.Reset();
	UE_LOG(LogGFCApi, Log, TEXT("Cache trimmed: %s"), *GetStats().ToString());
}

void UGFCCacheSubsystem::WipeAll()
{
	if (!Store)
	{
		return;
	}
	TArray<FString> Owners;
	InFlight.GetKeys(Owners);
	if (!Store->GetHeavyOwner().IsEmpty())
	{
		Owners.AddUnique(Store->GetHeavyOwner());
	}
	for (const FString& O : Owners)
	{
		ReleaseOwner(O);
	}
	DropTextures(Store->WipeAll());
	Thumbnails.Reset();
	PendingThumbnails.Reset();
	FailedThumbnails.Reset();
}

void UGFCCacheSubsystem::HandleMemoryTrim()
{
	if (!Store) return;
	DropTextures(Store->TrimToVisible());
	FailedThumbnails.Reset();
	UE_LOG(LogGFCApi, Verbose, TEXT("Memory trim (kept what's live): %s"), *GetStats().ToString());
}

void UGFCCacheSubsystem::HandleBackground()
{
	TrimForBackground();
}

void UGFCCacheSubsystem::HandleSessionChanged()
{
	const UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>();
	const bool bLoggedIn = Api && Api->GetClient() && Api->GetClient()->IsLoggedIn();
	if (bWasLoggedIn && !bLoggedIn)
	{
		WipeAll();
		UE_LOG(LogGFCApi, Log, TEXT("Logged out: cache wiped."));
	}
	bWasLoggedIn = bLoggedIn;
}

bool UGFCCacheSubsystem::IsOnCellular()
{
	return FPlatformMisc::GetNetworkConnectionType() == ENetworkConnectionType::Cell;
}
