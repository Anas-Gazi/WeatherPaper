#define MyAppName "WeatherPaper"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "WeatherPaper Contributors"
#define MyAppURL "https://github.com/Anas-Gazi/WeatherPaper"

[Setup]
AppId={{E5813A54-8B8D-4F29-B10E-6F39A4D9B042}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\WeatherPaper
DefaultGroupName=WeatherPaper
UninstallDisplayIcon={app}\WeatherPaper.exe
OutputDir=..\..\dist
OutputBaseFilename=WeatherPaper-Setup-v{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: "autostart"; Description: "Start WeatherPaper automatically when I log in"; GroupDescription: "Startup options:"; Flags: unchecked

[Files]
Source: "..\..\dist\WeatherPaper-Windows-Portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\WeatherPaper"; Filename: "{app}\WeatherPaper.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\WeatherPaper"; Filename: "{app}\WeatherPaper.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "WeatherPaper"; ValueData: """{app}\WeatherPaper.exe"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\WeatherPaper.exe"; Description: "Launch WeatherPaper"; Flags: nowait postinstall skipifsilent
