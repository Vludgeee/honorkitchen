; Installer for packaged Shipping build.
;
; UE 5.3 Editor "Package Project" often outputs:  Build\Packaged\Windows\MyProject.exe
; UAT BuildCookRun often outputs:                 Build\Packaged\WindowsNoEditor\...
;
; Pick folder automatically; if you use a custom path, set PackagedGameDir manually below.

#if FileExists("..\Build\Packaged\Windows\MyProject.exe")
#define PackagedGameDir "..\Build\Packaged\Windows"
#elif FileExists("..\Build\Packaged\WindowsNoEditor\MyProject.exe")
#define PackagedGameDir "..\Build\Packaged\WindowsNoEditor"
#else
#error Package the game first. Expected MyProject.exe in Build\Packaged\Windows OR Build\Packaged\WindowsNoEditor (UE: File / Package Project / Windows / Shipping).
#endif

#define MyAppName "Honor Kitchen"
#define MyAppExeName "MyProject.exe"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "HonorKitchen"

[Setup]
AppId={{B7E2F1A4-9C3D-5E6F-8012-3456789ABCDE}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=..\Build\Installer
OutputBaseFilename=HonorKitchen_Setup
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
DisableProgramGroupPage=no
MinVersion=10.0

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
Source: "{#PackagedGameDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
