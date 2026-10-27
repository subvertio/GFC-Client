#include "GFCSingleInstance.h"

#if PLATFORM_WINDOWS && !WITH_EDITOR

#include "GFCApi.h"
#include "GFCDeepLink.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include <atomic>

#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	// A link is a short URL; anything longer is cut off (and then fails link validation).
	constexpr DWORD MaxLinkBytes = 4096;

	HANDLE GMutex = nullptr;
	HANDLE GPipe = INVALID_HANDLE_VALUE;
	FRunnable* GListener = nullptr;
	FRunnableThread* GThread = nullptr;

	/** Pipe names are machine-wide, so the session id keeps two signed-in users apart. */
	FString PipeName()
	{
		DWORD Session = 0;
		::ProcessIdToSessionId(::GetCurrentProcessId(), &Session);
		return FString::Printf(TEXT("\\\\.\\pipe\\GFCClient-%u"), Session);
	}

	BOOL CALLBACK FindOurWindow(HWND Window, LPARAM Out)
	{
		DWORD Pid = 0;
		::GetWindowThreadProcessId(Window, &Pid);
		if (Pid == ::GetCurrentProcessId() && ::IsWindowVisible(Window) && !::GetWindow(Window, GW_OWNER))
		{
			*reinterpret_cast<HWND*>(Out) = Window;
			return 0;  // stop enumerating
		}
		return 1;
	}

	void BringToFront()
	{
		HWND Window = nullptr;
		::EnumWindows(&FindOurWindow, reinterpret_cast<LPARAM>(&Window));
		if (Window)
		{
			if (::IsIconic(Window))
			{
				::ShowWindowAsync(Window, SW_RESTORE);
			}
			::SetForegroundWindow(Window);
		}
	}

	/** Sends Link (may be empty: just focus) to the running copy. */
	bool Forward(const FString& Link)
	{
		const FString Name = PipeName();
		HANDLE Pipe = INVALID_HANDLE_VALUE;
		// The running copy may be between two clients, or still starting up.
		for (int32 Try = 0; Try < 20 && Pipe == INVALID_HANDLE_VALUE; ++Try)
		{
			Pipe = ::CreateFileW(*Name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if (Pipe == INVALID_HANDLE_VALUE)
			{
				if (::GetLastError() == ERROR_PIPE_BUSY)
				{
					::WaitNamedPipeW(*Name, 250);
				}
				else
				{
					::Sleep(250);
				}
			}
		}
		if (Pipe == INVALID_HANDLE_VALUE)
		{
			return false;
		}
		// Windows only lets the foreground process hand focus on; the browser launched us, so we have it.
		ULONG ServerPid = 0;
		if (::GetNamedPipeServerProcessId(Pipe, &ServerPid))
		{
			::AllowSetForegroundWindow(ServerPid);
		}
		const FTCHARToUTF8 Utf8(*Link);
		DWORD Written = 0;
		const bool bOk = Link.IsEmpty() || ::WriteFile(Pipe, Utf8.Get(), FMath::Min<DWORD>(Utf8.Length(), MaxLinkBytes), &Written, nullptr);
		::CloseHandle(Pipe);
		return bOk;
	}

	class FLinkListener final : public FRunnable
	{
	public:
		virtual uint32 Run() override
		{
			while (!bStopping)
			{
				const bool bConnected = ::ConnectNamedPipe(GPipe, nullptr) || ::GetLastError() == ERROR_PIPE_CONNECTED;
				if (bConnected && !bStopping)
				{
					// The sender writes the link and closes; read until it does.
					TArray<ANSICHAR> Bytes;
					ANSICHAR Chunk[512];
					DWORD Read = 0;
					while (::ReadFile(GPipe, Chunk, sizeof(Chunk), &Read, nullptr) && Read > 0 && (DWORD)Bytes.Num() < MaxLinkBytes)
					{
						Bytes.Append(Chunk, Read);
					}
					Bytes.Add('\0');
					const FString Link = FString(UTF8_TO_TCHAR(Bytes.GetData())).TrimStartAndEnd();
					if (!Link.IsEmpty())
					{
						FGFCDeepLink::Receive(Link);
					}
					BringToFront();
				}
				::DisconnectNamedPipe(GPipe);
			}
			return 0;
		}

		virtual void Stop() override
		{
			bStopping = true;
			// Wake ConnectNamedPipe with a connection of our own.
			HANDLE Self = ::CreateFileW(*PipeName(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if (Self != INVALID_HANDLE_VALUE)
			{
				::CloseHandle(Self);
			}
		}

	private:
		std::atomic<bool> bStopping{false};
	};
}

void GFCSingleInstance::Start()
{
	// Local\ is per sign-in session.
	GMutex = ::CreateMutexW(nullptr, 1, L"Local\\GFCClient.SingleInstance");
	if (GMutex && ::GetLastError() == ERROR_ALREADY_EXISTS)
	{
		FString Link;
		FParse::Value(FCommandLine::Get(), TEXT("GFCLink="), Link);
		Forward(Link);
		// Nothing is loaded yet that needs saving or shutting down; a normal exit from module startup would.
		::TerminateProcess(::GetCurrentProcess(), 0);
		return;
	}

	// FIRST_PIPE_INSTANCE: if something else already holds the name, don't share it.
	GPipe = ::CreateNamedPipeW(*PipeName(), PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 0, MaxLinkBytes, 0, nullptr);
	if (GPipe == INVALID_HANDLE_VALUE)
	{
		UE_LOG(LogGFCApi, Warning, TEXT("Links opened while the app runs won't reach it (pipe error %u)."), ::GetLastError());
		return;
	}
	GListener = new FLinkListener();
	GThread = FRunnableThread::Create(GListener, TEXT("GFCLinkListener"), 0, TPri_BelowNormal);
}

void GFCSingleInstance::Stop()
{
	if (GThread)
	{
		GThread->Kill(true);
		delete GThread;
		GThread = nullptr;
	}
	delete GListener;
	GListener = nullptr;
	if (GPipe != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(GPipe);
		GPipe = INVALID_HANDLE_VALUE;
	}
	if (GMutex)
	{
		::ReleaseMutex(GMutex);
		::CloseHandle(GMutex);
		GMutex = nullptr;
	}
}

#else

void GFCSingleInstance::Start() {}
void GFCSingleInstance::Stop() {}

#endif
