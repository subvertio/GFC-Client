#include "GFCSessionStore.h"

#include "GFCApi.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincrypt.h>
#include <dpapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace
{
	// Not a secret: just ties the blob to this app so other DPAPI users can't swap it in.
	const char* GEntropy = "GFC-Client/session-token/v1";

	void ToUtf8(const FString& S, TArray<uint8>& Out)
	{
		const FTCHARToUTF8 Utf8(*S);
		Out.SetNumUninitialized(Utf8.Length());
		FMemory::Memcpy(Out.GetData(), Utf8.Get(), Utf8.Length());
	}

	FString FromUtf8(const uint8* Data, int32 Len)
	{
		const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Data), Len);
		return FString(Conv.Length(), Conv.Get());
	}

#if PLATFORM_ANDROID
	// Blobs made by the Android Keystore path start with this; anything else is a plain
	// token saved by 0.1.0, accepted once and re-saved protected (UGFCApiSubsystem).
	const TCHAR* GAndroidPrefix = TEXT("GK1:");

	/** Calls a GameActivity String(String) method from GFCApi_UPL.xml; empty on any failure. */
	FString CallActivity(const char* Name, const FString& Arg)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env)
		{
			return FString();
		}
		const jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, Name, "(Ljava/lang/String;)Ljava/lang/String;", false);
		if (!Method)
		{
			return FString();
		}
		auto JArg = FJavaHelper::ToJavaString(Env, Arg);
		jstring JResult = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, Method, *JArg);
		return FJavaHelper::FStringFromLocalRef(Env, JResult);
	}
#endif

	void WarnPlainOnce()
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogGFCApi, Warning, TEXT("Session token is stored without OS protection on this platform (see FGFCSecureStore TODO)."));
		}
	}
}

bool FGFCSecureStore::IsHardwareOrOsBacked()
{
	return PLATFORM_WINDOWS != 0 || PLATFORM_ANDROID != 0;
}

bool FGFCSecureStore::NeedsUpgrade(const TArray<uint8>& Blob)
{
#if PLATFORM_ANDROID
	return Blob.Num() > 0 && !FromUtf8(Blob.GetData(), Blob.Num()).StartsWith(GAndroidPrefix, ESearchCase::CaseSensitive);
#else
	return false;
#endif
}

bool FGFCSecureStore::Protect(const FString& Secret, TArray<uint8>& OutBlob)
{
	TArray<uint8> Plain;
	ToUtf8(Secret, Plain);
#if PLATFORM_WINDOWS
	DATA_BLOB In{static_cast<DWORD>(Plain.Num()), Plain.GetData()};
	DATA_BLOB Entropy{static_cast<DWORD>(FCStringAnsi::Strlen(GEntropy)), reinterpret_cast<BYTE*>(const_cast<char*>(GEntropy))};
	DATA_BLOB Out{0, nullptr};
	const bool bOk = CryptProtectData(&In, L"GFC session", &Entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out) != 0;
	FMemory::Memzero(Plain.GetData(), Plain.Num());
	if (!bOk)
	{
		return false;
	}
	OutBlob.SetNumUninitialized(Out.cbData);
	FMemory::Memcpy(OutBlob.GetData(), Out.pbData, Out.cbData);
	LocalFree(Out.pbData);
	return true;
#elif PLATFORM_ANDROID
	FMemory::Memzero(Plain.GetData(), Plain.Num());
	const FString Sealed = CallActivity("AndroidThunkJava_GFCProtect", Secret);
	if (Sealed.IsEmpty())
	{
		return false;
	}
	ToUtf8(GAndroidPrefix + Sealed, OutBlob);
	return true;
#else
	WarnPlainOnce();
	OutBlob = MoveTemp(Plain);
	return true;
#endif
}

bool FGFCSecureStore::Unprotect(const TArray<uint8>& Blob, FString& OutSecret)
{
	if (Blob.Num() == 0)
	{
		return false;
	}
#if PLATFORM_WINDOWS
	DATA_BLOB In{static_cast<DWORD>(Blob.Num()), const_cast<BYTE*>(Blob.GetData())};
	DATA_BLOB Entropy{static_cast<DWORD>(FCStringAnsi::Strlen(GEntropy)), reinterpret_cast<BYTE*>(const_cast<char*>(GEntropy))};
	DATA_BLOB Out{0, nullptr};
	if (!CryptUnprotectData(&In, nullptr, &Entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
	{
		return false;
	}
	OutSecret = FromUtf8(Out.pbData, static_cast<int32>(Out.cbData));
	SecureZeroMemory(Out.pbData, Out.cbData);
	LocalFree(Out.pbData);
	return !OutSecret.IsEmpty();
#elif PLATFORM_ANDROID
	const FString Stored = FromUtf8(Blob.GetData(), Blob.Num());
	if (Stored.StartsWith(GAndroidPrefix, ESearchCase::CaseSensitive))
	{
		OutSecret = CallActivity("AndroidThunkJava_GFCUnprotect", Stored.RightChop(FCString::Strlen(GAndroidPrefix)));
		return !OutSecret.IsEmpty();
	}
	OutSecret = Stored;  // a plain token from 0.1.0; NeedsUpgrade() makes the caller re-save it protected
	return !OutSecret.IsEmpty();
#else
	WarnPlainOnce();
	OutSecret = FromUtf8(Blob.GetData(), Blob.Num());
	return !OutSecret.IsEmpty();
#endif
}
