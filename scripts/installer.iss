[Setup]
SourceDir=..
AppName=Velo
AppVersion=1.0
DefaultDirName={autopf}\Velo
DefaultGroupName=Velo
UninstallDisplayIcon={app}\Velo.exe
Compression=lzma2
SolidCompression=yes
OutputDir=output
OutputBaseFilename=VeloInstaller
WizardStyle=modern
SetupIconFile=icon\app.ico
DisableDirPage=no
DisableProgramGroupPage=yes

[Files]
Source: "Velo.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Scintilla.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Lexilla.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "fonts\*"; DestDir: "{app}\fonts"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Velo"; Filename: "{app}\Velo.exe"
Name: "{autodesktop}\Velo"; Filename: "{app}\Velo.exe"

[Run]
Filename: "{app}\Velo.exe"; Description: "Launch Velo"; Flags: nowait postinstall skipifsilent
