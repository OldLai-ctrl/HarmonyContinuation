; Build with tools/build-installer.ps1. Library-only packages never replace the plugin.
#ifndef StageDir
  #error StageDir is required
#endif
#ifndef OutputPath
  #error OutputPath is required
#endif
#ifndef ProductVersion
  #define ProductVersion "0.8.0-dev.1-installer.1"
#endif
#ifndef LibraryVersion
  #define LibraryVersion "2"
#endif
#ifdef TestRoot
  #define TestSuffix "-IsolatedTest"
#else
  #define TestSuffix ""
#endif
#ifdef LibraryOnly
  #define AppTitle "HarmonyContinuation 进行库"
  #define AppIdentity "HarmonyContinuation-FactoryLibrary" + TestSuffix
  #define InstallFolder "HarmonyContinuation Library"
  #define DisplayVersion LibraryVersion
  #define FileStem "HarmonyContinuation-Library-" + LibraryVersion + "-Setup" + TestSuffix
#else
  #define AppTitle "HarmonyContinuation"
  #define AppIdentity "HarmonyContinuation-Application" + TestSuffix
  #define InstallFolder "HarmonyContinuation"
  #define DisplayVersion ProductVersion
  #define FileStem "HarmonyContinuation-" + ProductVersion + "-Setup" + TestSuffix
#endif

[Setup]
AppId={#AppIdentity}
AppName={#AppTitle}
AppVersion={#DisplayVersion}
AppPublisher=HarmonyContinuation
#ifdef TestRoot
DefaultDirName={#TestRoot}\{#InstallFolder}
PrivilegesRequired=lowest
CreateUninstallRegKey=no
DisableProgramGroupPage=yes
#else
DefaultDirName={autopf}\{#InstallFolder}
PrivilegesRequired=admin
DisableProgramGroupPage=yes
#endif
DisableDirPage=yes
DisableWelcomePage=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputPath}
OutputBaseFilename={#FileStem}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallLogMode=append
CloseApplications=no
RestartApplications=no
SetupLogging=yes
UninstallDisplayName={#AppTitle}
InfoBeforeFile=installer-info.txt

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

#ifndef LibraryOnly
[Types]
Name: "full"; Description: "安装或更新插件及进行库"
Name: "library"; Description: "仅更新进行库"
Name: "custom"; Description: "自定义"; Flags: iscustom
[Components]
Name: "plugin"; Description: "HarmonyContinuation VST3 插件"; Types: full
Name: "library"; Description: "本地内置进行库（保留个人进行及旧版库）"; Types: full library
#endif

[Files]
Source: "{#StageDir}\library_manager.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\runtime\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\library_manager.exe"; Flags: dontcopy
Source: "{#StageDir}\runtime\*.dll"; Flags: dontcopy
Source: "{#StageDir}\factory.db"; Flags: dontcopy
#ifndef LibraryOnly
Source: "{#StageDir}\HarmonyContinuation.vst3\*"; DestDir: "{code:PluginFolder}"; Components: plugin; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\LibraryUpdate.exe"; DestDir: "{app}"; Flags: ignoreversion
#endif

#ifndef TestRoot
[Icons]
Name: "{autoprograms}\HarmonyContinuation\卸载 {#AppTitle}"; Filename: "{uninstallexe}"
#ifndef LibraryOnly
Name: "{autoprograms}\HarmonyContinuation\更新进行库"; Filename: "{app}\LibraryUpdate.exe"
#endif
#endif

[Code]
function PluginFolder(Param: String): String;
begin
#ifdef TestRoot
  Result := '{#TestRoot}\VST3\HarmonyContinuation.vst3';
#else
  Result := ExpandConstant('{commoncf64}\VST3\HarmonyContinuation.vst3');
#endif
end;

function StoreFolder: String;
begin
#ifdef TestRoot
  Result := '{#TestRoot}\Data\Libraries\Factory';
#else
  Result := ExpandConstant('{commonappdata}\HarmonyContinuation\Libraries\Factory');
#endif
end;

function BackupFolder: String;
begin
#ifdef TestRoot
  Result := '{#TestRoot}\Data\Backups\Plugins';
#else
  Result := ExpandConstant('{commonappdata}\HarmonyContinuation\Backups\Plugins');
#endif
end;

function WantsLibrary: Boolean;
begin
#ifdef LibraryOnly
  Result := True;
#else
  Result := WizardIsComponentSelected('library');
#endif
end;

procedure ExtractManager;
begin
  ExtractTemporaryFile('library_manager.exe');
  ExtractTemporaryFiles('{tmp}\*.dll');
end;

function RunManager(Params: String): Boolean;
var ExitCode: Integer;
begin
  Result := Exec(ExpandConstant('{tmp}\library_manager.exe'), Params, ExpandConstant('{tmp}'),
    SW_HIDE, ewWaitUntilTerminated, ExitCode);
  Result := Result and (ExitCode = 0);
  Log('Library manager: ' + Params + '; exit=' + IntToStr(ExitCode));
end;

function MoveLegacyBackups: String;
var
  Found: TFindRec;
  Source, Destination, BinaryPath: String;
  Suffix: Integer;
begin
  Result := '';
  if FindFirst(AddBackslash(ExtractFileDir(PluginFolder(''))) + 'HarmonyContinuation*-backup-*', Found) then begin
    try
      repeat
        if (Found.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then begin
          Source := AddBackslash(ExtractFileDir(PluginFolder(''))) + Found.Name;
          BinaryPath := AddBackslash(Source) + 'Contents\x86_64-win\HarmonyContinuation.vst3';
          if FileExists(BinaryPath) then begin
            if (Found.Attributes and $400) <> 0 then begin
              Result := '旧插件备份目录是链接，请先将其移出 VST3 扫描目录：' + Source;
              Exit;
            end;
            if not RunManager('check-plugin "' + BinaryPath + '"') then begin
              Result := '旧插件备份仍被宿主使用，请关闭宿主后重试：' + Source;
              Exit;
            end;
            if not ForceDirectories(BackupFolder) then begin
              Result := '无法建立旧插件备份目录，未删除旧文件。';
              Exit;
            end;
            Destination := AddBackslash(BackupFolder) + Found.Name;
            Suffix := 0;
            while DirExists(Destination) or FileExists(Destination) do begin
              Suffix := Suffix + 1;
              Destination := AddBackslash(BackupFolder) + Found.Name + '-' + IntToStr(Suffix);
            end;
            if not RenameFile(Source, Destination) then begin
              Result := '无法移走旧插件备份，请手动将其移出 VST3 扫描目录：' + Source;
              Exit;
            end;
            Log('Preserved legacy plugin backup outside scan directory: ' + Source + ' -> ' + Destination);
          end;
        end;
      until not FindNext(Found);
    finally
      FindClose(Found);
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  ExtractManager;
#ifndef LibraryOnly
  if WizardIsComponentSelected('plugin') then begin
    if not RunManager('check-plugin "' + PluginFolder('') + '\Contents\x86_64-win\HarmonyContinuation.vst3"') then begin
      Result := '请先关闭 Cubase 或其他正在使用插件的宿主，再重新安装。';
      Exit;
    end;
    Result := MoveLegacyBackups;
    if Result <> '' then Exit;
  end;
#endif
  if WantsLibrary then begin
    ExtractTemporaryFile('factory.db');
    if not RunManager('inspect "' + ExpandConstant('{tmp}\factory.db') + '"') then begin
      Result := '进行库验证失败，未修改已安装的进行库。';
      Exit;
    end;
    if not RunManager('install "' + ExpandConstant('{tmp}\factory.db') + '" "' + StoreFolder + '"') then
      Result := '进行库更新失败。旧版和个人进行仍保留；请查看安装日志或联系开发者。';
  end;
end;

function InitializeUninstall: Boolean;
var ExitCode: Integer;
begin
  Result := True;
#ifndef LibraryOnly
  if FileExists(ExpandConstant('{app}\library_manager.exe')) then begin
    Result := Exec(ExpandConstant('{app}\library_manager.exe'),
      'check-plugin "' + PluginFolder('') + '\Contents\x86_64-win\HarmonyContinuation.vst3"',
      ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ExitCode);
    Result := Result and (ExitCode = 0);
    if not Result then MsgBox('请先关闭正在使用插件的宿主，再卸载。', mbError, MB_OK);
  end;
#endif
end;
