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
 #if LibraryVersion == "6"
  #define MinimumPluginVersion "0.11.0-dev.4"
 #elif LibraryVersion == "5"
  #define MinimumPluginVersion "0.10.0-dev.12"
 #elif LibraryVersion == "4"
  #define MinimumPluginVersion "0.10.0-dev.1"
 #else
  #define MinimumPluginVersion "0.9.0-dev.2"
 #endif
#endif
#ifndef AnnotationHash
 #if LibraryVersion == "6"
  #error AnnotationHash required for Factory V6
 #else
  #define AnnotationHash ""
 #endif
#endif
#ifdef TestRoot
 #ifndef TestSuffix
  #define TestSuffix "-IsolatedTest"
 #endif
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
#if LibraryVersion == "6"
Source: "{#StageDir}\factory.hca"; Flags: dontcopy
#endif
Source: "{#StageDir}\official-factory\*.db"; DestDir: "{tmp}\official-factory"; Flags: dontcopy
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
  SnapshotHashes: TStringList;
  RecoveryDir, FailureMessage: String;
  RegistryCaptured, RegistryWasPresent, CriticalFailure, RecoveryFailed, AppWasPresent, RecoveryCreated: Boolean;
  PluginWasPresent, LibraryWasPresent, OwnPlugin, OwnLibrary: Boolean;
  RemovePlugin, RemoveLibrary, SkipLibrary, Committed, TransactionStarted, Uninstalled: Boolean;
  DetectedVersion, DetectedLibrary, LegacyUninstaller, BundledFactoryHash: String;

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
function ManagedLibraryDatabase: String;
var Version: String; ActiveText: AnsiString;
begin
  Version:=GetIniString('Components','LibraryVersion','',StateFile);
  if (Version='') and LoadStringFromFile(StoreFolder+'\active.txt',ActiveText) then Version:=Trim(String(ActiveText));
  if (Version='') or (IntToStr(StrToIntDef(Version,0))<>Version) or (StrToIntDef(Version,0)<1) then
    Result:=LibraryDatabase else Result:=StoreFolder+'\'+Version+'\factory.db';
end;
function ManagedAnnotationFile: String;
var Version: String; ActiveText: AnsiString;
begin
  Version:=GetIniString('Components','LibraryVersion','',StateFile);
  if (Version='') and LoadStringFromFile(StoreFolder+'\active.txt',ActiveText) then Version:=Trim(String(ActiveText));
  if Version='6' then Result:=StoreFolder+'\6\factory.hca' else Result:='';
end;
function ManagedAnnotationHash: String;
begin Result:=GetIniString('Components','AnnotationHash','',StateFile); end;
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
#if LibraryVersion == "6"
  ExtractTemporaryFile('factory.hca');
#endif
  ExtractTemporaryFiles('{tmp}\official-factory\*.db');
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
  Index:=Snapshots.Count; Backup:=RecoveryDir+'\'+IntToStr(Index);
  if not ForceDirectories(ExtractFileDir(Backup)) then RaiseException('Cannot create recovery backup');
  if FileExists(Path) then begin
    if not FileCopy(Path,Backup,False) then RaiseException('Cannot back up '+Path);
    SnapshotPresent.Add('1');
    SnapshotHashes.Add(GetSHA256OfFile(Backup));
  end else begin
    if DirExists(Path) then SnapshotPresent.Add('D') else SnapshotPresent.Add('0');
    SnapshotHashes.Add('');
  end;
  Snapshots.Add(Path);
end;
procedure RememberManifest(Manifest: String);
var Lines: TArrayOfString; I: Integer;
begin
  if not ManifestPaths(Manifest,False) then RaiseException('Invalid managed plugin manifest');
  LoadStringsFromFile(Manifest,Lines);
  for I:=0 to GetArrayLength(Lines)-1 do RememberFile(PluginFolder('')+'\'+Copy(Lines[I],66,Length(Lines[I])));
end;
function RecoveryRoot: String;
begin
#ifdef TestRoot
  Result:='{#TestRoot}\Recovery';
#else
  Result:=ExpandConstant('{commonappdata}\HarmonyContinuation\InstallerRecovery');
#endif
end;
procedure BeginRecovery;
begin
  RecoveryDir:=RecoveryRoot+'\'+ExtractFileName(ExpandConstant('{tmp}'));
  if not SafePath(RecoveryDir) or DirExists(RecoveryDir) or not ForceDirectories(RecoveryDir) then
    RaiseException('Cannot create private recovery directory');
  RecoveryCreated:=True;
end;
function HasPendingRecovery: Boolean;
var Find: TFindRec;
begin
  Result:=FileExists(RecoveryRoot+'\pending.txt');
  if Result then Exit;
  { A failed recovery may also fail to write a second marker. The journal
    created before mutation is sufficient to refuse a destructive retry. }
  if FindFirst(RecoveryRoot+'\*',Find) then begin
    try repeat
      if ((Find.Attributes and FILE_ATTRIBUTE_DIRECTORY)<>0) and (Find.Name<>'.') and (Find.Name<>'..') then
        if FileExists(RecoveryRoot+'\'+Find.Name+'\files.txt') then begin Result:=True; Break; end;
    until not FindNext(Find);
    finally FindClose(Find); end;
  end;
end;
function RegistryCommand(Params: String): Boolean;
var Code: Integer;
begin
  { Setup 6 is 32 bit; use the native reg.exe to match HKLM64/HKCU64. }
  Result:=Exec(ExpandConstant('{sysnative}\reg.exe'),Params,'',SW_HIDE,ewWaitUntilTerminated,Code);
  Result:=Result and (Code=0);
end;
function RegistryName: String;
begin
#ifdef TestRoot
  Result:='HKCU\'+MainKey;
#else
  Result:='HKLM\'+MainKey;
#endif
end;
procedure CaptureRegistry;
begin
  RegistryWasPresent:=RegKeyExists(RegistryRoot,MainKey);
  if RegistryWasPresent then
    if not RegistryCommand('export "'+RegistryName+'" "'+RecoveryDir+'\registration.reg" /y') then
      RaiseException('Cannot back up installation registration');
  RegistryCaptured:=True;
end;
function RegistryMatches: Boolean;
begin
  Result:=RegKeyExists(RegistryRoot,MainKey)=RegistryWasPresent;
  if Result and RegistryWasPresent then begin
    Result:=RegistryCommand('export "'+RegistryName+'" "'+RecoveryDir+'\registration-check.reg" /y');
    if Result then Result:=GetSHA256OfFile(RecoveryDir+'\registration-check.reg')=GetSHA256OfFile(RecoveryDir+'\registration.reg');
  end;
end;
procedure RememberAppSupport;
var Find: TFindRec; I: Integer; Base: String;
begin
  RememberFile(AppFolder+'\library_manager.exe');
  RememberFile(AppFolder+'\HarmonyContinuation-Setup.exe');
  RememberFile(AppFolder+'\LibraryUpdate.exe');
  if FindFirst(ExpandConstant('{tmp}\*.dll'),Find) then begin
    try repeat RememberFile(AppFolder+'\'+Find.Name); until not FindNext(Find);
    finally FindClose(Find); end;
  end;
  { Native Inno can allocate any free uninsNNN name. Capture existing ones;
    remember absence of the conventional initial name for a fresh install. }
  for I:=0 to 2 do begin
    if I=0 then Base:='exe' else if I=1 then Base:='dat' else Base:='msg';
    RememberFile(AppFolder+'\unins000.'+Base);
    if FindFirst(AppFolder+'\unins???.'+Base,Find) then begin
      try repeat RememberFile(AppFolder+'\'+Find.Name); until not FindNext(Find);
      finally FindClose(Find); end;
    end;
  end;
#ifndef TestRoot
  RememberFile(ExpandConstant('{autoprograms}\HarmonyContinuation\管理安装.lnk'));
  RememberFile(ExpandConstant('{autoprograms}\HarmonyContinuation\卸载 HarmonyContinuation.lnk'));
  RememberFile(ExpandConstant('{autoprograms}\HarmonyContinuation\更新进行库.lnk'));
#endif
end;
procedure SaveRecoveryJournal;
var Journal: TArrayOfString; I: Integer;
begin
  SetArrayLength(Journal,Snapshots.Count+3);
  Journal[0]:='Build version: {#ProductVersion}'; Journal[1]:='Registry: '+RegistryName;
  Journal[2]:='Registry existed: '+IntToStr(Ord(RegistryWasPresent));
  for I:=0 to Snapshots.Count-1 do
    Journal[I+3]:=IntToStr(I)+'|'+SnapshotPresent[I]+'|'+SnapshotHashes[I]+'|'+Snapshots[I];
  if not SaveStringsToUTF8File(RecoveryDir+'\files.txt',Journal,False) then
    RaiseException('Cannot save recovery journal');
end;
procedure CleanupRecovery;
var I: Integer;
begin
  if not RecoveryCreated then Exit;
  { Only files created by this transaction; never recursively clear a data tree. }
  for I:=0 to Snapshots.Count-1 do DeleteFile(RecoveryDir+'\'+IntToStr(I));
  DeleteFile(RecoveryDir+'\registration.reg'); DeleteFile(RecoveryDir+'\registration-check.reg');
  DeleteFile(RecoveryDir+'\files.txt'); RemoveDir(RecoveryDir); RemoveDir(RecoveryRoot);
  RecoveryCreated:=False;
end;
procedure Rollback;
var I: Integer; Path, Backup: String; Ok, ItemOk: Boolean; Find: TFindRec;
begin
  if not TransactionStarted then Exit;
  Ok:=True;
  { After native rollback has finished, undo app/plugin payload before publishing
    restored component state and the Factory active pointer (captured first). }
  for I:=Snapshots.Count-1 downto 0 do begin
    Path:=Snapshots[I]; Backup:=RecoveryDir+'\'+IntToStr(I); ItemOk:=False;
    try
      if not SafePath(Path) then RaiseException('Unsafe recovery target');
      if SnapshotPresent[I]='1' then begin
        if FileExists(Path) then ItemOk:=GetSHA256OfFile(Path)=SnapshotHashes[I];
        if not ItemOk then begin
          if not ForceDirectories(ExtractFileDir(Path)) or not FileCopy(Backup,Path,False) then
            RaiseException('Cannot restore file');
          ItemOk:=GetSHA256OfFile(Path)=SnapshotHashes[I];
        end;
      end else if SnapshotPresent[I]='D' then ItemOk:=DirExists(Path)
      else begin
        if FileExists(Path) then DeleteFile(Path);
        ItemOk:=not FileExists(Path) and not DirExists(Path);
      end;
    except Log('HC restore error: '+Path+': '+GetExceptionMessage); end;
    if not ItemOk then begin Ok:=False; Log('HC RESTORE FAILED: '+Path); end;
  end;
  { Remove only new native uninstaller files; every pre-existing name was captured. }
  if RegistryCaptured and FindFirst(AppFolder+'\unins???.*',Find) then begin
    try repeat
      Path:=AppFolder+'\'+Find.Name;
      if (Snapshots.IndexOf(Path)<0) and
         (Length(Find.Name)=12) and (Pos(Copy(Find.Name,6,1),'0123456789')>0) and
         (Pos(Copy(Find.Name,7,1),'0123456789')>0) and (Pos(Copy(Find.Name,8,1),'0123456789')>0) and
         ((CompareText(ExtractFileExt(Path),'.exe')=0) or (CompareText(ExtractFileExt(Path),'.dat')=0) or
          (CompareText(ExtractFileExt(Path),'.msg')=0)) then
        if not SafePath(Path) or not DeleteFile(Path) then Ok:=False;
    until not FindNext(Find);
    finally FindClose(Find); end;
  end;
  if RegistryCaptured then begin
    try
      if not RegistryMatches then begin
        if RegKeyExists(RegistryRoot,MainKey) then
          if not RegDeleteKeyIncludingSubkeys(RegistryRoot,MainKey) then RaiseException('Cannot remove failed registration');
        if RegistryWasPresent then
          if not RegistryCommand('import "'+RecoveryDir+'\registration.reg"') then RaiseException('Cannot restore registration');
      end;
      if not RegistryMatches then RaiseException('Restored registration differs');
    except Ok:=False; Log('HC registry restore error: '+GetExceptionMessage); end;
  end;
  TransactionStarted:=False;
  if Ok then begin
    Log('HC rollback verified'); CleanupRecovery;
    if RegistryCaptured and not AppWasPresent then RemoveDir(AppFolder);
  end
  else begin
    RecoveryFailed:=True; CriticalFailure:=True;
    SaveStringToFile(RecoveryDir+'\FAILURE.txt',FailureMessage+#13#10+'Recovery incomplete. Preserve these files.',False);
    SaveStringToFile(RecoveryRoot+'\pending.txt',RecoveryDir,False);
    Log('HC recovery incomplete; retained at '+RecoveryDir);
  end;
end;
procedure TestFailure(Point: String);
begin
#ifdef TestRoot
  if ExpandConstant('{param:HCFAIL|}')=Point then RaiseException('Injected transaction failure: '+Point);
#endif
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
var AnnotationFile: String;
begin
  AnnotationFile:=ManagedAnnotationFile;
  if (AnnotationFile<>'') and FileExists(AnnotationFile) then begin
    if (ManagedAnnotationHash='') or (CompareText(GetSHA256OfFile(AnnotationFile),ManagedAnnotationHash)<>0) then
      RaiseException('Factory annotation ownership/content differs; preserved');
    if not DeleteFile(AnnotationFile) then RaiseException('Cannot remove managed Factory annotation');
  end;
  if FileExists(LibraryDatabase) and not DeleteFile(ManagedLibraryDatabase) then RaiseException('Cannot remove managed Factory DB');
  if CompareText(StoreFolder+'\'+ReadActive+'\factory.db',ManagedLibraryDatabase)=0 then
    if not DeleteFile(StoreFolder+'\active.txt') then RaiseException('Cannot clear active Factory pointer');
  RemoveDir(ExtractFileDir(ManagedLibraryDatabase)); RemoveDir(StoreFolder);
end;
function BundledFactoryPath: String;
begin Result:=PluginFolder('')+'\Contents\Resources\factory.db'; end;
function KnownBundledFactory: Boolean;
var Path, Hash: String;
begin
  Path:=BundledFactoryPath; Result:=True; BundledFactoryHash:='';
  if not FileExists(Path) then Exit;
  if not SafePath(Path) or FileExists(Path+'-wal') or FileExists(Path+'-journal') or FileExists(Path+'-shm') then begin Result:=False; Exit; end;
  Hash:=GetSHA256OfFile(Path); BundledFactoryHash:=Hash;
  Log('Bundled Factory path='+Path+' SHA256='+Hash);
  // Shipped RC1 artifact, corroborated by original release checksum manifest
  // and preserved installed backup; metadata/version is never sufficient.
  Result:=(CompareText(Hash,'9b1fae16302d13a97e1f694ad7147be08920078e6c16057586aaa34e28fbe076')=0) or
    Manager('verify-factory "'+Path+'" "'+ExpandConstant('{tmp}\official-factory')+'"',True);
end;
function UnknownBundledFactoryMessage: String;
var Details: AnsiString; Report: String;
begin
  Report:=ExpandConstant('{tmp}\factory-conflict.txt'); Details:='Unknown';
  if Manager('describe-factory "'+BundledFactoryPath+'" "'+Report+'"',True) then LoadStringFromFile(Report,Details);
  Result:=Text('检测到无法确认来源的旧版 Factory 曲库文件。为保护现有数据，安装已暂停。',
    'An old Factory file of unverified origin was detected. Installation is paused to protect existing data.')+#13#10+
    Text('冲突路径：','Conflict path: ')+BundledFactoryPath+#13#10+
    Text('实际版本信息（不代表来源已确认）：','Reported version (not proof of origin): ')+String(Details)+#13#10+
    'SHA-256: '+BundledFactoryHash+#13#10+
    Text('原因：完整内容与随包官方参考库不符，或路径/日志文件无法安全验证。不会覆盖、删除或接管该文件。',
      'Reason: full content does not match official references, or linked paths/journal files prevent safe verification. The file will not be overwritten, deleted or adopted.')+#13#10+
    Text('请关闭宿主，将该文件及其日志完整备份到独立目录并核对备份哈希，再手动移走冲突文件后重试；无法确认备份可恢复时请保留原状。不要删除 User Library、收藏、设置或历史数据库。',
      'Close hosts, back up this file and any journals to a separate directory and verify backup hashes before manually moving the conflict and retrying. Keep the original if recovery cannot be assured. Do not delete User Library, favourites, settings or historical databases.');
end;
procedure BackupBundledFactory;
var Path, Backup, Hash: String;
begin
  Path:=BundledFactoryPath;
  if not FileExists(Path) then begin
    if BundledFactoryHash<>'' then RaiseException('Factory changed after verification; retry.');
    Exit;
  end;
  Hash:=GetSHA256OfFile(Path);
  if (BundledFactoryHash='') or (CompareText(Hash,BundledFactoryHash)<>0) or
      FileExists(Path+'-wal') or FileExists(Path+'-journal') or FileExists(Path+'-shm') then
    RaiseException('Factory changed after verification; preserved.');
  Backup:=ExtractFileDir(ExtractFileDir(StoreFolder))+'\Backups\FactoryMigration\'+Hash+'\factory.db';
  if not SafePath(Backup) then RaiseException('Unsafe Factory backup path; preserved.');
  if not ForceDirectories(ExtractFileDir(Backup)) then RaiseException('Cannot create permanent Factory backup; preserved.');
  if not FileExists(Backup) then
    if not FileCopy(Path,Backup,True) then RaiseException('Cannot create permanent Factory backup; preserved.');
  if CompareText(GetSHA256OfFile(Backup),Hash)<>0 then RaiseException('Factory backup verification failed; original preserved.');
  Log('Verified permanent Factory backup: '+Backup+' SHA256='+Hash);
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
  Snapshots:=TStringList.Create; SnapshotPresent:=TStringList.Create; SnapshotHashes:=TStringList.Create;
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
function KnownV6Increment: Boolean;
begin
  { Only the verified dev.4 pair may be superseded under the same content series.
    Unknown or modified Factory content still fails the normal ownership gates. }
  Result:=False;
#if LibraryVersion == "6"
  Result:=OwnLibrary and (ManagedHash='3f6a17bb4e696a47462b0ffb8535ab76af3ec0ece5f6a88db855cbcc53707bf9') and
    (ManagedAnnotationHash='d3c3db46eed083d74065a526d4c2f5f45175b3f473a3b3dc1a432b1d1389c90a') and
    (CompareText(ManagedLibraryDatabase,LibraryDatabase)=0) and
    FileExists(LibraryDatabase) and FileExists(StoreFolder+'\6\factory.hca');
  if Result then Result:=(CompareText(GetSHA256OfFile(LibraryDatabase),ManagedHash)=0) and
    (CompareText(GetSHA256OfFile(StoreFolder+'\6\factory.hca'),ManagedAnnotationHash)=0);
#endif
end;
procedure PreserveV6IncrementBase;
var Folder, Db, Hca: String;
begin
  if not KnownV6Increment then Exit;
  Folder:=StoreFolder+'\history\6-'+ManagedHash;
  if not SafePath(Folder) then RaiseException('Unsafe Factory archive path; preserved');
  Db:=Folder+'\factory.db'; Hca:=Folder+'\factory.hca';
  if not ForceDirectories(Folder) then RaiseException('Cannot preserve previous Factory V6');
  if not FileExists(Db) then
    if not FileCopy(LibraryDatabase,Db,True) then RaiseException('Cannot archive previous Factory V6 DB');
  if not FileExists(Hca) then
    if not FileCopy(StoreFolder+'\6\factory.hca',Hca,True) then RaiseException('Cannot archive previous Factory V6 annotation');
  if (CompareText(GetSHA256OfFile(Db),ManagedHash)<>0) or
     (CompareText(GetSHA256OfFile(Hca),ManagedAnnotationHash)<>0) then
    RaiseException('Previous Factory V6 archive identity differs; no replacement');
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
var Code, Active: Integer; Path, Hash: String;
begin
  Result:='';
  if HasPendingRecovery then begin
    Result:='Previous recovery incomplete. Preserve recovery materials at '+RecoveryRoot+'; review before retrying.'; Exit;
  end;
  if not SafePath(AppFolder) or not SafePath(PluginFolder('')) or not SafePath(StoreFolder) then begin Result:='Linked installation/data paths are not managed.'; Exit; end;
  ExtractPayload;
  if not Manager('check-plugin "'+PluginBinary+'"',True) or
     not Manager('check-plugin "'+LibraryDatabase+'"',True) then begin Result:=Text('请关闭 Cubase、FL Studio 或其它占用插件/曲库的程序后重试。','Close Cubase, FL Studio or any process using the plugin/library, then retry.'); Exit; end;
  if RemovePlugin and (not OwnPlugin or not ManifestPaths(AppFolder+'\plugin-files.txt',True)) then begin Result:='Plugin ownership cannot be verified. Repair/update the plugin first; no files removed.'; Exit; end;
  if RemoveLibrary and (not OwnLibrary or ((FileExists(ManagedLibraryDatabase)) and (CompareText(GetSHA256OfFile(ManagedLibraryDatabase),ManagedHash)<>0))) then begin Result:='Factory ownership/content differs. Repair the managed library first; no files removed.'; Exit; end;
  if RemoveLibrary and (ManagedAnnotationFile<>'') and FileExists(ManagedAnnotationFile) and
      ((ManagedAnnotationHash='') or (CompareText(GetSHA256OfFile(ManagedAnnotationFile),ManagedAnnotationHash)<>0)) then begin Result:='Factory annotation ownership/content differs. Repair the managed library first; no files removed.'; Exit; end;
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
  if (Selection('plugin') or RemoveLibrary) and not Manager('check-plugin '+Chr(34)+BundledFactoryPath+Chr(34),True) then begin Result:=Text('请关闭占用旧版 Factory 文件的程序后重试。','Close applications using the old bundled Factory and retry.'); Exit; end;
  if (Selection('plugin') or RemoveLibrary) and not KnownBundledFactory then begin Result:=UnknownBundledFactoryMessage; Log(Result); Exit; end;
  if Selection('library') and not SkipLibrary and FileExists(LibraryDatabase) then begin
    Hash:=GetSHA256OfFile(LibraryDatabase);
    if (CompareText(Hash,'{#FactoryHash}')<>0) and not KnownV6Increment and not ((OperationPage.SelectedValueIndex=1) and OwnLibrary and (CompareText(ManagedHash,'{#FactoryHash}')=0)) then begin
      Result:='Same library version has different content; no overwrite. Repair a known managed component or retain it.'; Exit;
    end;
  end;
  if Selection('library') and not SkipLibrary and ('{#LibraryVersion}'='6') and FileExists(StoreFolder+'\6\factory.hca') and
      (CompareText(GetSHA256OfFile(StoreFolder+'\6\factory.hca'),'{#AnnotationHash}')<>0) and
      not KnownV6Increment and
      not ((OperationPage.SelectedValueIndex=1) and OwnLibrary and (CompareText(ManagedAnnotationHash,'{#AnnotationHash}')=0)) then begin
    Result:='Same library version has different annotation content; no overwrite. Repair a known managed component or retain it.'; Exit;
  end;
  if LegacyUninstaller<>'' then begin
    if not Exec(LegacyUninstaller,'/VERYSILENT /SUPPRESSMSGBOXES /NORESTART',ExtractFileDir(LegacyUninstaller),SW_HIDE,ewWaitUntilTerminated,Code) or (Code<>0) then begin Result:='Legacy library uninstall failed; no new files installed.'; Exit; end;
    if RegKeyExists(RegistryRoot,LibraryKey) then begin Result:='Legacy record remains; no forced registry cleanup performed.'; Exit; end;
    LegacyUninstaller:='';
  end;
end;
procedure CommitComponentState;
begin
  if not ForceDirectories(AppFolder) then RaiseException('Cannot create component state directory');
  if Selection('plugin') then begin
    if not FileCopy(ExpandConstant('{tmp}\plugin-files.txt'),AppFolder+'\plugin-files.txt',False) then RaiseException('Cannot record plugin ownership');
    OwnPlugin:=True;
  end;
  TestFailure('after-manifest');
  if RemovePlugin then OwnPlugin:=False;
  if Selection('library') and not SkipLibrary then OwnLibrary:=True;
  if RemoveLibrary then OwnLibrary:=False;
  if not SetIniString('Components','Plugin',IntToStr(Ord(OwnPlugin)),StateFile) or
     not SetIniString('Components','Library',IntToStr(Ord(OwnLibrary)),StateFile) then RaiseException('Cannot record component state');
  if Selection('library') and not SkipLibrary then begin
    if not SetIniString('Components','LibraryHash','{#FactoryHash}',StateFile) or
       not SetIniString('Components','LibraryVersion','{#LibraryVersion}',StateFile) then RaiseException('Cannot record Factory identity');
#if LibraryVersion == "6"
    if not SetIniString('Components','AnnotationHash','{#AnnotationHash}',StateFile) then RaiseException('Cannot record Factory annotation identity');
#endif
  end;
  if (GetIniString('Components','Plugin','',StateFile)<>IntToStr(Ord(OwnPlugin))) or
     (GetIniString('Components','Library','',StateFile)<>IntToStr(Ord(OwnLibrary))) then
    RaiseException('Component state verification failed');
#if LibraryVersion == "6"
  if Selection('library') and not SkipLibrary and (ManagedAnnotationHash<>'{#AnnotationHash}') then
    RaiseException('Factory annotation state verification failed');
#endif
end;
procedure VerifyInstalledState;
var Version: String; Find: TFindRec; Path: String; Lines: TArrayOfString; I: Integer;
begin
  if Selection('plugin') then begin
    if not ManifestPaths(ExpandConstant('{tmp}\plugin-files.txt'),True) or
       not LoadStringsFromFile(ExpandConstant('{tmp}\plugin-files.txt'),Lines) then RaiseException('Installed plugin verification failed');
    for I:=0 to GetArrayLength(Lines)-1 do
      if not FileExists(PluginFolder('')+'\'+Copy(Lines[I],66,Length(Lines[I]))) then
        RaiseException('Installed plugin file missing');
  end;
  if GetSHA256OfFile(AppFolder+'\library_manager.exe')<>GetSHA256OfFile(ExpandConstant('{tmp}\library_manager.exe')) then
    RaiseException('Installed support tool verification failed');
  if FindFirst(ExpandConstant('{tmp}\*.dll'),Find) then begin
    try repeat
      Path:=AppFolder+'\'+Find.Name;
      if not FileExists(Path) or (GetSHA256OfFile(Path)<>GetSHA256OfFile(ExpandConstant('{tmp}\')+Find.Name)) then
        RaiseException('Installed runtime verification failed: '+Path);
    until not FindNext(Find);
    finally FindClose(Find); end;
  end;
  if GetSHA256OfFile(AppFolder+'\HarmonyContinuation-Setup.exe')<>GetSHA256OfFile(ExpandConstant('{srcexe}')) then
    RaiseException('Installed maintenance Setup verification failed');
  if Selection('library') and not SkipLibrary then
    if (ReadActive<>'{#LibraryVersion}') or (CompareText(GetSHA256OfFile(LibraryDatabase),'{#FactoryHash}')<>0) then
      RaiseException('Installed Factory verification failed');
  if Selection('library') and not SkipLibrary and ('{#LibraryVersion}'='6') then
    if not FileExists(StoreFolder+'\6\factory.hca') or (CompareText(GetSHA256OfFile(StoreFolder+'\6\factory.hca'),'{#AnnotationHash}')<>0) then
      RaiseException('Installed Factory annotation verification failed');
  if not RegQueryStringValue(RegistryRoot,MainKey,'DisplayVersion',Version) or (Version<>'{#ProductVersion}') then
    RaiseException('Installed registration verification failed');
end;
procedure CurStepChanged(CurStep: TSetupStep);
var Path: String;
begin
  if CurStep=ssInstall then begin
    try
      AppWasPresent:=DirExists(AppFolder); BeginRecovery; CaptureRegistry;
      RememberFile(StoreFolder+'\active.txt');
      RememberFile(StateFile); RememberFile(AppFolder+'\plugin-files.txt');
      if Selection('library') and not SkipLibrary then RememberFile(LibraryDatabase);
      if Selection('library') and not SkipLibrary and ('{#LibraryVersion}'='6') then RememberFile(StoreFolder+'\6\factory.hca');
      if RemoveLibrary then RememberFile(ManagedLibraryDatabase);
      if RemoveLibrary and (ManagedAnnotationFile<>'') then RememberFile(ManagedAnnotationFile);
      if Selection('plugin') then RememberManifest(ExpandConstant('{tmp}\plugin-files.txt'));
      if RemovePlugin then RememberManifest(AppFolder+'\plugin-files.txt');
      if Selection('plugin') or RemoveLibrary then RememberFile(BundledFactoryPath);
      RememberAppSupport; SaveRecoveryJournal;
      TransactionStarted:=True;
      if RemovePlugin then RemoveManagedPlugin;
      if RemoveLibrary then RemoveManagedLibrary;
      Path:=PluginFolder('')+'\Contents\Resources\factory.db';
      if (Selection('plugin') or RemoveLibrary) and FileExists(Path) then
        begin
          BackupBundledFactory;
          if not DeleteFile(Path) then RaiseException('Cannot remove known legacy bundled Factory copy');
        end;
      if Selection('library') and not SkipLibrary then begin
        PreserveV6IncrementBase;
        if FileExists(LibraryDatabase) and (CompareText(GetSHA256OfFile(LibraryDatabase),'{#FactoryHash}')<>0) then
          if not DeleteFile(LibraryDatabase) then RaiseException('Cannot replace damaged managed Factory');
        if '{#LibraryVersion}'='6' then begin
          Path:=StoreFolder+'\6\factory.hca';
          if not ForceDirectories(ExtractFileDir(Path)) or not FileCopy(ExpandConstant('{tmp}\factory.hca'),Path,False) then
            RaiseException('Factory annotation install failed; restoring previous data.');
          if CompareText(GetSHA256OfFile(Path),'{#AnnotationHash}')<>0 then RaiseException('Factory annotation verification failed; restoring previous data.');
        end;
        if not Manager('install "'+ExpandConstant('{tmp}\factory.db')+'" "'+StoreFolder+'"',True) then RaiseException('Factory install failed; restoring previous data.');
      end;
      TestFailure('after-factory');
      { All fallible custom writes precede native file copying and registration. }
      CommitComponentState;
    except
      FailureMessage:=GetExceptionMessage; CriticalFailure:=True;
      Log('HC transaction failed: '+FailureMessage);
      if not TransactionStarted then CleanupRecovery;
      SuppressibleMsgBox(Text('安装失败，将恢复原状态。请保留安装日志。','Installation failed; restoring the previous state. Preserve the Setup log.')+#13#10+FailureMessage,mbError,MB_OK,IDOK);
      Abort;
    end;
  end;
  if CurStep=ssPostInstall then begin
    try
      TestFailure('after-registration'); VerifyInstalledState;
      Committed:=True; TransactionStarted:=False;
      Log('HC transaction committed and verified'); CleanupRecovery;
    except
      FailureMessage:=GetExceptionMessage; CriticalFailure:=True;
      Log('HC transaction failed: '+FailureMessage);
      { Native Inno has committed at this point. Restore our complete snapshot
        here and override the otherwise-success exit code explicitly. }
      Rollback;
      if not WizardSilent then MsgBox(FailureMessage+#13#10+'Installation failed; see Setup log.',mbError,MB_OK);
    end;
  end;
end;
function GetCustomSetupExitCode: Integer;
begin
  Result:=0;
  if CriticalFailure then Result:=201;
  if RecoveryFailed then Result:=202;
  Log('HC custom exit code='+IntToStr(Result));
end;
procedure DeinitializeSetup;
begin
  if not Committed then Rollback;
  if Snapshots<>nil then Snapshots.Free;
  if SnapshotPresent<>nil then SnapshotPresent.Free;
  if SnapshotHashes<>nil then SnapshotHashes.Free;
end;
function InitializeUninstall: Boolean;
begin
  Snapshots:=TStringList.Create; SnapshotPresent:=TStringList.Create; SnapshotHashes:=TStringList.Create;
  OwnPlugin:=GetIniString('Components','Plugin','0',StateFile)='1';
  OwnLibrary:=GetIniString('Components','Library','0',StateFile)='1';
  Result:=not HasPendingRecovery and SafePath(AppFolder) and SafePath(PluginFolder('')) and SafePath(StoreFolder);
  if Result and OwnPlugin then Result:=ManifestPaths(AppFolder+'\plugin-files.txt',True);
  if Result and OwnLibrary and FileExists(ManagedLibraryDatabase) then Result:=CompareText(GetSHA256OfFile(ManagedLibraryDatabase),ManagedHash)=0;
  if Result then Result:=Manager('check-plugin "'+PluginBinary+'"',False) and Manager('check-plugin "'+LibraryDatabase+'"',False);
  if not Result then SuppressibleMsgBox(Text('请关闭宿主。若安装文件被修改，请先修复；未删除文件或个人数据。','Close hosts; repair modified managed files before removal. No personal data was deleted.'),mbError,MB_OK,IDOK);
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep=usUninstall then begin
    BeginRecovery;
    TransactionStarted:=True;
    RememberFile(StoreFolder+'\active.txt');
    if OwnPlugin then RememberManifest(AppFolder+'\plugin-files.txt');
    if OwnLibrary then RememberFile(ManagedLibraryDatabase);
    if OwnLibrary and (ManagedAnnotationFile<>'') and FileExists(ManagedAnnotationFile) then begin
      if (ManagedAnnotationHash='') or
          (CompareText(GetSHA256OfFile(ManagedAnnotationFile),ManagedAnnotationHash)<>0) then begin
        Rollback; RaiseException('Factory annotation ownership/content differs; no files removed.');
      end;
      RememberFile(ManagedAnnotationFile);
    end;
    try
      if OwnPlugin then RemoveManagedPlugin;
      if OwnLibrary then RemoveManagedLibrary;
      TransactionStarted:=False; CleanupRecovery;
    except Rollback; RaiseException(GetExceptionMessage); end;
  end;
end;
procedure DeinitializeUninstall;
begin
  Rollback;
  if Snapshots<>nil then Snapshots.Free;
  if SnapshotPresent<>nil then SnapshotPresent.Free;
  if SnapshotHashes<>nil then SnapshotHashes.Free;
end;
