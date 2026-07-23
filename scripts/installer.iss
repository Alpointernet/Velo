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
ChangesAssociations=yes

[Files]
Source: "Velo.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Scintilla.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Lexilla.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "fonts\*"; DestDir: "{app}\fonts"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "icon\papirus\*"; DestDir: "{app}\icon\papirus"; Flags: ignoreversion

[Icons]
Name: "{group}\Velo"; Filename: "{app}\Velo.exe"
Name: "{autodesktop}\Velo"; Filename: "{app}\Velo.exe"

[Registry]
; Clean up previous wildcard context menu if present
Root: HKCR; Subkey: "*\shell\Edit with Velo"; Flags: deletekey dontcreatekey

[Run]
Filename: "{app}\Velo.exe"; Description: "Launch Velo"; Flags: nowait postinstall skipifsilent

[Code]
procedure SHChangeNotify(wEventID: Longint; uFlags: Cardinal; dwItem1, dwItem2: Longint);
  external 'SHChangeNotify@shell32.dll stdcall';

procedure RegisterProgID();
var
  AppPath: string;
begin
  AppPath := ExpandConstant('{app}\Velo.exe');
  RegWriteStringValue(HKEY_CLASSES_ROOT, 'Applications\Velo.exe\shell\open\command', '', '"' + AppPath + '" "%1"');
  RegWriteStringValue(HKEY_CLASSES_ROOT, 'Applications\Velo.exe', 'FriendlyAppName', 'Velo');
end;

procedure UnregisterProgID();
begin
  RegDeleteKeyIncludingSubkeys(HKEY_CLASSES_ROOT, 'Velo.Document');
  RegDeleteKeyIncludingSubkeys(HKEY_CLASSES_ROOT, 'Applications\Velo.exe');
end;

procedure RegisterExtension(Ext: string; IsExecutable: Boolean);
var
  KeyPath: string;
  ProgID: string;
  IconPath: string;
begin
  // Always add "Edit with Velo" context menu
  KeyPath := 'SystemFileAssociations\' + Ext + '\shell\Edit with Velo';
  RegWriteStringValue(HKEY_CLASSES_ROOT, KeyPath, '', 'Edit with Velo');
  RegWriteStringValue(HKEY_CLASSES_ROOT, KeyPath, 'Icon', ExpandConstant('{app}\Velo.exe'));
  RegWriteStringValue(HKEY_CLASSES_ROOT, KeyPath + '\command', '', '"' + ExpandConstant('{app}\Velo.exe') + '" "%1"');

  if IsExecutable then
  begin
    // For executables, NEVER take over the default action or icon, 
    // as it breaks double-clicking to run them (e.g. .bat).
    
    // Unconditionally restore default executable mappings 
    // in case a previous version of Velo mistakenly broke it.
    if Ext = '.bat' then
      RegWriteStringValue(HKEY_CLASSES_ROOT, Ext, '', 'batfile')
    else if Ext = '.cmd' then
      RegWriteStringValue(HKEY_CLASSES_ROOT, Ext, '', 'cmdfile')
    else if Ext = '.ps1' then
      RegWriteStringValue(HKEY_CLASSES_ROOT, Ext, '', 'Microsoft.PowerShellScript.1')
    else if Ext = '.sh' then
      RegDeleteValue(HKEY_CLASSES_ROOT, Ext, ''); // No system default for .sh

    // Also remove any stray Velo ProgID from OpenWithProgids
    RegDeleteValue(HKEY_CLASSES_ROOT, Ext + '\OpenWithProgids', 'Velo' + Ext);
    Exit;
  end;

  ProgID := 'Velo' + Ext;
  RegWriteStringValue(HKEY_CLASSES_ROOT, ProgID, '', 'Velo ' + UpperCase(Copy(Ext, 2, Length(Ext) - 1)) + ' Document');
  RegWriteStringValue(HKEY_CLASSES_ROOT, ProgID + '\shell\open\command', '', '"' + ExpandConstant('{app}\Velo.exe') + '" "%1"');

  if (Ext = '.txt') or (Ext = '.log') then
    IconPath := ExpandConstant('{app}\icon\papirus\txt.ico')
  else if (Ext = '.md') or (Ext = '.markdown') then
    IconPath := ExpandConstant('{app}\icon\papirus\md.ico')
  else if (Ext = '.json') or (Ext = '.jsonc') then
    IconPath := ExpandConstant('{app}\icon\papirus\json.ico')
  else if (Ext = '.ini') or (Ext = '.conf') or (Ext = '.env') then
    IconPath := ExpandConstant('{app}\icon\papirus\ini.ico')
  else if (Ext = '.cfg') or (Ext = '.config') then
    IconPath := ExpandConstant('{app}\icon\papirus\cfg.ico')
  else if (Ext = '.toml') then
    IconPath := ExpandConstant('{app}\icon\papirus\toml.ico')
  else if (Ext = '.yaml') then
    IconPath := ExpandConstant('{app}\icon\papirus\yaml.ico')
  else if (Ext = '.yml') then
    IconPath := ExpandConstant('{app}\icon\papirus\yml.ico')
  else if (Ext = '.go') then
    IconPath := ExpandConstant('{app}\icon\papirus\go.ico')
  else if (Ext = '.rs') then
    IconPath := ExpandConstant('{app}\icon\papirus\rs.ico')
  else if (Ext = '.py') then
    IconPath := ExpandConstant('{app}\icon\papirus\py.ico')
  else if (Ext = '.rb') then
    IconPath := ExpandConstant('{app}\icon\papirus\rb.ico')
  else if (Ext = '.java') then
    IconPath := ExpandConstant('{app}\icon\papirus\java.ico')
  else if (Ext = '.c') then
    IconPath := ExpandConstant('{app}\icon\papirus\c.ico')
  else if (Ext = '.cpp') or (Ext = '.cc') or (Ext = '.cs') or (Ext = '.ahk') or (Ext = '.rc') then
    IconPath := ExpandConstant('{app}\icon\papirus\cpp.ico')
  else if (Ext = '.hpp') then
    IconPath := ExpandConstant('{app}\icon\papirus\hpp.ico')
  else if (Ext = '.h') then
    IconPath := ExpandConstant('{app}\icon\papirus\h.ico')
  else if (Ext = '.html') or (Ext = '.htm') then
    IconPath := ExpandConstant('{app}\icon\papirus\html.ico')
  else if (Ext = '.js') or (Ext = '.jsx') or (Ext = '.ts') or (Ext = '.tsx') then
    IconPath := ExpandConstant('{app}\icon\papirus\js.ico')
  else if (Ext = '.css') then
    IconPath := ExpandConstant('{app}\icon\papirus\css.ico')
  else if (Ext = '.scss') then
    IconPath := ExpandConstant('{app}\icon\papirus\scss.ico')
  else if (Ext = '.sql') then
    IconPath := ExpandConstant('{app}\icon\papirus\sql.ico')
  else if (Ext = '.xml') then
    IconPath := ExpandConstant('{app}\icon\papirus\xml.ico')
  else
    IconPath := ExpandConstant('{app}\icon\papirus\_page.ico');

  RegWriteStringValue(HKEY_CLASSES_ROOT, ProgID + '\DefaultIcon', '', IconPath);

  RegDeleteValue(HKEY_CLASSES_ROOT, Ext + '\OpenWithProgids', 'Velo.Document');
  RegWriteStringValue(HKEY_CLASSES_ROOT, Ext + '\OpenWithProgids', ProgID, '');
  RegWriteStringValue(HKEY_CLASSES_ROOT, Ext, '', ProgID);
end;

procedure UnregisterExtension(Ext: string; IsExecutable: Boolean);
var
  KeyPath: string;
  ProgID: string;
begin
  KeyPath := 'SystemFileAssociations\' + Ext + '\shell\Edit with Velo';
  RegDeleteKeyIncludingSubkeys(HKEY_CLASSES_ROOT, KeyPath);

  ProgID := 'Velo' + Ext;
  RegDeleteKeyIncludingSubkeys(HKEY_CLASSES_ROOT, ProgID);
  RegDeleteValue(HKEY_CLASSES_ROOT, Ext + '\OpenWithProgids', ProgID);
end;

procedure CleanUserChoice(Ext: string);
var
  KeyPath: string;
  Val: string;
begin
  KeyPath := 'Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\' + Ext + '\UserChoice';
  if RegQueryStringValue(HKEY_CURRENT_USER, KeyPath, 'ProgId', Val) then
  begin
    if Val = 'Velo.Document' then
    begin
      RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, KeyPath);
    end;
  end;
end;

procedure ProcessExtensions(Register: Boolean);
var
  DocExts: string;
  ExecExts: string;
  Ext: string;
  CommaPos: Integer;
begin
  DocExts := '.txt,.log,.md,.markdown,.json,.jsonc,.xml,.html,.htm,.css,.scss,.ini,.cfg,.conf,.config,.env,.yaml,.yml,.toml,.c,.cpp,.cc,.h,.hpp,.cs,.java,.js,.jsx,.ts,.tsx,.py,.rb,.go,.rs,.sql,.ahk,.rc,.iss';
  while Length(DocExts) > 0 do
  begin
    CommaPos := Pos(',', DocExts);
    if CommaPos > 0 then
    begin
      Ext := Copy(DocExts, 1, CommaPos - 1);
      Delete(DocExts, 1, CommaPos);
    end
    else
    begin
      Ext := DocExts;
      DocExts := '';
    end;
    Ext := Trim(Ext);
    if Ext <> '' then
    begin
      if Register then
      begin
        CleanUserChoice(Ext);
        RegisterExtension(Ext, False);
      end
      else
        UnregisterExtension(Ext, False);
    end;
  end;

  ExecExts := '.bat,.cmd,.ps1,.sh';
  while Length(ExecExts) > 0 do
  begin
    CommaPos := Pos(',', ExecExts);
    if CommaPos > 0 then
    begin
      Ext := Copy(ExecExts, 1, CommaPos - 1);
      Delete(ExecExts, 1, CommaPos);
    end
    else
    begin
      Ext := ExecExts;
      ExecExts := '';
    end;
    Ext := Trim(Ext);
    if Ext <> '' then
    begin
      if Register then
      begin
        RegisterExtension(Ext, True);
      end
      else
        UnregisterExtension(Ext, True);
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    RegDeleteKeyIncludingSubkeys(HKEY_CLASSES_ROOT, 'Velo.Document');
    RegisterProgID();
    ProcessExtensions(True);
    SHChangeNotify($08000000, 0, 0, 0); // SHCNE_ASSOCCHANGED, SHCNF_IDLIST
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    ProcessExtensions(False);
    UnregisterProgID();
    SHChangeNotify($08000000, 0, 0, 0); // SHCNE_ASSOCCHANGED, SHCNF_IDLIST
  end;
end;
