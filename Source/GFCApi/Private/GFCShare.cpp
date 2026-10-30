#include "GFCShare.h"

#include "GFCApi.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

FString FGFCShare::TeaserFileName(const FString& Title, const FString& SeedId)
{
	FString Base = FPaths::MakeValidFileName(Title.TrimStartAndEnd().Left(60), TEXT('_')).TrimStartAndEnd();
	if (Base.IsEmpty())
	{
		Base = TEXT("GFC-") + SeedId.Left(12);
	}
	return Base + TEXT("-teaser.mp4");
}

FString FGFCShare::WindowsVideosDir()
{
	const FString Profile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (!Profile.IsEmpty() && IFileManager::Get().DirectoryExists(*FPaths::Combine(Profile, TEXT("Videos"))))
	{
		return FPaths::Combine(Profile, TEXT("Videos"), TEXT("GFC"));
	}
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Teasers"));
}

FGFCShare::EResult FGFCShare::ShareVideo(const FString& VideoPath, const FString& SaveName, const FString& Text,
	const FString& XIntentUrl, FString* OutSavedPath)
{
	const bool bHaveClip = !VideoPath.IsEmpty() && IFileManager::Get().FileSize(*VideoPath) > 0;
#if PLATFORM_ANDROID
	if (bHaveClip)
	{
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
				"AndroidThunkJava_GFCShareVideo", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z", false);
			if (Method)
			{
				auto JPath = FJavaHelper::ToJavaString(Env, VideoPath);
				auto JName = FJavaHelper::ToJavaString(Env, SaveName);
				auto JText = FJavaHelper::ToJavaString(Env, Text);
				if (FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method, *JPath, *JName, *JText))
				{
					return EResult::SheetOpened;
				}
			}
		}
		UE_LOG(LogGFCApi, Warning, TEXT("Share sheet unavailable; opening the X post page instead"));
	}
#else
	if (bHaveClip)
	{
		const FString Dir = WindowsVideosDir();
		const FString Dest = FPaths::Combine(Dir, SaveName);
		IFileManager::Get().MakeDirectory(*Dir, true);
		if (IFileManager::Get().Copy(*Dest, *VideoPath, true, true) == COPY_OK)
		{
			if (OutSavedPath) *OutSavedPath = Dest;
			FPlatformProcess::ExploreFolder(*Dest); // selects the clip, ready to drag into X
			FString Error;
			if (!XIntentUrl.IsEmpty()) FPlatformProcess::LaunchURL(*XIntentUrl, nullptr, &Error);
			return EResult::SavedAndOpened;
		}
		UE_LOG(LogGFCApi, Warning, TEXT("Couldn't save the teaser to %s"), *Dir);
	}
#endif
	if (XIntentUrl.IsEmpty())
	{
		return EResult::Failed;
	}
	FString Error;
	FPlatformProcess::LaunchURL(*XIntentUrl, nullptr, &Error);
	return Error.IsEmpty() ? EResult::OpenedLink : EResult::Failed;
}
