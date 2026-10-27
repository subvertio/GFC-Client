#include "GFCApiSubsystem.h"

#include "GFCApi.h"
#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCUrl.h"

#include "HAL/PlatformFileManager.h"
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

const TCHAR* UGFCApiSubsystem::SlotName = TEXT("GFCSession");

void UGFCApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Client = NewObject<UGFCApiClient>(this);

	if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SlotName, 0))
	{
		Save = Cast<UGFCSessionSave>(Loaded);
	}
	if (!Save)
	{
		Save = Cast<UGFCSessionSave>(UGameplayStatics::CreateSaveGameObject(UGFCSessionSave::StaticClass()));
	}

	// A -GFCNode= command line wins over the node picked on the login screen.
	FString CommandLineNode;
	const bool bCommandLineNode = FParse::Value(FCommandLine::Get(), TEXT("GFCNode="), CommandLineNode);
	if (!bCommandLineNode && !Save->NodeOverride.IsEmpty())
	{
		Client->SetBaseUrl(Save->NodeOverride);
	}

	// Restore the session only for the node it was issued by.
	FString Token;
	if (!Save->Username.IsEmpty() && Save->NodeBaseUrl == Client->GetBaseUrl()
		&& FGFCSecureStore::Unprotect(Save->ProtectedToken, Token))
	{
		bRestoring = true;
		Client->SetSession(Save->Username, Token);
		bRestoring = false;
	}

	Client->OnSessionChanged.AddUObject(this, &UGFCApiSubsystem::HandleClientSessionChanged);
}

void UGFCApiSubsystem::Deinitialize()
{
	if (Client)
	{
		Client->OnSessionChanged.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UGFCApiSubsystem::HandleClientSessionChanged()
{
	if (!bRestoring)
	{
		Persist();
	}
	OnSessionChanged.Broadcast();
}

void UGFCApiSubsystem::Persist()
{
	Save->Username = Client->GetUsername();
	Save->NodeBaseUrl = Client->GetBaseUrl();
	Save->ProtectedToken.Reset();
	if (Client->IsLoggedIn() && !FGFCSecureStore::Protect(Client->GetTokenForStorage(), Save->ProtectedToken))
	{
		UE_LOG(LogGFCApi, Warning, TEXT("Couldn't protect the session token; it won't be remembered."));
		Save->ProtectedToken.Reset();
	}
	UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

bool UGFCApiSubsystem::SetNode(const FString& BaseUrl)
{
	const FString Normalized = FGFCUrl::NormalizeBaseUrl(BaseUrl);
	if (Normalized.IsEmpty())
	{
		return false;
	}
	if (Normalized != Client->GetBaseUrl())
	{
		Client->ClearSession(); // tokens belong to one node
		Client->SetBaseUrl(Normalized);
	}
	Save->NodeOverride = Normalized == UGFCApiSettings::GetEffectiveNodeBaseUrl() ? FString() : Normalized;
	Persist();
	return true;
}

TArray<FGFCKnownWallet> UGFCApiSubsystem::GetKnownWallets() const
{
	TArray<FGFCKnownWallet> Out;
	for (const FGFCKnownWallet& W : Save->KnownWallets)
	{
		if (W.Owner.Equals(Client->GetUsername(), ESearchCase::IgnoreCase) && W.NodeBaseUrl == Client->GetBaseUrl())
		{
			Out.Add(W);
		}
	}
	return Out;
}

void UGFCApiSubsystem::RememberWallet(const FString& WalletId, const FString& Name)
{
	if (WalletId.IsEmpty() || !Client->IsLoggedIn())
	{
		return;
	}
	for (FGFCKnownWallet& W : Save->KnownWallets)
	{
		if (W.WalletId == WalletId && W.Owner.Equals(Client->GetUsername(), ESearchCase::IgnoreCase))
		{
			W.Name = Name.IsEmpty() ? W.Name : Name;
			Persist();
			return;
		}
	}
	FGFCKnownWallet& W = Save->KnownWallets.AddDefaulted_GetRef();
	W.Owner = Client->GetUsername();
	W.NodeBaseUrl = Client->GetBaseUrl();
	W.WalletId = WalletId;
	W.Name = Name;
	Persist();
}

int32 UGFCApiSubsystem::LearnWallets(const TArray<FString>& WalletIds)
{
	int32 Added = 0;
	const TArray<FGFCKnownWallet> Known = GetKnownWallets();
	for (const FString& Id : WalletIds)
	{
		if (!Known.ContainsByPredicate([&](const FGFCKnownWallet& W) { return W.WalletId == Id; }))
		{
			RememberWallet(Id, Added == 0 && Known.Num() == 0 ? TEXT("main") : TEXT(""));
			++Added;
		}
	}
	return Added;
}

int64 UGFCApiSubsystem::EarnedSince(int64 LastSeenCents, int64 NowCents)
{
	return LastSeenCents >= 0 && NowCents > LastSeenCents ? NowCents - LastSeenCents : 0;
}

void UGFCApiSubsystem::NoteBalance(const FString& WalletId, int64 Cents)
{
	for (FGFCKnownWallet& W : Save->KnownWallets)
	{
		if (W.WalletId == WalletId && W.Owner.Equals(Client->GetUsername(), ESearchCase::IgnoreCase)
			&& W.NodeBaseUrl == Client->GetBaseUrl() && W.LastSeenCents != Cents)
		{
			W.LastSeenCents = Cents;
			Persist();
			return;
		}
	}
}

void UGFCApiSubsystem::CheckEarnings(TFunction<void(int64 EarnedCents)> Done)
{
	const TArray<FGFCKnownWallet> Wallets = GetKnownWallets();
	if (Wallets.IsEmpty() || !Client->IsLoggedIn())
	{
		Done(0);
		return;
	}
	struct FTally { int32 Pending = 0; int64 Earned = 0; TFunction<void(int64)> Done; };
	TSharedRef<FTally> Tally = MakeShared<FTally>();
	Tally->Pending = Wallets.Num();
	Tally->Done = MoveTemp(Done);
	TWeakObjectPtr<UGFCApiSubsystem> WeakThis(this);
	for (const FGFCKnownWallet& Known : Wallets)
	{
		const FString Id = Known.WalletId;
		const int64 LastSeen = Known.LastSeenCents;
		Client->GetWallet(Id, [WeakThis, Tally, Id, LastSeen](const FGFCApiResult& R, const FGFCWallet& W)
		{
			if (R.bOk && WeakThis.IsValid())
			{
				Tally->Earned += EarnedSince(LastSeen, W.BalanceCents);
				WeakThis->NoteBalance(Id, W.BalanceCents);
			}
			if (--Tally->Pending == 0 && WeakThis.IsValid())
			{
				Tally->Done(Tally->Earned);
			}
		});
	}
}

void UGFCApiSubsystem::ForgetWallet(const FString& WalletId)
{
	Save->KnownWallets.RemoveAll([&](const FGFCKnownWallet& W)
	{
		return W.WalletId == WalletId && W.Owner.Equals(Client->GetUsername(), ESearchCase::IgnoreCase);
	});
	Persist();
}

void UGFCApiSubsystem::MarkWalletsBackedUp()
{
	for (FGFCKnownWallet& W : Save->KnownWallets)
	{
		if (W.Owner.Equals(Client->GetUsername(), ESearchCase::IgnoreCase) && W.NodeBaseUrl == Client->GetBaseUrl())
		{
			W.bBackedUp = true;
		}
	}
	Persist();
}

bool UGFCApiSubsystem::NeedsBackup() const
{
	for (const FGFCKnownWallet& W : GetKnownWallets())
	{
		if (!W.bBackedUp)
		{
			return true;
		}
	}
	return false;
}

FString UGFCApiSubsystem::SaveBackupFile(const FString& BackupJson) const
{
	const FString Safe = FPaths::MakeValidFileName(Client->GetUsername(), TEXT('_'));
	const FString Name = FString::Printf(TEXT("%s-wallet-backup-%s.json"), *Safe, *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")));
#if PLATFORM_ANDROID
	// Downloads/GFC, where the user can see it and copy it off the phone (and it outlives the app).
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
			"AndroidThunkJava_GFCSaveToDownloads", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", false);
		if (Method)
		{
			auto JName = FJavaHelper::ToJavaString(Env, Name);
			auto JText = FJavaHelper::ToJavaString(Env, BackupJson);
			jstring JResult = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, Method, *JName, *JText);
			const FString Where = FJavaHelper::FStringFromLocalRef(Env, JResult);
			if (!Where.IsEmpty())
			{
				return Where;
			}
		}
	}
#endif
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups"));
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.CreateDirectoryTree(*Dir))
	{
		return FString();
	}
	const FString Path = Dir / Name;
	return FFileHelper::SaveStringToFile(BackupJson, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ? Path : FString();
}
