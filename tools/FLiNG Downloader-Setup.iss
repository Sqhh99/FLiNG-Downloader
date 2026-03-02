; FLiNG Downloader installer script

#define MyAppName "FLiNG Downloader"
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define MyAppPublisher "Sqhh99"
#define MyAppURL "https://github.com/Sqhh99/FLiNG-Downloader"
#define MyAppExeName "FLiNG Downloader.exe"
#define MyAppAssocName MyAppName + " File"
#define MyAppAssocExt ".myp"
#define MyAppAssocKey StringChange(MyAppAssocName, " ", "") + MyAppAssocExt

#ifndef SourceRoot
  #define SourceRoot ".."
#endif
#ifndef DistDir
  #define DistDir "..\\dist\\FLiNG Downloader"
#endif
#ifndef OutputDir
  #define OutputDir "..\\dist\\installer"
#endif

[Setup]
AppId={{900BACD9-18E2-4A87-A3CE-3AB25B5A226D}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes
DisableProgramGroupPage=yes
LicenseFile={#SourceRoot}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=FLiNG-Downloader-v{#AppVersion}-win-x64-setup
SetupIconFile={#SourceRoot}\resources\icons\app_icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
Root: HKA; Subkey: "Software\Classes\{#MyAppAssocExt}\OpenWithProgids"; ValueType: string; ValueName: "{#MyAppAssocKey}"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\{#MyAppAssocKey}"; ValueType: string; ValueName: ""; ValueData: "{#MyAppAssocName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#MyAppAssocKey}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKA; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
var
  RemoveUserDataOnUninstall: Boolean;

function ConfirmRemoveUserData(): Boolean;
begin
  if UninstallSilent then
  begin
    Result := False;
    exit;
  end;

  Result := MsgBox(
    'Do you also want to remove all FLiNG Downloader user data?' + #13#10 +
    'This includes settings, cache, and search history in AppData.' + #13#10 +
    'Downloaded trainer files will NOT be deleted.',
    mbConfirmation, MB_YESNO) = IDYES;
end;

procedure RemoveRegistryKeys();
begin
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\FLiNG Downloader');
  RegDeleteKeyIncludingSubkeys(HKLM, 'Software\FLiNG Downloader');
end;

procedure RemoveUserDataDirectories();
begin
  DelTree(ExpandConstant('{userappdata}\FLiNG Downloader'), True, True, True);
  DelTree(ExpandConstant('{localappdata}\FLiNG Downloader'), True, True, True);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    RemoveUserDataOnUninstall := ConfirmRemoveUserData();
  end
  else if (CurUninstallStep = usPostUninstall) and RemoveUserDataOnUninstall then
  begin
    RemoveUserDataDirectories();
    RemoveRegistryKeys();
  end;
end;
