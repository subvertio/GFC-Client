// Android side of deep links (see GFCApi_UPL.xml). Compiled only for Android.

#include "CoreMinimal.h"

#if PLATFORM_ANDROID

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "GFCDeepLink.h"

// A link that arrives while the app is running (GameActivity.onNewIntent).
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeGFCOnDeepLink(JNIEnv* Env, jobject Thiz, jstring JUrl)
{
	const FString Url = FJavaHelper::FStringFromParam(Env, JUrl);
	FGFCDeepLink::Receive(Url);
}

namespace GFCAndroid
{
	/** The link that launched the app, if any. Called once at startup. */
	void ConsumeLaunchLink()
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env)
		{
			return;
		}
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GFCTakePendingLink", "()Ljava/lang/String;", false);
		if (!Method)
		{
			return;
		}
		jstring JUrl = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, Method);
		const FString Url = FJavaHelper::FStringFromLocalRef(Env, JUrl);
		if (!Url.IsEmpty())
		{
			FGFCDeepLink::Receive(Url);
		}
	}
}

#endif
