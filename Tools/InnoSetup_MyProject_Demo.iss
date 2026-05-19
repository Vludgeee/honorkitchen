; Inno Setup 6 — установщик демо Win64 для куратора / колледжа.
; Инструкция: Docs/INSTALLER_BUILD.md
; Сборка из командной строки (рекомендуется):  Tools\BuildInstaller.ps1
; Скачать Inno Setup: https://jrsoftware.org/isdl.php
;
; GameSourceDir:
;   - через /D при вызове ISCC (BuildInstaller.ps1) — надёжно;
;   - при сборке ТОЛЬКО из GUI Inno без /D — ниже путь по умолчанию ОТНОСИТЕЛЬНО этого .iss (папка Tools\).

#ifndef GameSourceDir
#define GameSourceDir "..\Build\Packaged\WindowsNoEditor\*"
#endif

#define MyAppName "HonorKitchen Demo"
#define MyAppVersion "0.2.2"
#define MyAppPublisher "(ФИО / группа — подставь)"
#define MyAppExeName "MyProject.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
UsePreviousAppDir=no
DefaultDirName={localappdata}\Programs\HonorKitchenDemo\v{#MyAppVersion}
DefaultGroupName={#MyAppName} {#MyAppVersion}
; Выход установщика: папка Dist в корне проекта (относительно этого .iss)
OutputDir=..\Dist
OutputBaseFilename=HonorKitchen_Demo_Setup_{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#GameSourceDir}"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\Docs\README_DEMO.txt"; DestDir: "{app}"; DestName: "README_DEMO.txt"; Flags: ignoreversion isreadme
Source: "installer_package_stamp.txt"; DestDir: "{app}"; DestName: "BUILD_FROM_INSTALLER.txt"; Flags: ignoreversion
; Положи VC_redist.x64.exe в Tools\redist\ и раскомментируй при раздаче куратору:
; Source: "redist\VC_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{autoprograms}\{#MyAppName} {#MyAppVersion}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName} {#MyAppVersion}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
; Тихая установка VC++ (часто нужны права admin — смени PrivilegesRequired=admin):
; Filename: "{tmp}\VC_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing VC++ Runtime..."; Flags: waituntilterminated

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
