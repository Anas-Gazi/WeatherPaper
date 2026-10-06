; Inno Setup Script for WeatherPaper on Windows 10 and Windows 11
; Compiles into WeatherPaper-Setup.exe

#define MyAppName "WeatherPaper"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "WeatherPaper Contributors"
#define MyAppURL "https://github.com/Anas-Gazi/WeatherPaper"
#define MyAppExeName "weatherpaperd.exe"

[Setup]
AppId={{E5813A54-8B8D-4F29-B10E-6F39A4D9B042}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\..\dist
OutputBaseFilename=WeatherPaper-Setup-v{#MyAppVersion}
SetupIconFile=..\..\assets\icons\weatherpaper-icon-256.png
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start WeatherPaper automatically when Windows starts"; GroupDescription: "Startup options:"

[Files]
; Main Executable
Source: "..\..\build\src\app\weatherpaperd.exe"; DestDir: "{app}"; Flags: ignoreversion
; Optional Qt and MinGW / MSVC runtime DLLs (if bundled)
Source: "..\..\build\src\app\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
; Bundled default theme assets
Source: "..\..\assets\default_theme\*"; DestDir: "{app}\assets\default_theme"; Flags: ignoreversion recursesubdirs createallsubdirs
; Icons
Source: "..\..\assets\icons\*"; DestDir: "{app}\assets\icons"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\assets\icons\weatherpaper-icon-256.png"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\assets\icons\weatherpaper-icon-256.png"

[Registry]
; Autostart with Windows task
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "WeatherPaper"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
