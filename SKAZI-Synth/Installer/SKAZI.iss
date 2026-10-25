; Inno Setup script for SKAZI (VST3 + standalone), 64-bit Windows
#define AppName "SKAZI"
#define AppVersion "1.3.0"
#define AppPublisher "Asher Swissa"

[Setup]
AppId={{3F9B7C21-5D4E-4A8B-B1C6-5CA21F0E0001}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion} by {#AppPublisher}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\dist
OutputBaseFilename=SKAZI_Setup_v{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName} by {#AppPublisher}

[Types]
Name: "full"; Description: "VST3 plugin + standalone app"
Name: "vst3"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (Common Files\VST3)"; Types: full vst3 custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "..\dist\SKAZI.vst3\*"; DestDir: "{commoncf64}\VST3\SKAZI.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\SKAZI.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\SKAZI.exe"; Components: standalone
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\SKAZI.exe"; Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; Components: standalone; Flags: unchecked

[Run]
Filename: "{app}\SKAZI.exe"; Description: "Launch SKAZI standalone"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked
