; Unified offline setup. Historical HarmonyContinuation.iss is retained only for migration fixtures.
#ifndef StageDir
 #error StageDir required
#endif
#ifndef OutputPath
 #error OutputPath required
#endif
#ifndef ProductVersion
 #error ProductVersion required
#endif
#ifndef SetupFileStem
 #define SetupFileStem "HarmonyContinuation-Setup"
#endif
#ifndef LibraryVersion
 #define LibraryVersion "3"
#endif
#ifndef FactoryEntryCount
 #define FactoryEntryCount "629"
#endif
#ifndef MinimumPluginVersion
 #if LibraryVersion == "4"
  #define MinimumPluginVersion "0.10.0-dev.1"
 #else
  #define MinimumPluginVersion "0.9.0-dev.2"
 #endif
#endif
#ifdef TestRoot
 #define TestSuffix "-IsolatedTest"
#else
 #define TestSuffix ""
#endif
#define Identity "HarmonyContinuation-Application" + TestSuffix
[Setup]
AppId={#Identity}
AppName=HarmonyContinuation
AppVersion={#ProductVersion}
AppPublisher=HarmonyContinuation
#ifdef TestRoot
DefaultDirName={#TestRoot}\HarmonyContinuation
PrivilegesRequired=lowest
#else
DefaultDirName={autopf}\HarmonyContinuation
PrivilegesRequired=admin
#endif
UsePreviousAppDir=no
UsePreviousSetupType=no
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputPath}
OutputBaseFilename={#SetupFileStem}{#TestSuffix}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallLogMode=append
CloseApplications=no
RestartApplications=no
SetupLogging=yes
AppModifyPath="{app}\HarmonyContinuation-Setup.exe"
UninstallDisplayName=HarmonyContinuation
InfoBeforeFile=installer-info.txt
[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
[Types]
Name: "full"; Description: "VST3 + Factory Library"
Name: "custom"; Description: "Custom"; Flags: iscustom
[Components]
Name: "plugin"; Description: "HarmonyContinuation VST3 (x64)"; Types: full
Name: "library"; Description: "Factory Library V{#LibraryVersion} ({#FactoryEntryCount})"; Types: full
[Files]
Source: "{#StageDir}\library_manager.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\runtime\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\library_manager.exe"; Flags: dontcopy
Source: "{#StageDir}\runtime\*.dll"; Flags: dontcopy
Source: "{#StageDir}\factory.db"; Flags: dontcopy
Source: "{#StageDir}\plugin-files.txt"; Flags: dontcopy
Source: "{#StageDir}\HarmonyContinuation.vst3\*"; DestDir: "{code:PluginFolder}"; Components: plugin; Flags: ignoreversion recursesubdirs createallsubdirs uninsneveruninstall
Source: "{srcexe}"; DestDir: "{app}"; DestName: "HarmonyContinuation-Setup.exe"; Flags: external ignoreversion; Check: CopySetup
[Icons]
#ifndef TestRoot
Name: "{autoprograms}\HarmonyContinuation\管理安装"; Filename: "{app}\HarmonyContinuation-Setup.exe"
Name: "{autoprograms}\HarmonyContinuation\卸载 HarmonyContinuation"; Filename: "{uninstallexe}"
#endif
[InstallDelete]
; These two obsolete application-owned maintenance entry points are in the old main uninstall log.
Type: files; Name: "{app}\LibraryUpdate.exe"
#ifndef TestRoot
Type: files; Name: "{autoprograms}\HarmonyContinuation\更新进行库.lnk"
#endif
[UninstallDelete]
Type: files; Name: "{app}\components.ini"
Type: files; Name: "{app}\plugin-files.txt"
Type: dirifempty; Name: "{app}"
[Code]
var
  OperationPage, RemovePage: TInputOptionWizardPage;
  StatusPage: TOutputMsgWizardPage;
  Snapshots: TStringList;
  SnapshotPresent: TStringList;
  PluginWasPresent, LibraryWasPresent, OwnPlugin, OwnLibrary: Boolean;
  RemovePlugin, RemoveLibrary, SkipLibrary, Committed, TransactionStarted, Uninstalled: Boolean;
  DetectedVersion, DetectedLibrary, LegacyUninstaller: String;

function Text(Zh, En: String): String;
begin
  if ActiveLanguage = 'chinesesimp' then Result := Zh else Result := En;
end;
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
function AppFolder: String;
begin
#ifdef TestRoot
  Result := '{#TestRoot}\HarmonyContinuation';
#else
  Result := ExpandConstant('{autopf}\HarmonyContinuation');
#endif
end;
function RegistryRoot: Integer;
begin
#ifdef TestRoot
  Result := HKCU64;
#else
  Result := HKLM64;
#endif
end;
function MainKey: String;
begin Result := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#Identity}_is1'; end;
function LibraryKey: String;
begin Result := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\HarmonyContinuation-FactoryLibrary{#TestSuffix}_is1'; end;
function StateFile: String;
begin Result := AppFolder + '\components.ini'; end;
function PluginBinary: String;
begin Result := PluginFolder('') + '\Contents\x86_64-win\HarmonyContinuation.vst3'; end;
function LibraryDatabase: String;
begin Result := StoreFolder + '\{#LibraryVersion}\factory.db'; end;
function CopySetup: Boolean;
begin Result := CompareText(ExpandConstant('{srcexe}'), AppFolder + '\HarmonyContinuation-Setup.exe') <> 0; end;
function FileAttributes(Path: String): LongWord;
  external 'GetFileAttributesW@kernel32.dll stdcall';
function SafePath(Path: String): Boolean;
var Attributes: LongWord; Parent: String;
begin
  Result := False;
  repeat
    Attributes := FileAttributes(Path);
    if (Attributes <> $FFFFFFFF) and ((Attributes and $400) <> 0) then Exit;
    Parent := ExtractFileDir(Path);
    if CompareText(Parent, Path) = 0 then Break;
    Path := Parent;
  until Length(Path) < 3;
  Result := True;
end;
function ReadVersion: String;
var Lines: TArrayOfString; I, P: Integer; S: String;
begin
  Result := '';
  if LoadStringsFromFile(PluginFolder('') + '\Contents\Resources\moduleinfo.json', Lines) then
    for I := 0 to GetArrayLength(Lines)-1 do begin
      S := Trim(Lines[I]);
      if Pos('"Version": "', S) = 1 then begin
        Delete(S,1,12); P := Pos('"', S); if P > 0 then Result := Copy(S,1,P-1);
        Exit;
      end;
    end;
end;
function VersionOrder(Value: String): Int64;
var I, P, N: Integer; S, Suffix, Part: String; Major, Minor, Patch: Int64;
begin
  Result := -1; S := Value; Suffix := ''; P := Pos('-',S);
  if P > 0 then begin Suffix := Copy(S,P+1,Length(S)); S := Copy(S,1,P-1); end;
  Major := 0; Minor := 0; Patch := 0;
  for I := 0 to 2 do begin
    P := Pos('.',S); if (P=0) and (I<2) then Exit;
    if P=0 then Part := S else Part := Copy(S,1,P-1);
    N := StrToIntDef(Part,-1); if (N<0) or (N>999) then Exit;
    if I=0 then Major:=N else if I=1 then Minor:=N else Patch:=N;
    if P>0 then Delete(S,1,P) else S:='';
  end;
  if S<>'' then Exit;
  N := 900000;
  if Pos('dev.',Suffix)=1 then N:=100000+StrToIntDef(Copy(Suffix,5,Length(Suffix)),0)
  else if Pos('rc.',Suffix)=1 then N:=500000+StrToIntDef(Copy(Suffix,4,Length(Suffix)),0)
  else if Suffix<>'' then Exit;
  Result := ((Major*1000+Minor)*1000+Patch)*1000000+N;
end;
function Manager(Params: String; Temporary: Boolean): Boolean;
var Code: Integer; Exe: String;
begin
  if Temporary then Exe := ExpandConstant('{tmp}\library_manager.exe') else Exe := AppFolder+'\library_manager.exe';
  Result := Exec(Exe,Params,ExtractFileDir(Exe),SW_HIDE,ewWaitUntilTerminated,Code);
  Result := Result and (Code=0); Log('manager '+Params+' success='+IntToStr(Ord(Result)));
end;
procedure ExtractPayload;
begin
  ExtractTemporaryFile('library_manager.exe'); ExtractTemporaryFiles('{tmp}\*.dll');
  ExtractTemporaryFile('factory.db'); ExtractTemporaryFile('plugin-files.txt');
end;
function ReadActive: String;
var S: AnsiString;
begin Result:=''; if LoadStringFromFile(StoreFolder+'\active.txt',S) then Result:=Trim(String(S)); end;
function ManagedHash: String;
begin Result := GetIniString('Components','LibraryHash','',StateFile); end;
function ManifestPaths(Manifest: String; Verify: Boolean): Boolean;
var Lines: TArrayOfString; I: Integer; Rel, Hash, Path: String;
begin
  Result:=False;
  if not LoadStringsFromFile(Manifest,Lines) then Exit;
  for I:=0 to GetArrayLength(Lines)-1 do begin
    if (Length(Lines[I])<66) or (Copy(Lines[I],65,1)<>'|') then Exit;
    Hash:=Copy(Lines[I],1,64); Rel:=Copy(Lines[I],66,Length(Lines[I]));
    if (Pos('..',Rel)>0) or (Pos(':',Rel)>0) or (Copy(Rel,1,1)='\') or (Pos('/',Rel)>0) then Exit;
    Path:=PluginFolder('')+'\'+Rel;
    if not SafePath(Path) then Exit;
    if Verify and FileExists(Path) then begin
      if CompareText(GetSHA256OfFile(Path),Hash)<>0 then Exit;
    end;
  end;
  Result:=True;
end;
procedure RememberFile(Path: String);
var Index: Integer; Backup: String;
begin
  if Snapshots.IndexOf(Path)>=0 then Exit;
  if not SafePath(Path) then RaiseException('Unsafe linked path: '+Path);
  Index:=Snapshots.Count; Backup:=ExpandConstant('{tmp}\hc-backup\')+IntToStr(Index);
  if not ForceDirectories(ExtractFileDir(Backup)) then RaiseException('Cannot create recovery backup');
  if FileExists(Path) then begin
    if not FileCopy(Path,Backup,False) then RaiseException('Cannot back up '+Path);
    SnapshotPresent.Add('1');
  end else SnapshotPresent.Add('0');
  Snapshots.Add(Path);
end;
procedure RememberManifest(Manifest: String);
var Lines: TArrayOfString; I: Integer;
begin
  if not ManifestPaths(Manifest,False) then RaiseException('Invalid managed plugin manifest');
  LoadStringsFromFile(Manifest,Lines);
  for I:=0 to GetArrayLength(Lines)-1 do RememberFile(PluginFolder('')+'\'+Copy(Lines[I],66,Length(Lines[I])));
end;
procedure Rollback;
var I: Integer; Path, Backup: String;
begin
  if not TransactionStarted then Exit;
  for I:=Snapshots.Count-1 downto 0 do begin
    Path:=Snapshots[I]; Backup:=ExpandConstant('{tmp}\hc-backup\')+IntToStr(I);
    if SnapshotPresent[I]='1' then begin
      ForceDirectories(ExtractFileDir(Path)); if not FileCopy(Backup,Path,False) then Log('RESTORE FAILED: '+Path);
    end else if FileExists(Path) then if not DeleteFile(Path) then Log('CLEANUP FAILED: '+Path);
  end;
  TransactionStarted:=False;
end;
procedure RemoveManagedPlugin;
var Lines: TArrayOfString; I: Integer; Path, Parent: String;
begin
  LoadStringsFromFile(AppFolder+'\plugin-files.txt',Lines);
  for I:=0 to GetArrayLength(Lines)-1 do begin
    Path:=PluginFolder('')+'\'+Copy(Lines[I],66,Length(Lines[I]));
    if FileExists(Path) and not DeleteFile(Path) then RaiseException('Cannot remove '+Path);
    Parent:=ExtractFileDir(Path);
    while (Length(Parent)>=Length(PluginFolder(''))) do begin
      RemoveDir(Parent); Parent:=ExtractFileDir(Parent);
    end;
  end;
end;
procedure RemoveManagedLibrary;
begin
  if FileExists(LibraryDatabase) and not DeleteFile(LibraryDatabase) then RaiseException('Cannot remove managed Factory DB');
  if ReadActive='{#LibraryVersion}' then
    if not DeleteFile(StoreFolder+'\active.txt') then RaiseException('Cannot clear active Factory pointer');
  RemoveDir(ExtractFileDir(LibraryDatabase)); RemoveDir(StoreFolder);
end;
function KnownBundledFactory: Boolean;
var Path, Hash: String;
begin
  Path:=PluginFolder('')+'\Contents\Resources\factory.db'; Result:=True;
  if not FileExists(Path) then Exit;
  Hash:=GetSHA256OfFile(Path);
  Result:=(CompareText(Hash,'{#FactoryHash}')=0) or (CompareText(Hash,'{#LegacyFactoryHash}')=0);
end;
function Selection(Name: String): Boolean;
begin Result:=WizardIsComponentSelected(Name); end;
procedure Detect;
var S, Location, Publisher: String;
begin
  OwnPlugin:=GetIniString('Components','Plugin','0',StateFile)='1';
  OwnLibrary:=GetIniString('Components','Library','0',StateFile)='1';
  PluginWasPresent:=FileExists(PluginBinary); DetectedVersion:=ReadVersion;
  DetectedLibrary:=ReadActive; LibraryWasPresent:=DetectedLibrary<>'';
  LegacyUninstaller:='';
  if RegKeyExists(RegistryRoot,MainKey) then begin
    RegQueryStringValue(RegistryRoot,MainKey,'Inno Setup: App Path',Location);
    if CompareText(RemoveBackslashUnlessRoot(Location),AppFolder)<>0 then RaiseException(Text('旧主程序安装位置无法安全接管，请先使用原卸载器处理。','Unknown legacy application location; use its original uninstaller first.'));
  end else if PluginWasPresent then RaiseException(Text('发现无安装记录的插件，未接管。请备份后移走该插件，再安装。','An unregistered plugin exists. Back it up and move it before installation.'));
  if RegKeyExists(RegistryRoot,LibraryKey) then begin
#ifdef TestRoot
    Location:='{#TestRoot}\HarmonyContinuation Library';
#else
    Location:=ExpandConstant('{autopf}\HarmonyContinuation Library');
#endif
    RegQueryStringValue(RegistryRoot,LibraryKey,'Inno Setup: App Path',S);
    RegQueryStringValue(RegistryRoot,LibraryKey,'Publisher',Publisher);
    if (CompareText(RemoveBackslashUnlessRoot(S),Location)<>0) or (Publisher<>'HarmonyContinuation') then
      RaiseException(Text('旧进行库安装记录无法可靠识别；保留原状，请先运行原卸载器。','Unrecognized legacy library registration; preserved. Use its original uninstaller first.'));
    RegQueryStringValue(RegistryRoot,LibraryKey,'UninstallString',S);
    LegacyUninstaller:=Location+'\unins000.exe';
    if (CompareText(S,'"'+LegacyUninstaller+'"')<>0) or not FileExists(LegacyUninstaller) then
      RaiseException('Unrecognized legacy library uninstaller; no registration was removed.');
  end;
end;
function RunFullUninstall: Boolean;
var Code: Integer; Exe, Params: String;
begin
  Result:=False; Exe:=AppFolder+'\unins000.exe'; if not FileExists(Exe) then Exit;
  Params:='/NORESTART'; if WizardSilent then Params:=Params+' /VERYSILENT /SUPPRESSMSGBOXES';
  Result:=Exec(Exe,Params,AppFolder,SW_SHOWNORMAL,ewWaitUntilTerminated,Code); Result:=Result and (Code=0);
end;
function InitializeSetup: Boolean;
begin
  Snapshots:=TStringList.Create; SnapshotPresent:=TStringList.Create;
  Result:=True;
  if ExpandConstant('{param:OPERATION|}')='uninstall' then begin
    Uninstalled:=RunFullUninstall;
    if not Uninstalled then MsgBox('Uninstall failed or no managed installation exists.',mbError,MB_OK);
    Result:=False;
  end;
end;
procedure InitializeWizard;
var Op: String;
begin
  Detect;
  StatusPage:=CreateOutputMsgPage(wpWelcome,Text('检测安装状态','Detected installation'),'',
    Text('主程序版本：','Plugin version: ')+DetectedVersion+#13#10+'Factory Library: '+DetectedLibrary+#13#10+
    'VST3: '+PluginFolder('')+#13#10+'Factory: '+StoreFolder+#13#10+
    Text('更新时未选择的组件保持原状。删除组件必须选择“修改组件”中的明确移除项。个人数据始终保留。','Unselected update components are preserved. Removal is explicit under Modify. Personal data is always preserved.'));
  OperationPage:=CreateInputOptionPage(StatusPage.ID,Text('选择操作','Choose operation'),'',
    Text('安装 / 更新；修复；修改组件；完整卸载','Install / Update; Repair; Modify; Full uninstall'),True,False);
  OperationPage.Add(Text('安装 / 更新所选组件','Install / update selected components'));
  OperationPage.Add(Text('修复所选组件','Repair selected components'));
  OperationPage.Add(Text('修改组件（可明确移除）','Modify components (explicit removal)'));
  OperationPage.Add(Text('完整卸载（保留个人数据）','Full uninstall (preserve personal data)'));
  OperationPage.SelectedValueIndex:=0;
  Op:=ExpandConstant('{param:OPERATION|install}');
  if Op='repair' then OperationPage.SelectedValueIndex:=1 else if Op='modify' then OperationPage.SelectedValueIndex:=2;
  RemovePage:=CreateInputOptionPage(wpSelectComponents,Text('明确移除组件','Explicit component removal'),'',
    Text('仅勾选需要移除的已管理组件。更新页取消勾选不会卸载。','Check only components to remove. Deselecting an update never uninstalls it.'),False,False);
  RemovePage.Add('VST3'); RemovePage.Add('Factory Library');
  RemovePage.Values[0]:=Pos('plugin',ExpandConstant('{param:REMOVE|}'))>0;
  RemovePage.Values[1]:=Pos('library',ExpandConstant('{param:REMOVE|}'))>0;
end;
function ShouldSkipPage(PageID: Integer): Boolean;
begin Result:=(PageID=RemovePage.ID) and (OperationPage.SelectedValueIndex<>2); end;
procedure CancelButtonClick(CurPageID: Integer; var Cancel, Confirm: Boolean);
begin if Uninstalled then begin Cancel:=True; Confirm:=False; end; end;
function NextButtonClick(CurPageID: Integer): Boolean;
var Selected: String;
begin
  Result:=True;
  if (CurPageID=OperationPage.ID) and (OperationPage.SelectedValueIndex=3) then begin
    Uninstalled:=RunFullUninstall; Result:=False;
    if Uninstalled then WizardForm.Close else MsgBox('Uninstall failed or not installed.',mbError,MB_OK);
  end;
  if (CurPageID=RemovePage.ID) or ((CurPageID=wpReady) and WizardSilent) then begin
    RemovePlugin:=(OperationPage.SelectedValueIndex=2) and RemovePage.Values[0];
    RemoveLibrary:=(OperationPage.SelectedValueIndex=2) and RemovePage.Values[1];
    Selected:='';
    if Selection('plugin') and not RemovePlugin then Selected:='plugin';
    if Selection('library') and not RemoveLibrary then begin if Selected<>'' then Selected:=Selected+','; Selected:=Selected+'library'; end;
    WizardSelectComponents(Selected);
  end;
end;
function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result:=StatusPage.MsgLabel.Caption+NewLine+OperationPage.CheckListBox.Items[OperationPage.SelectedValueIndex]+NewLine+MemoComponentsInfo;
  if RemovePlugin then Result:=Result+NewLine+'REMOVE: VST3';
  if RemoveLibrary then Result:=Result+NewLine+'REMOVE: Factory Library';
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
var Code, Active: Integer; Path, Hash: String;
begin
  Result:='';
  if not SafePath(AppFolder) or not SafePath(PluginFolder('')) or not SafePath(StoreFolder) then begin Result:='Linked installation/data paths are not managed.'; Exit; end;
  ExtractPayload;
  if not Manager('check-plugin "'+PluginBinary+'"',True) or
     not Manager('check-plugin "'+LibraryDatabase+'"',True) then begin Result:=Text('请关闭 Cubase、FL Studio 或其它占用插件/曲库的程序后重试。','Close Cubase, FL Studio or any process using the plugin/library, then retry.'); Exit; end;
  if RemovePlugin and (not OwnPlugin or not ManifestPaths(AppFolder+'\plugin-files.txt',True)) then begin Result:='Plugin ownership cannot be verified. Repair/update the plugin first; no files removed.'; Exit; end;
  if RemoveLibrary and (not OwnLibrary or ((FileExists(LibraryDatabase)) and (CompareText(GetSHA256OfFile(LibraryDatabase),ManagedHash)<>0))) then begin Result:='Factory ownership/content differs. Repair the managed library first; no files removed.'; Exit; end;
  if Selection('plugin') and PluginWasPresent then begin
    if VersionOrder(DetectedVersion)<0 then begin Result:='Installed plugin version unknown; preserved.'; Exit; end;
    if VersionOrder(DetectedVersion)>VersionOrder('{#ProductVersion}') then
      if SuppressibleMsgBox(Text('已安装较新主程序。确认降级？','A newer plugin is installed. Confirm downgrade?'),mbConfirmation,MB_YESNO,IDNO)<>IDYES then begin Result:='Downgrade cancelled.'; Exit; end;
  end;
  SkipLibrary:=False; Active:=StrToIntDef(ReadActive,0);
  if Selection('library') and (Active>{#LibraryVersion}) then begin
    SkipLibrary:=True; Log('Newer Factory retained; downgrade not performed.');
    if not WizardSilent then MsgBox(Text('检测到较新 Factory，保持原库，不降级。','Newer Factory retained; no downgrade.'),mbInformation,MB_OK);
  end;
  if Selection('library') and not Selection('plugin') and
      (not PluginWasPresent or RemovePlugin or (VersionOrder(DetectedVersion)<0) or
       (VersionOrder(DetectedVersion)<VersionOrder('{#MinimumPluginVersion}'))) then begin
    Result:=Text('Factory V{#LibraryVersion} 需要 {#MinimumPluginVersion} 或更新主程序，请同时勾选主程序升级。',
      'Factory V{#LibraryVersion} requires plugin {#MinimumPluginVersion} or newer; select the plugin component too.'); Exit;
  end;
  if (Selection('plugin') or RemoveLibrary) and not KnownBundledFactory then begin Result:='Unrecognized bundled Factory file preserved. Back it up and move it before maintenance.'; Exit; end;
  if Selection('library') and not SkipLibrary and FileExists(LibraryDatabase) then begin
    Hash:=GetSHA256OfFile(LibraryDatabase);
    if (CompareText(Hash,'{#FactoryHash}')<>0) and not ((OperationPage.SelectedValueIndex=1) and OwnLibrary and (CompareText(ManagedHash,'{#FactoryHash}')=0)) then begin
      Result:='Same library version has different content; no overwrite. Repair a known managed component or retain it.'; Exit;
    end;
  end;
  if LegacyUninstaller<>'' then begin
    if not Exec(LegacyUninstaller,'/VERYSILENT /SUPPRESSMSGBOXES /NORESTART',ExtractFileDir(LegacyUninstaller),SW_HIDE,ewWaitUntilTerminated,Code) or (Code<>0) then begin Result:='Legacy library uninstall failed; no new files installed.'; Exit; end;
    if RegKeyExists(RegistryRoot,LibraryKey) then begin Result:='Legacy record remains; no forced registry cleanup performed.'; Exit; end;
    LegacyUninstaller:='';
  end;
end;
procedure CurStepChanged(CurStep: TSetupStep);
var Path: String;
begin
  if CurStep=ssInstall then begin
    TransactionStarted:=True;
    RememberFile(StateFile); RememberFile(AppFolder+'\plugin-files.txt');
    RememberFile(StoreFolder+'\active.txt');
    if Selection('plugin') then RememberManifest(ExpandConstant('{tmp}\plugin-files.txt'));
    if RemovePlugin then RememberManifest(AppFolder+'\plugin-files.txt');
    if (Selection('library') and not SkipLibrary) or RemoveLibrary then RememberFile(LibraryDatabase);
    if Selection('plugin') or RemoveLibrary then RememberFile(PluginFolder('')+'\Contents\Resources\factory.db');
    try
      if RemovePlugin then RemoveManagedPlugin;
      if RemoveLibrary then RemoveManagedLibrary;
      Path:=PluginFolder('')+'\Contents\Resources\factory.db';
      if (Selection('plugin') or RemoveLibrary) and FileExists(Path) then
        if not DeleteFile(Path) then RaiseException('Cannot remove known legacy bundled Factory copy');
      if Selection('library') and not SkipLibrary then begin
        if FileExists(LibraryDatabase) and (CompareText(GetSHA256OfFile(LibraryDatabase),'{#FactoryHash}')<>0) then
          if not DeleteFile(LibraryDatabase) then RaiseException('Cannot replace damaged managed Factory');
        if not Manager('install "'+ExpandConstant('{tmp}\factory.db')+'" "'+StoreFolder+'"',True) then RaiseException('Factory install failed; restoring previous data.');
      end;
    except Rollback; RaiseException(GetExceptionMessage); end;
  end;
  if CurStep=ssPostInstall then begin
    if Selection('plugin') then begin
      if not FileCopy(ExpandConstant('{tmp}\plugin-files.txt'),AppFolder+'\plugin-files.txt',False) then RaiseException('Cannot record plugin ownership');
      OwnPlugin:=True;
    end;
    if RemovePlugin then OwnPlugin:=False;
    if Selection('library') and not SkipLibrary then OwnLibrary:=True;
    if RemoveLibrary then OwnLibrary:=False;
    if not SetIniString('Components','Plugin',IntToStr(Ord(OwnPlugin)),StateFile) or
       not SetIniString('Components','Library',IntToStr(Ord(OwnLibrary)),StateFile) or
       not SetIniString('Components','LibraryHash','{#FactoryHash}',StateFile) then RaiseException('Cannot record component state');
    Committed:=True; TransactionStarted:=False;
  end;
end;
procedure DeinitializeSetup;
begin
  if not Committed then Rollback;
  if Snapshots<>nil then Snapshots.Free;
  if SnapshotPresent<>nil then SnapshotPresent.Free;
end;
function InitializeUninstall: Boolean;
begin
  Snapshots:=TStringList.Create; SnapshotPresent:=TStringList.Create;
  OwnPlugin:=GetIniString('Components','Plugin','0',StateFile)='1';
  OwnLibrary:=GetIniString('Components','Library','0',StateFile)='1';
  Result:=SafePath(AppFolder) and SafePath(PluginFolder('')) and SafePath(StoreFolder);
  if Result and OwnPlugin then Result:=ManifestPaths(AppFolder+'\plugin-files.txt',True);
  if Result and OwnLibrary and FileExists(LibraryDatabase) then Result:=CompareText(GetSHA256OfFile(LibraryDatabase),ManagedHash)=0;
  if Result then Result:=Manager('check-plugin "'+PluginBinary+'"',False) and Manager('check-plugin "'+LibraryDatabase+'"',False);
  if not Result then SuppressibleMsgBox(Text('请关闭宿主。若安装文件被修改，请先修复；未删除文件或个人数据。','Close hosts; repair modified managed files before removal. No personal data was deleted.'),mbError,MB_OK,IDOK);
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep=usUninstall then begin
    TransactionStarted:=True;
    RememberFile(StoreFolder+'\active.txt');
    if OwnPlugin then RememberManifest(AppFolder+'\plugin-files.txt');
    if OwnLibrary then RememberFile(LibraryDatabase);
    try
      if OwnPlugin then RemoveManagedPlugin;
      if OwnLibrary then RemoveManagedLibrary;
      TransactionStarted:=False;
    except Rollback; RaiseException(GetExceptionMessage); end;
  end;
end;
procedure DeinitializeUninstall;
begin
  Rollback;
  if Snapshots<>nil then Snapshots.Free;
  if SnapshotPresent<>nil then SnapshotPresent.Free;
end;
