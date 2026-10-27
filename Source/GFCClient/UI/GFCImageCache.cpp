#include "UI/GFCImageCache.h"

#include "Engine/Texture2D.h"
#include "GFCApiClient.h"
#include "GFCApiSubsystem.h"
#include "ImageUtils.h"

void UGFCImageCache::Request(const FString& Url, FOnImage Done)
{
	if (Url.IsEmpty() || Failed.Contains(Url))
	{
		Done(nullptr);
		return;
	}
	if (const TObjectPtr<UTexture2D>* Hit = Cache.Find(Url))
	{
		Done(*Hit);
		return;
	}
	if (TArray<FOnImage>* Waiting = Pending.Find(Url))
	{
		Waiting->Add(MoveTemp(Done));
		return;
	}
	Pending.Add(Url).Add(MoveTemp(Done));

	UGFCApiSubsystem* Api = GetGameInstance()->GetSubsystem<UGFCApiSubsystem>();
	TWeakObjectPtr<UGFCImageCache> WeakThis(this);
	Api->GetClient()->DownloadBytes(Url, MaxImageBytes, [WeakThis, Url](bool bOk, const TArray<uint8>& Bytes)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		UTexture2D* Tex = bOk ? FImageUtils::ImportBufferAsTexture2D(Bytes) : nullptr;
		if (Tex)
		{
			if (WeakThis->Cache.Num() >= MaxCachedImages)
			{
				WeakThis->Cache.Reset();
			}
			WeakThis->Cache.Add(Url, Tex);
		}
		else
		{
			WeakThis->Failed.Add(Url);
		}
		TArray<FOnImage> Callbacks;
		WeakThis->Pending.RemoveAndCopyValue(Url, Callbacks);
		for (FOnImage& Cb : Callbacks)
		{
			Cb(Tex);
		}
	});
}
