#include "GFCApiSettings.h"

#include "GFCApi.h"
#include "GFCUrl.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const TCHAR* UGFCApiSettings::DefaultNodeBaseUrl = TEXT("https://node.galacticfederation.tech");
const TCHAR* UGFCApiSettings::DefaultShareBaseUrl = TEXT("https://galacticfederation.tech");

UGFCApiSettings::UGFCApiSettings()
{
	NodeBaseUrl = DefaultNodeBaseUrl;
	ShareBaseUrl = DefaultShareBaseUrl;
}

FString UGFCApiSettings::GetEffectiveNodeBaseUrl()
{
	FString FromCommandLine;
	if (FParse::Value(FCommandLine::Get(), TEXT("GFCNode="), FromCommandLine))
	{
		const FString Normalized = FGFCUrl::NormalizeBaseUrl(FromCommandLine);
		if (!Normalized.IsEmpty())
		{
			return Normalized;
		}
	}
	const FString FromSettings = FGFCUrl::NormalizeBaseUrl(GetDefault<UGFCApiSettings>()->NodeBaseUrl);
	if (FromSettings.IsEmpty())
	{
		UE_LOG(LogGFCApi, Warning, TEXT("GFCApiSettings.NodeBaseUrl isn't a valid URL (quote it in the ini); using %s"), DefaultNodeBaseUrl);
		return DefaultNodeBaseUrl;
	}
	return FromSettings;
}

FString UGFCApiSettings::GetEffectiveShareBaseUrl()
{
	const FString FromSettings = FGFCUrl::NormalizeBaseUrl(GetDefault<UGFCApiSettings>()->ShareBaseUrl);
	return FromSettings.IsEmpty() ? FString(DefaultShareBaseUrl) : FromSettings;
}
