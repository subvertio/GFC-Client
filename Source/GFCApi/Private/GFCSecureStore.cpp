#include "GFCSessionStore.h"

#include "GFCApi.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincrypt.h>
#include <dpapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
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
	return PLATFORM_WINDOWS != 0;
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
#else
	// TODO(android-keystore): see header.
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
#else
	WarnPlainOnce();
	OutSecret = FromUtf8(Blob.GetData(), Blob.Num());
	return !OutSecret.IsEmpty();
#endif
}
