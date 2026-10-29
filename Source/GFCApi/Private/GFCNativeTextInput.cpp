#include "GFCNativeTextInput.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace GFCNativeTextInput
{
#if PLATFORM_ANDROID
	namespace
	{
		int32 Argb(const FColor& C)
		{
			return int32((uint32(C.A) << 24) | (uint32(C.R) << 16) | (uint32(C.G) << 8) | uint32(C.B));
		}
	}

	bool IsSupported()
	{
		return true;
	}

	void Place(const FGFCNativeTextParams& P)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env)
		{
			return;
		}
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GFCPlaceTextInput",
			"(IIIIIFIIIIIIIIFLjava/lang/String;Z)V", false);
		if (!Method)
		{
			return;
		}
		auto JHint = FJavaHelper::ToJavaString(Env, P.Hint);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method,
			P.Rect.Min.X, P.Rect.Min.Y, P.Rect.Width(), P.Rect.Height(), P.WindowWidth,
			P.TextPx, P.Padding.Min.X, P.Padding.Min.Y, P.Padding.Max.X, P.Padding.Max.Y,
			Argb(P.TextColor), Argb(P.HintColor), Argb(P.Background), Argb(P.Accent), P.CornerRadius, *JHint, jboolean(P.bMultiLine));
	}

	void Release()
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env)
		{
			return;
		}
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GFCReleaseTextInput", "()V", false);
		if (Method)
		{
			FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
		}
	}
#else
	bool IsSupported()
	{
		return false;
	}

	void Place(const FGFCNativeTextParams&) {}
	void Release() {}
#endif
}
