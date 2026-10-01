; Inno Setup script for wineyes.
;
; Build (after build.bat):  iscc /DAppVersion=1.2.3 wineyes.iss
; Output goes to dist\wineyes-<version>-setup.exe

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif

[Setup]
AppId={{8F048849-A0F3-4A08-BCEB-5B5DA443A41C}
AppName=wineyes
AppVersion={#AppVersion}
AppVerName=wineyes {#AppVersion}
AppPublisher=Graham Ollis
AppPublisherURL=https://github.com/plicease/win32-wineyes
AppSupportURL=https://github.com/plicease/win32-wineyes/issues
DefaultDirName={autopf}\wineyes
; Start menu entry only; no program group page and nothing on the desktop.
DisableProgramGroupPage=yes
DisableDirPage=auto
LicenseFile=LICENSE
UninstallDisplayIcon={app}\wineyes.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Installs per-user without elevation by default; the user can choose an
; all-users install from the dialog.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=dist
OutputBaseFilename=wineyes-{#AppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "wineyes.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\wineyes"; Filename: "{app}\wineyes.exe"

[Run]
Filename: "{app}\wineyes.exe"; Description: "Launch wineyes"; Flags: nowait postinstall skipifsilent
