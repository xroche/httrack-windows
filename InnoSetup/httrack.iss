; WinHTTrack Website Copier -- installer
;
; Everything comes from the command line, so nothing is tied to one machine and CI
; can build this from the very artifact it already publishes:
;
;   ISCC /DArch=x64 /DAppVersion=3.49-12 ^
;        /DPayloadDir=...\WinHTTrack\bin\x64\Release ^
;        /DEngineDir=...\httrack /DGuiDir=...\httrack-windows ^
;        /DRedistFile=...\vc_redist.x64.exe /DOutDir=...\out ^
;        InnoSetup\httrack.iss
;
; Signing is opt-in (/DSign) and off by default so unsigned CI builds work. The old
; script hardcoded a .pfx on a personal drive and a Verisign timestamp URL that has
; been dead for years; when signing returns it needs an RFC3161 one.
;
; Replaces the old httrack.iss + httrack-x64.iss pair. They had drifted apart, and
; both referenced files that no longer exist (readme, copying, file_id.diz, src_win).

#ifndef Arch
  #error Arch must be set (x64 or x86)
#endif
#ifndef AppVersionNumeric
  #error AppVersionNumeric must be set (dotted, e.g. 3.49.12)
#endif
#ifndef AppVersion
  #error AppVersion must be set
#endif

[Setup]
AppName=WinHTTrack Website Copier
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
; Windows 7 SP1 is the floor on purpose: HTTrack is still used on very old machines.
; It is also why the bundled runtime must stay on the 14.4x (VS2022) line -- the
; 14.5x redistributable refuses to install here.
MinVersion=6.1sp1
PrivilegesRequired=admin
OutputBaseFilename=httrack_{#Arch}_{#AppVersion}
OutputDir={#OutDir}
SetupIconFile={#GuiDir}\WinHTTrack\res\Shell.ico
UninstallIconFile={#GuiDir}\WinHTTrack\res\Shell.ico
#if Arch == "x64"
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
#endif
#ifdef Sign
SignedUninstaller=yes
#else
SignedUninstaller=no
#endif

[Tasks]
Name: "regfiles"; Description: "Associate the .whtt project file type (the only extension this affects) with WinHTTrack, and add ""New > WinHTTrack Project"" to the Explorer menu"; GroupDescription: "Setup:"
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"
Name: "quicklaunchicon"; Description: "Create a &quick launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
; The program: exe, libhttrack.dll, the OpenSSL/zlib DLLs it imports, and lang/ and
; templates/. This is the same staged directory CI publishes, so what gets tested is
; what gets shipped. The PDBs ship too: without them a crash report names no function and
; no line, and we already ship the sources they point into.
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Excludes: "*.iobj,*.ipdb,*.exp,*.lib,README-artifact.txt"; Flags: recursesubdirs ignoreversion

; Documentation that actually exists. The old script also listed readme, copying and
; file_id.diz, none of which are in the tree any more.
Source: "{#EngineDir}\httrack-doc.html"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\history.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\license.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\greetings.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\COPYING"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\AUTHORS"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#EngineDir}\gpl-fr.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Sources, kept: HTTrack is GPL and shipping them is how that offer is met. The
; excludes are load-bearing -- CI builds inside these trees, so without them the
; installer would carry object files, import libs and vcpkg_installed as well.
Source: "{#EngineDir}\src\*"; DestDir: "{app}\src"; Excludes: "*.obj,*.pdb,*.lib,*.exp,*.dll,*.exe,*.iobj,*.ipdb,*.tlog,\x64\*,\Win32\*,\vcpkg_installed\*,\.vs\*"; Flags: recursesubdirs ignoreversion
Source: "{#GuiDir}\WinHTTrack\*"; DestDir: "{app}\src_win"; Excludes: "*.obj,*.pdb,*.lib,*.exp,*.dll,*.exe,*.iobj,*.ipdb,*.tlog,\bin\*,\x64\*,\Win32\*,\vcpkg_installed\*,\.vs\*"; Flags: recursesubdirs ignoreversion

; The Visual C++ runtime, bundled rather than assumed. The app links the dynamic CRT
; and shared MFC, so on a machine without it the exe simply refuses to start, with a
; message that tells the user nothing useful.
Source: "{#RedistFile}"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Run]
Filename: "{tmp}\{#ExtractFileName(RedistFile)}"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Visual C++ runtime..."; Flags: waituntilterminated
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
