#pragma once

#include "Modules/ModuleManager.h"

GFCAPI_API DECLARE_LOG_CATEGORY_EXTERN(LogGFCApi, Log, All);

class FGFCApiModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
