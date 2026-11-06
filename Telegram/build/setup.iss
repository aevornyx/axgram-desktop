#define MyAppShortName "AxGram"
#define MyAppName "AxGram Desktop"
#define MyAppPublisher "AxGram"
#define MyAppURL "https://github.com/aevornyx/axgram-desktop"
#define MyAppExeName "AxGram.exe"
#define MyAppId "12FFD268-D3BF-43EB-8CC0-4CA437920205"
#ifdef DebugBuild
  #undef MyAppShortName
  #undef MyAppName
  #undef MyAppId
  #define MyAppShortName "AxGram Debug"
  #define MyAppName "AxGram Desktop Debug"
  #define MyAppId "9F30F3DD-765A-4BCB-B813-CE6E6585912E"
#endif
#define CurrentYear GetDateTimeString('yyyy','','')

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{{#MyAppId}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppCopyright={#MyAppPublisher} {#CurrentYear}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={userappdata}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir={#ReleasePath}
SetupIconFile={#SourcePath}..\Resources\art\icon256.ico
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma
SolidCompression=yes
DisableStartupPrompt=yes
PrivilegesRequired=lowest
VersionInfoVersion={#MyAppVersion}.0
CloseApplications=yes
RestartApplications=no
DisableDirPage=no
DisableProgramGroupPage=no
WizardStyle=modern
#ifndef UnsignedBuild
SignTool=sha256
#endif

#if MyBuildTarget == "winarm"
  ArchitecturesAllowed="arm64"
  OutputBaseFilename=tsetup-arm64.{#MyAppVersionFull}
  #define ArchModulesFolder "arm64"
  AppVerName={#MyAppName} {#MyAppVersion} arm64
#elif MyBuildTarget == "win64"
  ArchitecturesAllowed="x64compatible"
  ArchitecturesInstallIn64BitMode="x64compatible"
  OutputBaseFilename=tsetup-x64.{#MyAppVersionFull}
  #define ArchModulesFolder "x64"
  AppVerName={#MyAppName} {#MyAppVersion} 64bit
#else
  OutputBaseFilename=tsetup.{#MyAppVersionFull}
  #define ArchModulesFolder "x86"
  AppVerName={#MyAppName} {#MyAppVersion} 32bit
#endif

#ifdef SetupFileName
  OutputBaseFilename={#SetupFileName}
#endif

#define ModulesFolder "modules\" + ArchModulesFolder

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "it";      MessagesFile: "compiler:Languages\Italian.isl"
Name: "es";      MessagesFile: "compiler:Languages\Spanish.isl"
Name: "de";      MessagesFile: "compiler:Languages\German.isl"
Name: "nl";      MessagesFile: "compiler:Languages\Dutch.isl"
Name: "pt_BR";   MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "ru";      MessagesFile: "compiler:Languages\Russian.isl"
Name: "fr";      MessagesFile: "compiler:Languages\French.isl"
Name: "ua";      MessagesFile: "compiler:Languages\Ukrainian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\..\LICENSE"; DestDir: "{app}\licenses"; DestName: "GPL-3.0.txt"; Flags: ignoreversion
Source: "..\..\NOTICE.md"; DestDir: "{app}\licenses"; DestName: "AxGram-NOTICE.md"; Flags: ignoreversion
Source: "..\SourceFiles\ax\libs\sqlite\SQLCIPHER-LICENSE.md"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "{#ReleasePath}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
#ifndef WithoutUpdater
Source: "{#ReleasePath}\Updater.exe"; DestDir: "{app}"; Flags: ignoreversion
#endif
#if MyBuildTarget != "winarm" && (!Defined(DebugBuild) || FileExists(ReleasePath + "\" + ModulesFolder + "\d3d\d3dcompiler_47.dll"))
Source: "{#ReleasePath}\{#ModulesFolder}\d3d\d3dcompiler_47.dll"; DestDir: "{app}\{#ModulesFolder}\d3d"; Flags: ignoreversion
#endif
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

#ifdef DebugBuild
[Dirs]
Name: "{app}\TelegramForcePortable"; Flags: uninsneveruninstall
#endif

[Icons]
Name: "{group}\{#MyAppShortName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppShortName}}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#MyAppShortName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppShortName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
RemoveProfile=Also delete AxGram accounts, settings, message archive and cached media?%n%nChoose No to keep your data for reinstallation.
CloseAppFailed=AxGram files are still in use. Close this copy of AxGram and run the uninstaller again. No files have been removed.
ru.CloseAppFailed=Файлы AxGram всё ещё заняты. Закройте эту копию AxGram и снова запустите удаление. Файлы приложения не удалены.
ProfileCleanupFailed=Some AxGram profile files could not be deleted. Close programs using this folder and remove the remaining profile files manually: %1
ru.ProfileCleanupFailed=Не удалось удалить часть данных AxGram. Закройте программы, использующие эту папку, и удалите оставшиеся файлы профиля вручную: %1
ru.RemoveProfile=Удалить также аккаунты AxGram, настройки, архив сообщений и кэш медиа?%n%nВыберите «Нет», чтобы сохранить данные для повторной установки.

[UninstallDelete]
Type: files; Name: "{app}\log.txt"
Type: filesandordirs; Name: "{app}\DebugLogs"
Type: filesandordirs; Name: "{app}\tupdates"
Type: filesandordirs; Name: "{app}\tdumps"
#ifdef DebugBuild
Type: dirifempty; Name: "{app}\TelegramForcePortable"
#endif
Type: dirifempty; Name: "{app}"

[Code]
var RemoveProfile: Boolean;

function ApplicationFilesAvailable: Boolean;
var Stream: TFileStream;
begin
  Result := not FileExists(ExpandConstant('{app}\{#MyAppExeName}'));
  if Result then Exit;
  try
    Stream := TFileStream.Create(ExpandConstant('{app}\{#MyAppExeName}'), fmOpenReadWrite or fmShareExclusive);
    try
      Result := True;
    finally
      Stream.Free;
    end;
  except
    Result := False;
  end;
end;

function CloseApplicationForUninstall: Boolean;
var ResultCode, Attempt: Integer;
begin
  Result := ApplicationFilesAvailable;
  if Result then Exit;
  Log('Requesting graceful exit from the installed AxGram executable.');
  if not Exec(ExpandConstant('{app}\{#MyAppExeName}'), '-noupdate -quit', ExpandConstant('{app}'), SW_HIDE, ewNoWait, ResultCode) then
    Log('Could not start the exit request: ' + SysErrorMessage(ResultCode));
  for Attempt := 1 to 150 do
  begin
    Sleep(100);
    Result := ApplicationFilesAvailable;
    if Result then Exit;
  end;
  Log(CustomMessage('CloseAppFailed'));
end;

function InitializeUninstall: Boolean;
begin
  RemoveProfile := False;
  if not UninstallSilent then
    RemoveProfile := MsgBox(CustomMessage('RemoveProfile'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = idYes;
  Result := CloseApplicationForUninstall;
  if not Result and not UninstallSilent then
    MsgBox(CustomMessage('CloseAppFailed'), mbError, MB_OK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
#ifndef DebugBuild
  ResultCode: Integer;
#endif
  CleanupSucceeded: Boolean;
  ProfileFolder: String;
begin
#ifndef DebugBuild
  if CurUninstallStep = usUninstall then
    Exec(ExpandConstant('{app}\{#MyAppExeName}'), '-cleanup', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
#endif
  if (CurUninstallStep = usPostUninstall) and RemoveProfile then
  begin
    CleanupSucceeded := True;
    if not DeleteFile(ExpandConstant('{app}\data')) and FileExists(ExpandConstant('{app}\data')) then
      CleanupSucceeded := False;
    if not DeleteFile(ExpandConstant('{app}\data_config')) and FileExists(ExpandConstant('{app}\data_config')) then
      CleanupSucceeded := False;
    if not DelTree(ExpandConstant('{app}\tdata'), True, True, True) and DirExists(ExpandConstant('{app}\tdata')) then
      CleanupSucceeded := False;
    if not DelTree(ExpandConstant('{app}\tcache'), True, True, True) and DirExists(ExpandConstant('{app}\tcache')) then
      CleanupSucceeded := False;
    ProfileFolder := ExpandConstant('{app}\TelegramForcePortable');
    if not DelTree(ProfileFolder, True, True, True) and DirExists(ProfileFolder) then
      CleanupSucceeded := False;
    if not CleanupSucceeded then
    begin
      Log('AxGram profile cleanup incomplete.');
      if not UninstallSilent then
        MsgBox(FmtMessage(CustomMessage('ProfileCleanupFailed'), [ProfileFolder]), mbError, MB_OK);
    end;
  end;
  if CurUninstallStep = usPostUninstall then
  begin
    RemoveDir(ExpandConstant('{app}\TelegramForcePortable'));
    RemoveDir(ExpandConstant('{app}'));
  end;
end;

#ifndef DebugBuild
const CSIDL_DESKTOPDIRECTORY = $0010;
      CSIDL_COMMON_DESKTOPDIRECTORY = $0019;

procedure CurStepChanged(CurStep: TSetupStep);
var ResultCode: Integer;
    HasOldKey: Boolean;
    HasNewKey: Boolean;
    HasOldLnk: Boolean;
    HasNewLnk: Boolean;
    UserDesktopLnk: String;
    CommonDesktopLnk: String;
begin
  if CurStep = ssPostInstall then
  begin
    HasNewKey := RegKeyExists(HKEY_CURRENT_USER, 'Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{{#MyAppId}}_is1') or RegKeyExists(HKEY_CURRENT_USER, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{{#MyAppId}}_is1');
    HasOldKey := RegKeyExists(HKEY_LOCAL_MACHINE, 'SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{{#MyAppId}}_is1') or RegKeyExists(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{{#MyAppId}}_is1');
    UserDesktopLnk := ExpandFileName(GetShellFolderByCSIDL(CSIDL_DESKTOPDIRECTORY, False) + '\{#MyAppShortName}.lnk');
    CommonDesktopLnk := ExpandFileName(GetShellFolderByCSIDL(CSIDL_COMMON_DESKTOPDIRECTORY, False) + '\{#MyAppShortName}.lnk');
    HasNewLnk := FileExists(UserDesktopLnk);
    HasOldLnk := FileExists(CommonDesktopLnk) and (UserDesktopLnk <> CommonDesktopLnk);
    if (HasOldKey and HasNewKey) or (HasOldLnk and HasNewLnk) then
    begin
      if (GetWindowsVersion >= $06000000) then // Vista or later
        ShellExec('runas', ExpandConstant('{app}\{#MyAppExeName}'), '-fixprevious', '', SW_SHOW, ewWaitUntilTerminated, ResultCode)
      else
        ShellExec('', ExpandConstant('{app}\{#MyAppExeName}'), '-fixprevious', '', SW_SHOW, ewWaitUntilTerminated, ResultCode);
    end;
  end;
end;
#endif
