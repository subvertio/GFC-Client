# GFC Client

Unreal Engine 5.7 front end for **Galactic Federation Coin**: browse the seed
feed, manage your GFC wallet, send GFC, back up your wallet, and share seeds.
PC (Win64) first, then Android.

The backend and its API live in [subvertio/GFCCoin](https://github.com/subvertio/GFCCoin)
(see `docs/build-your-own-frontend.md` there, and `/docs` on any node).

Milestone 1 builds its whole UI in C++ (UMG constructed in code), so the project
opens and runs with no editor-made assets. Designers can swap in Widget
Blueprints later: see [docs/ui-spec.md](docs/ui-spec.md). What's next:
[docs/roadmap.md](docs/roadmap.md).

## Layout

```
GFCClient.uproject
Config/                  DefaultEngine.ini, DefaultGame.ini (node URL)
Source/GFCApi/           Runtime module: HTTP client, JSON parsing, session storage, tests
Source/GFCClient/        Game module: game mode, player controller, UI/ screens
Scripts/                 Build, run, test and local-node scripts (PowerShell)
docs/                    UI spec, roadmap
```

## Requirements (Windows)

- Unreal Engine **5.7** (Epic Games Launcher), default path `C:\Program Files\Epic Games\UE_5.7`.
  Set `UE_ROOT` if yours is elsewhere.
- Visual Studio 2022 with **Game development with C++** (MSVC v143, Windows SDK).
- Git LFS (`git lfs install`) before cloning: binary assets are stored in LFS.
- For the local test node: Python 3.11+ and a GFCCoin checkout with its
  `requirements.txt` installed.

## Build

```powershell
.\Scripts\Build.ps1
```

This builds **GFCClientEditor** and **GFCClient** (Development, Win64) with
UnrealBuildTool. Use `-Targets Editor` for the editor only. If the compiler
runs out of memory (C3859 / C1076), lower parallelism: `-Jobs 4`.

Or right-click `GFCClient.uproject` > **Generate Visual Studio project files**,
open `GFCClient.sln`, choose **Development Editor / Win64** and build.

## Run

```powershell
.\Scripts\Run.ps1
```

Opens the client in a window (`UnrealEditor.exe GFCClient.uproject -game`).
To open the editor instead, double-click `GFCClient.uproject`.

## Point it at a node

By default the client talks to `https://node.galacticfederation.tech`.

- **Per launch:** `-GFCNode=http://127.0.0.1:8000`, or `.\Scripts\Run.ps1 -Node http://127.0.0.1:8000`.
- **Per user:** the node box at the bottom of the login screen (remembered).
- **Project default:** Project Settings > Game > **GFC API**, or edit
  `Config/DefaultGame.ini`:

  ```ini
  [/Script/GFCApi.GFCApiSettings]
  NodeBaseUrl="https://node.galacticfederation.tech"
  ShareBaseUrl="https://galacticfederation.tech"
  ```

  Keep the quotes: without them the ini parser treats `//` as a comment.

A session belongs to the node that issued it; switching nodes logs you out.

## Security

- Wallet private keys never reach the client. The node signs transfers and
  holds keys encrypted with your password.
- The token is sent only as `Authorization: Bearer …` and is never logged.
- On Windows the saved token is encrypted with **DPAPI** (bound to your Windows
  account) in `Saved/SaveGames/GFCSession.sav`. Android Keystore storage is a
  TODO for the Android build (`Source/GFCApi/Public/GFCSessionStore.h`).
- Wallet backups are passphrase-encrypted by the node and saved to
  `Saved/Backups/`. Move them somewhere safe and store the passphrase apart.

## Test

### Unit tests (headless)

```powershell
.\Scripts\RunTests.ps1
```

Runs the `GFC` automation tests in `UnrealEditor-Cmd` with `-nullrhi`:
JSON parsing, error handling, amounts, URL building, settings, DPAPI storage,
and client behaviour offline. Also in the editor: **Tools > Session Frontend > Automation**, filter `GFC`.

### End-to-end against a local node

Never run the end-to-end test against the public node: it registers throwaway accounts.

```powershell
# Terminal 1: an isolated node on 127.0.0.1 with scratch data and P2P switched off
.\Scripts\RunLocalNode.ps1 -Port 8100 -Fresh
```

```powershell
# Terminal 2: demo seeds, then the tests including GFC.E2E.LocalNode
py -3 .\Scripts\seed_local_node.py http://127.0.0.1:8100
.\Scripts\RunTests.ps1 -E2ENode http://127.0.0.1:8100 -ExpectSeed <one of the printed ids> -SaveSession
.\Scripts\Run.ps1 -Node http://127.0.0.1:8100
```

`RunLocalNode.ps1` runs the GFCCoin backend (`-BackendDir`, default
`D:\projects\GFC`) through `Scripts/local_node.py`, which turns off the
backend's P2P sync so test data never leaves your machine. The E2E test
registers, logs in, creates wallets, reads balance and history, checks transfer
errors, reads the feed and a seed, exports a backup, and logs out (checking the
token is revoked). `-SaveSession` leaves its test user logged in so `Run.ps1`
opens straight to the feed. `Run.ps1 -Screen wallet` (or `feed`, `seed:<id>`)
opens a screen directly.

## Android (UE 5.7)

Not set up on this machine yet. The code is Android-ready (no Windows-only
code outside `#if PLATFORM_WINDOWS`), and `DefaultEngine.ini` already sets the
package name and the `INTERNET` permission.

UE 5.7 expects (from `Engine/Config/Android/Android_SDK.json`):

| Component | Version |
|---|---|
| NDK | r27c (`27.2.12479018`); r27c to r28c accepted |
| Platform | `android-34` |
| Build tools | `35.0.1` |
| CMake | `3.22.1` |
| Min / target SDK (engine defaults) | 26 / 34 |
| JDK | the one bundled with Android Studio |

Steps:

1. Install **Android Studio** (a recent stable release), open it once, and let
   it install the default SDK.
2. Close it, then run Epic's setup script. It installs the SDK packages and
   NDK above and sets `ANDROID_HOME`, `NDK_ROOT` and `JAVA_HOME`:

   ```powershell
   & "C:\Program Files\Epic Games\UE_5.7\Engine\Extras\Android\SetupAndroid.bat"
   ```

3. Open a new terminal, then check the setup with Turnkey:

   ```powershell
   & "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" Turnkey -command=VerifySdk -platform=Android -UpdateIfNeeded -project="$PWD\GFCClient.uproject"
   ```

4. In the editor: Project Settings > Platforms > Android, click **Accept SDK License**.
5. Package from **Platforms > Android > Package Project** (ASTC), or:

   ```powershell
   & "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="$PWD\GFCClient.uproject" -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -pak -package -archive -archivedirectory="$PWD\Packaged"
   ```

Before shipping on Android: Keystore-backed token storage, share-sheet
backup export and a touch-sized UI (see [docs/roadmap.md](docs/roadmap.md)).
