; Windows installer for the packaged GFC app. Built by Scripts\PackageWindows.ps1, which passes:
;   AppVersion, AppCommit, AppDir (Packaged\Windows), GameExe, OutDir, IconFile
; Installs per user by default (no admin prompt), offers all users; adds a Start menu entry, an optional
; desktop shortcut, the gfc: link handler, and an uninstaller. The VC++ runtime is installed only if missing.

#ifndef AppVersion
  #error Build this with Scripts\PackageWindows.ps1
#endif

#define AppName "GFC"
#define AppExe "GFCClient\Binaries\Win64\" + GameExe
#define Prereq "Engine\Extras\Redist\en-us\vc_redist.x64.exe"

[Setup]
; Never change AppId: it's how upgrades and the uninstaller find an existing install.
AppId={{C7D2A4E1-5B3F-4E8A-9C61-2F0B7D3E4A15}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Subvertio
AppPublisherURL=https://galacticfederation.tech
AppSupportURL=https://galacticfederation.tech
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutDir}
OutputBaseFilename=GFC-Setup-{#AppVersion}-{#AppCommit}
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; The running app holds this mutex (GFCSingleInstance.cpp): setup asks the user to close it first.
AppMutex=Local\GFCClient.SingleInstance
CloseApplications=yes
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#AppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#AppDir}\*"; DestDir: "{app}"; Excludes: "\Manifest_*.txt,\Engine\Extras\Redist\*"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#AppDir}\{#Prereq}"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist; Check: NeedsVCRuntime

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; gfc:// links (share links and the return from phone verification) open in the app. HKA is the current
; user's classes for a per-user install, the machine's for an all-users install.
Root: HKA; Subkey: "Software\Classes\gfc"; ValueType: string; ValueName: ""; ValueData: "URL:GFC link"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\gfc"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\gfc\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"",0"
Root: HKA; Subkey: "Software\Classes\gfc\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" -GFCLink=""%1"""

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Microsoft Visual C++ runtime..."; Flags: shellexec waituntilterminated skipifdoesntexist; Verb: "runas"; Check: NeedsVCRuntime
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Settings and the cached feed live in %LOCALAPPDATA%\GFCClient; the signed-in session and wallet backups
; are there too, so they're left alone on uninstall.
Type: filesandordirs; Name: "{app}\GFCClient\Saved"

[Code]
function NeedsVCRuntime: Boolean;
var
  Installed: Cardinal;
begin
  Result := not (RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and (Installed = 1));
end;
