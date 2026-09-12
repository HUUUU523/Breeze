; Breeze 安装程序脚本（Inno Setup 6）
#define MyAppName "Breeze"
#define MyAppVersion "2.0.0"
#define MyAppPublisher "HUUUU523"
#define MyAppURL "https://github.com/HUUUU523/Breeze"
#define MyAppExeName "Breeze.exe"

[Setup]
AppId={{8F3A2B1C-4D5E-6F70-8A9B-0C1D2E3F4A5B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=Breeze-{#MyAppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "chinese"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; 只打包运行必需文件，排除构建中间产物
Source: "build\Breeze.exe";           DestDir: "{app}"; Flags: ignoreversion
Source: "build\Breeze.exe.manifest";  DestDir: "{app}"; Flags: ignoreversion
Source: "build\*.dll";                DestDir: "{app}"; Flags: ignoreversion
Source: "build\QtWebEngineProcess.exe"; DestDir: "{app}"; Flags: ignoreversion
; 资源与插件目录
Source: "build\resources\*";     DestDir: "{app}\resources";     Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\platforms\*";     DestDir: "{app}\platforms";     Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\styles\*";        DestDir: "{app}\styles";        Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\tls\*";           DestDir: "{app}\tls";           Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\imageformats\*";  DestDir: "{app}\imageformats";  Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\generic\*";       DestDir: "{app}\generic";       Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\position\*";      DestDir: "{app}\position";      Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\translations\*";  DestDir: "{app}\translations";  Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\qml\*";           DestDir: "{app}\qml";           Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\qmltooling\*";    DestDir: "{app}\qmltooling";    Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
