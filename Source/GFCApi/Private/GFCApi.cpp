#include "GFCApi.h"

#include "GFCSingleInstance.h"

DEFINE_LOG_CATEGORY(LogGFCApi);

IMPLEMENT_MODULE(FGFCApiModule, GFCApi);

void FGFCApiModule::StartupModule()
{
	// Packaged Windows app: before the game window exists, so a second launch never shows one.
	GFCSingleInstance::Start();
}

void FGFCApiModule::ShutdownModule()
{
	GFCSingleInstance::Stop();
}
