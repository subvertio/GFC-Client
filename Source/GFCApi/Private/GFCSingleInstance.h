#pragma once

#include "CoreMinimal.h"

/**
 * One running copy of the packaged Windows app. A second launch (a gfc: link clicked in the browser, or the
 * Start menu shortcut while the app is open) hands its -GFCLink to the running copy over a named pipe, brings
 * that window to the front and exits. The running copy feeds the link to FGFCDeepLink::Receive.
 * Only active in packaged Windows builds; editor and -game runs from Run.ps1 are unaffected.
 */
namespace GFCSingleInstance
{
	/** Claims the instance, or forwards this launch's link to the running copy and ends this process. */
	void Start();

	/** Stops listening for forwarded links. */
	void Stop();
}
