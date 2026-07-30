; WinHTTrack Website Copier -- installer
;
; Everything comes from the command line, so nothing is tied to one machine and CI
; can build this from the very artifact it already publishes:
;
;   ISCC /DArch=x64 /DAppVersion=3.50-beta-1 ^
;        /DPayloadDir=...\WinHTTrack\bin\x64\Release ^
;        /DEngineDir=...\httrack /DGuiDir=...\httrack-windows ^
;        /DOutDir=...\out ^
;        InnoSetup\httrack.iss
;
; Signing is opt-in (/DSign) and off by default, so an ordinary CI build needs no
; certificate. With it, ISCC signs setup.exe and the embedded uninstaller through the
; sign tool the caller passes as /Scertum=signtool.exe sign ... $f.
;
; Replaces the old httrack.iss + httrack-x64.iss pair. They had drifted apart, and
; both referenced files that no longer exist (readme, copying, file_id.diz).

#ifndef Arch
  #error Arch must be set (x64 or x86)
#endif
#ifndef AppVersionNumeric
  #error AppVersionNumeric must be set (dotted, e.g. 3.49.99.1)
#endif
#ifndef AppVersion
  #error AppVersion must be set
#endif

[Setup]
AppName=WinHTTrack Website Copier
; The GUI carries its own version (WinHTTrack/version.h); the engine is on its own.
AppVersion={#AppVersion}
AppVerName=WinHTTrack Website Copier {#AppVersion}
VersionInfoProductName=HTTrack Website Copier
VersionInfoVersion={#AppVersionNumeric}
VersionInfoProductVersion={#AppVersionNumeric}
VersionInfoProductTextVersion={#AppVersion}
VersionInfoTextVersion={#AppVersion}
AppPublisher=HTTrack
AppPublisherURL=https://www.httrack.com
AppSupportURL=https://forum.httrack.com
AppUpdatesURL=https://www.httrack.com/page/2/
DefaultDirName={autopf}\WinHTTrack
DefaultGroupName=WinHTTrack
AllowNoIcons=yes
LicenseFile={#GuiDir}\setup_license.txt
AppMutex=WinHTTrack_RUN
; Solid is worth ~0.5 MB: per-file streams cannot dedupe the 41 near-identical
; api-ms-win-crt stubs. The dictionary stays put -- 8 MiB is within 9 KB of 64 MiB at
; this size, and lzma2 allocates it to DECOMPRESS, on the machines that are our floor.
Compression=lzma2/max
SolidCompression=yes
; Windows 7 SP1 is the floor on purpose: HTTrack is still used on very old machines.
; It is also why the app-local runtime must stay on the 14.4x (VS2022) line: 14.5x
; dropped Windows 7.
MinVersion=6.1sp1
PrivilegesRequired=admin
OutputBaseFilename=httrack_{#Arch}_{#AppVersion}
OutputDir={#OutDir}
SetupIconFile={#GuiDir}\WinHTTrack\res\Shell.ico
UninstallIconFile={#GuiDir}\WinHTTrack\res\Shell.ico
; Add/Remove Programs reads DisplayIcon, which only this directive writes.
UninstallDisplayIcon={app}\WinHTTrack.exe
#if Arch == "x64"
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
#endif
#ifdef Sign
SignedUninstaller=yes
SignTool=certum
#else
SignedUninstaller=no
#endif

[Tasks]
Name: "regfiles"; Description: "Associate the .whtt project file type (the only extension this affects) with WinHTTrack, and add ""New > WinHTTrack Project"" to the Explorer menu"; GroupDescription: "Setup:"
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"
Name: "quicklaunchicon"; Description: "Create a &quick launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
; The program: exe, libhttrack.dll, the OpenSSL/zlib and VC++ runtime DLLs it imports,
; and lang/ and templates/. This is the same staged directory CI publishes, so what gets tested is
; what gets shipped. PDBs are built but not shipped, so a user crash report names no
; line; source.txt carries the GPL offer the src/ tree used to meet.
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Excludes: "*.pdb,*.iobj,*.ipdb,*.exp,*.lib,README-artifact.txt,runtime-vendored.txt"; Flags: recursesubdirs ignoreversion

; Documentation that actually exists. The old script also listed readme, copying and
; file_id.diz, none of which are in the tree any more.
Source: "{#EngineDir}\httrack-doc.html"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\history.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\license.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\greetings.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\COPYING"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\AUTHORS"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\gpl-fr.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[InstallDelete]
; Left by installers up to 3.49-14, which shipped both source trees and the PDBs.
Type: filesandordirs; Name: "{app}\src"
Type: filesandordirs; Name: "{app}\src_win"
Type: files; Name: "{app}\*.pdb"

[Run]
Filename: "{app}\WinHTTrack.exe"; Description: "Launch WinHTTrack Website Copier"; Flags: nowait postinstall skipifsilent
Filename: "{win}\notepad.exe"; Parameters: "{app}\history.txt"; WorkingDir: "{app}"; Description: "View history.txt file"; Flags: nowait postinstall skipifsilent

[Icons]
Name: "{group}\WinHTTrack Website Copier"; Filename: "{app}\WinHTTrack.exe"; Comment: "Launch WinHTTrack Website Copier"; WorkingDir: "{app}"
Name: "{group}\Documentation"; Filename: "{app}\httrack-doc.html"; Comment: "View documentation"; WorkingDir: "{app}"
Name: "{group}\history.txt"; Filename: "{win}\notepad.exe"; Parameters: "{app}\history.txt"; Comment: "history.txt"; WorkingDir: "{app}"
Name: "{group}\license.txt"; Filename: "{win}\notepad.exe"; Parameters: "{app}\license.txt"; Comment: "license.txt"; WorkingDir: "{app}"
Name: "{autodesktop}\HTTrack Website Copier"; Filename: "{app}\WinHTTrack.exe"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\HTTrack Website Copier"; Filename: "{app}\WinHTTrack.exe"; Tasks: quicklaunchicon

[Registry]
Root: HKCU; Subkey: "Software\WinHTTrack Website Copier"; Flags: uninsdeletekeyifempty noerror
Root: HKCU; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier"; Flags: uninsdeletekey noerror
Root: HKCU; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier\Interface"; ValueType: dword; ValueName: "SetupRun"; ValueData: 1; Flags: uninsdeletekey noerror
Root: HKCU; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier\Interface"; ValueType: dword; ValueName: "SetupHasRegistered"; ValueData: 1; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKLM; Subkey: "Software\WinHTTrack Website Copier"; Flags: uninsdeletekeyifempty noerror
Root: HKLM; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier"; Flags: uninsdeletekey noerror
Root: HKLM; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Flags: uninsdeletekey noerror
Root: HKCR; Subkey: ".whtt\ShellNew"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCR; Subkey: ".whtt"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCR; Subkey: "WinHTTrackProject"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKLM; Subkey: "Software\Classes\WinHTTrackProject"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCR; Subkey: "Applications\WinHTTrack.exe"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\WinHTTrack.exe"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCU; Subkey: "AppEvents\Schemes\Apps\WinHTTrack"; ValueType: string; ValueData: "WinHTTrack Website Copier"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCU; Subkey: "AppEvents\EventLabels\MirrorFinished"; ValueType: string; ValueData: "Mirror Finished"; Flags: uninsdeletekey noerror; Tasks: regfiles
