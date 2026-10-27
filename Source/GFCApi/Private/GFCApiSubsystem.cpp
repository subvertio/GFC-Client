#include "GFCApiSubsystem.h"

#include "GFCApi.h"
#include "GFCApiClient.h"
#include "GFCApiSettings.h"
#include "GFCUrl.h"

#include "HAL/PlatformFileManager.h"
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
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups"));
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.CreateDirectoryTree(*Dir))
	{
		return FString();
	}
	const FString Safe = FPaths::MakeValidFileName(Client->GetUsername(), TEXT('_'));
	const FString Path = Dir / FString::Printf(TEXT("%s-wallet-backup-%s.json"), *Safe, *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")));
	return FFileHelper::SaveStringToFile(BackupJson, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ? Path : FString();
}
