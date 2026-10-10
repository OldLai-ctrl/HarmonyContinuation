param([string]$Iscc='build-installer/tools/Inno/ISCC.exe',
      [string]$PackageDirectory='build-installer/v4-0.10.0-dev.2',
      [string]$OldPackageDirectory='build-installer/dev2-work/official-v09-package',
      [string]$TestRoot='build-installer/dev2-upgrade-tests',
      [string]$RCV2Factory='build-installer/dev2-work/rc08/factory.db',
      [string[]]$Cases=@('fresh','official-v09','rc-manual','unknown','official-content-layout','official-v09-plugin-only','backup-conflict','legacy-dual'))
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(![IO.Path]::IsPathRooted($Iscc)){$Iscc=Join-Path $repo $Iscc}
if(![IO.Path]::IsPathRooted($RCV2Factory)){$RCV2Factory=Join-Path $repo $RCV2Factory}
$root=[IO.Path]::GetFullPath((Join-Path $repo $TestRoot))
if (!$root.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $root)) {throw 'Use a fresh workspace-owned test root.'}
$stage=Join-Path $repo "$PackageDirectory/stage-4"
$oldStage=Join-Path $repo "$OldPackageDirectory/stage-3"
$oldOriginal=Join-Path $repo 'build-installer/dev2-work/official-v09-source/packaging/UnifiedSetup.iss'
$oldScript=Join-Path (Split-Path $oldOriginal) 'Dev2FixtureSetup.iss'
$oldText=[IO.File]::ReadAllText($oldOriginal).Replace(' #define TestSuffix "-IsolatedTest"'," #ifndef TestSuffix`n  #define TestSuffix `"-IsolatedTest`"`n #endif")
[IO.File]::WriteAllText($oldScript,$oldText,[Text.UTF8Encoding]::new($false))
$manager=Join-Path $stage 'library_manager.exe'
$factoryHash=(Get-FileHash -LiteralPath (Join-Path $stage 'factory.db')).Hash
$oldHash=(Get-FileHash -LiteralPath (Join-Path $oldStage 'factory.db')).Hash
New-Item -ItemType Directory -Path $root | Out-Null
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class HcTestSqlite {
 [DllImport("winsqlite3",CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_open_v2([MarshalAs(UnmanagedType.LPUTF8Str)]string path,out IntPtr db,int flags,IntPtr vfs);
 [DllImport("winsqlite3",CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_exec(IntPtr db,[MarshalAs(UnmanagedType.LPUTF8Str)]string sql,IntPtr cb,IntPtr arg,out IntPtr error);
 [DllImport("winsqlite3",CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_close(IntPtr db);
 public static void Exec(string path,string sql) {IntPtr db; if(sqlite3_open_v2(path,out db,2,IntPtr.Zero)!=0)throw new Exception("open test DB failed");try{IntPtr error;if(sqlite3_exec(db,sql,IntPtr.Zero,IntPtr.Zero,out error)!=0)throw new Exception("test SQL failed");}finally{sqlite3_close(db);}}
}
"@
$script:checks=0
function Check([bool]$ok,[string]$message) {if(!$ok){throw $message};$script:checks++}
function Hash([string]$path) {(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$exe,[string]$name,[string[]]$extra=@(),[bool]$success=$true,[string]$lang='english') {
 $log=Join-Path $script:case ($name+'.log')
 $hcArgs=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LANG=$lang",('/LOG="'+$log+'"'))+$extra
 $p=Start-Process -FilePath $exe -ArgumentList $hcArgs -WindowStyle Hidden -Wait -PassThru
 Check (($p.ExitCode -eq 0) -eq $success) "Unexpected exit $($p.ExitCode): $name"
 return $log
}
function Compile([string]$source,[string]$payload,[string]$version,[string]$library,[string]$stem) {
 $hash=Hash (Join-Path $payload 'factory.db')
 $args=@('/Qp',"/DStageDir=$payload","/DOutputPath=$script:case","/DProductVersion=$version","/DLibraryVersion=$library",('/DFactoryEntryCount='+$(if($library -eq '4'){657}else{629})),"/DFactoryHash=$hash",'/DLegacyFactoryHash=',"/DTestRoot=$script:case","/DTestSuffix=$script:suffix","/DSetupFileStem=$stem")
 & $Iscc @args $source *> (Join-Path $script:case ($stem+'-compile.log'))
 if($LASTEXITCODE -ne 0){throw "Compile failed: $stem"}
 return Join-Path $script:case ($stem+$script:suffix+'.exe')
}
function PersonalIntact {
 foreach($p in $script:sentinels.Keys){Check ((Hash $p) -eq $script:sentinels[$p]) "Personal/historical/unmanaged data changed: $p"}
}
function Matching {
 Check ((Get-Content -LiteralPath $script:active -Raw).Trim() -eq '4') 'Active Factory is not V4'
 Check ((Hash $script:db4) -eq $factoryHash) 'Factory V4 bytes differ'
 $info=& $manager inspect $script:db4
 Check ($LASTEXITCODE -eq 0 -and "$info" -match 'library_version=4 schema=2 progressions=657') 'Wrong Factory version/schema/count'
 Check ((Get-Content -LiteralPath $script:module -Raw) -match '"Version": "0.10.0-dev.2"') 'Plugin version mismatch'
 $state=Get-Content -LiteralPath (Join-Path $script:app 'components.ini') -Raw
 Check ($state -match 'Plugin=1' -and $state -match 'Library=1' -and $state -match 'LibraryVersion=4' -and $state.ToUpperInvariant().Contains($factoryHash)) 'Component ownership record mismatch'
 PersonalIntact
}
foreach($name in $Cases) {
 $script:case=Join-Path $root $name; $script:suffix='-D2'+[Guid]::NewGuid().ToString('N').Substring(0,8)
 $reg='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/HarmonyContinuation-Application'+$script:suffix+'_is1'
 if(Test-Path -LiteralPath $reg){throw "Existing test registration: $reg"}
 New-Item -ItemType Directory -Path $script:case | Out-Null
 $script:app=Join-Path $script:case 'HarmonyContinuation'
 $bundle=Join-Path $script:case 'VST3/HarmonyContinuation.vst3'
 $script:module=Join-Path $bundle 'Contents/Resources/moduleinfo.json'
 $bundled=Join-Path $bundle 'Contents/Resources/factory.db'
 $binary=Join-Path $bundle 'Contents/x86_64-win/HarmonyContinuation.vst3'
 $script:active=Join-Path $script:case 'Data/Libraries/Factory/active.txt'
 $script:db4=Join-Path $script:case 'Data/Libraries/Factory/4/factory.db'
 $db3=Join-Path $script:case 'Data/Libraries/Factory/3/factory.db'
 $script:sentinels=@{}
 foreach($rel in @('Data/User/user.db','Data/User/favourites.keep','Data/User/settings.keep','Data/User/personal-progressions.keep','Data/Libraries/Factory/1/history.keep','VST3/HarmonyContinuation.vst3/unmanaged.keep')) {
  $p=Join-Path $script:case $rel;New-Item -ItemType Directory -Force -Path (Split-Path $p) | Out-Null
  [IO.File]::WriteAllText($p,"$name/$rel personal sentinel");$script:sentinels[$p]=Hash $p
 }
 $new=Compile (Join-Path $repo 'packaging/UnifiedSetup.iss') $stage '0.10.0-dev.2' '4' 'new'
 if($name -eq 'fresh') {
  $null=Run $new 'library-without-engine' @('/COMPONENTS=library') $false
  Check (!(Test-Path -LiteralPath $script:db4)) 'Library-only without engine was allowed'
  $null=Run $new 'fresh-install' @('/COMPONENTS=plugin,library');Matching
 } else {
  $old=Compile $oldScript $oldStage '0.9.0' '3' 'old'
  $null=Run $old 'official-v09-install' @('/COMPONENTS=plugin,library')
  Check ((Hash $db3) -eq $oldHash) 'Official old Factory fixture differs'
  $script:sentinels[$db3]=Hash $db3
  if($name -eq 'rc-manual') {
   Copy-Item -LiteralPath (Join-Path $repo 'build-installer/dev2-work/rc09/{code_PluginFolder}/Contents/Resources/factory.db') -Destination $bundled
   Copy-Item -LiteralPath (Join-Path $repo 'build-installer/dev2-work/rc09/{code_PluginFolder}/Contents/Resources/moduleinfo.json') -Destination $script:module
   Copy-Item -LiteralPath (Join-Path $repo 'build-installer/dev2-work/rc09/{code_PluginFolder}/Contents/x86_64-win/HarmonyContinuation.vst3') -Destination $binary
  }
  if($name -eq 'unknown') {
   Copy-Item -LiteralPath (Join-Path $oldStage 'factory.db') -Destination $bundled
   [HcTestSqlite]::Exec($bundled,"UPDATE progressions SET payload=replace(payload,'Turnaround','UserEdit') WHERE payload LIKE '%Turnaround%'; INSERT INTO metadata VALUES('user_modified','yes');")
   $before=@{}; foreach($p in @($bundled,$binary,$db3,$script:active,(Join-Path $script:app 'components.ini'))){$before[$p]=Hash $p}
   $log=Run $new 'unknown-en' @('/COMPONENTS=plugin,library') $false
   $text=Get-Content -LiteralPath $log -Raw
   Check ($text.Contains($bundled.Replace('/','\')) -and $text.Contains('Library 3 / Schema 2 / 629') -and $text.Contains('full content does not match') -and $text.Contains((Hash $bundled).ToLowerInvariant())) 'English rejection lacks path/version/reason/hash'
   $log=Run $new 'unknown-zh' @('/COMPONENTS=plugin,library') $false 'chinesesimp'
   $text=Get-Content -LiteralPath $log -Raw
   Check ($text.Contains('为保护现有数据，安装已暂停') -and $text.Contains('冲突路径：') -and $text.Contains('核对备份哈希')) 'Chinese rejection lacks safe instructions'
   foreach($p in $before.Keys){Check ((Hash $p) -eq $before[$p]) "Unknown rejection changed $p"};PersonalIntact
   Check (!(Test-Path -LiteralPath $script:db4)) 'Unknown rejection installed V4'
   $script:sentinels.Remove($db3) # Still the old installer's active owned component, not history.
   $null=Run (Join-Path $script:app 'unins000.exe') 'unknown-fixture-cleanup'
   Check ((Hash $bundled) -eq $before[$bundled]) 'Uninstall removed unmanaged unknown Factory'
   PersonalIntact;continue
  }
  if($name -eq 'legacy-dual') {
   $legacyOriginal=Join-Path $repo 'packaging/HarmonyContinuation.iss'
   $legacySource=Join-Path (Split-Path $oldOriginal) 'Dev2LegacyLibraryFixture.iss'
   $legacyText=[IO.File]::ReadAllText($legacyOriginal).Replace('CreateUninstallRegKey=no','CreateUninstallRegKey=yes')
   $legacyText=$legacyText.Replace('  #define TestSuffix "-IsolatedTest"',"  #ifndef TestSuffix`n   #define TestSuffix `"-IsolatedTest`"`n  #endif")
   [IO.File]::WriteAllText($legacySource,$legacyText,[Text.UTF8Encoding]::new($false))
   & $Iscc '/Qp' "/DStageDir=$oldStage" "/DOutputPath=$script:case" '/DProductVersion=0.9.0' '/DLibraryVersion=3' '/DLibraryOnly' "/DTestRoot=$script:case" "/DTestSuffix=$script:suffix" $legacySource *> (Join-Path $script:case 'legacy-library-compile.log')
   if($LASTEXITCODE -ne 0){throw 'Legacy component fixture failed to compile'}
   $legacySetup=Join-Path $script:case ('HarmonyContinuation-Library-3-Setup'+$script:suffix+'.exe')
   $null=Run $legacySetup 'legacy-library-install'
   $legacyReg='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/HarmonyContinuation-FactoryLibrary'+$script:suffix+'_is1'
   Check (Test-Path -LiteralPath $legacyReg) 'Legacy dual registration not created'
  }
  $oldBinaryHash=Hash $binary
  $null=Run $new 'v09-library-only-blocked' @('/COMPONENTS=library') $false
  Check ((Hash $binary) -eq $oldBinaryHash -and !(Test-Path -LiteralPath $script:db4)) 'Compatibility rejection changed engine/data'
  if($name -eq 'official-content-layout') {
   # Byte layout changes must not turn trusted complete content into "unknown".
   Copy-Item -LiteralPath (Join-Path $oldStage 'factory.db') -Destination $bundled
   [HcTestSqlite]::Exec($bundled,'PRAGMA page_size=8192; VACUUM;')
   Check ((Hash $bundled) -ne $oldHash) 'Non-hash identity fixture must have different bytes'
  }
  if($name -eq 'official-v09-plugin-only') {
   $null=Run $new 'v09-plugin-only' @('/COMPONENTS=plugin')
   $record=Get-Content -LiteralPath (Join-Path $script:app 'components.ini') -Raw
   Check ($record.ToUpperInvariant().Contains($oldHash) -and (Hash $db3) -eq $oldHash -and (Get-Content -LiteralPath $script:active -Raw).Trim() -eq '3') 'Plugin-only changed old Factory identity/ownership'
  }
  if($name -eq 'backup-conflict') {
   Copy-Item -LiteralPath (Join-Path $oldStage 'factory.db') -Destination $bundled
   $backup=Join-Path $script:case ('Data/Backups/FactoryMigration/'+$oldHash.ToLowerInvariant()+'/factory.db')
   New-Item -ItemType Directory -Force -Path (Split-Path $backup) | Out-Null
   [IO.File]::WriteAllText($backup,'Unrelated existing backup; must not overwrite')
   $badBackupHash=Hash $backup;$beforeState=Hash (Join-Path $script:app 'components.ini')
   $null=Run $new 'backup-conflict-blocked' @('/COMPONENTS=plugin,library') $false
   Check ((Hash $bundled) -eq $oldHash -and (Hash $backup) -eq $badBackupHash -and (Hash $binary) -eq $oldBinaryHash -and (Hash (Join-Path $script:app 'components.ini')) -eq $beforeState -and !(Test-Path -LiteralPath $script:db4)) 'Backup failure changed original data or engine'
   PersonalIntact
   # Repair only our deliberately damaged test backup, then retry migration.
   Copy-Item -LiteralPath $bundled -Destination $backup -Force
  }
  $migrationHash=if(Test-Path -LiteralPath $bundled){Hash $bundled}else{''}
  $null=Run $new 'upgrade-both' @('/COMPONENTS=plugin,library');Matching
  Check (!(Test-Path -LiteralPath $bundled)) 'Legacy bundled fallback remains active'
  if($name -eq 'legacy-dual'){Check (!(Test-Path -LiteralPath $legacyReg)) 'Old dual component registration remains'}
  if($migrationHash) {
  $backup=Join-Path $script:case ('Data/Backups/FactoryMigration/'+$migrationHash.ToLowerInvariant()+'/factory.db')
  Check ((Hash $backup) -eq $migrationHash) 'Verified permanent migration backup missing'
  $script:sentinels[$backup]=Hash $backup
  }
  if($name -eq 'rc-manual') {
   # Corroborated RC V2, even though its old build has different bytes/content.
   Copy-Item -LiteralPath $RCV2Factory -Destination $bundled
   $rcHash=Hash $bundled
   $null=Run $new 'historical-v2-upgrade' @('/COMPONENTS=plugin,library');Matching
   $backup=Join-Path $script:case ('Data/Backups/FactoryMigration/'+$rcHash.ToLowerInvariant()+'/factory.db')
   Check ((Hash $backup) -eq $rcHash) 'Historical RC V2 backup missing';$script:sentinels[$backup]=Hash $backup
  }
 }
 # An unchecked component is retained, not removed; its ownership stays correct.
 $pluginHash=Hash $binary; $libraryHash=Hash $script:db4
 $null=Run $new 'plugin-only-preserves-library' @('/COMPONENTS=plugin')
 Check ((Hash $script:db4) -eq $libraryHash) 'Unchecked library changed';Matching
 $null=Run $new 'library-only-preserves-plugin' @('/COMPONENTS=library')
 Check ((Hash $binary) -eq $pluginHash) 'Unchecked plugin changed';Matching
 [IO.File]::WriteAllText($script:db4,'Damaged owned V4 fixture')
 $null=Run $new 'repair' @('/OPERATION=repair','/COMPONENTS=plugin,library');Matching
 # Remove library explicitly, then add it back without touching the engine.
 $null=Run $new 'modify-remove-library' @('/OPERATION=modify','/REMOVE=library','/COMPONENTS=plugin')
 Check (!(Test-Path -LiteralPath $script:db4) -and (Hash $binary) -eq $pluginHash) 'Explicit library removal affected engine'
 PersonalIntact
 $null=Run $new 'modify-add-library' @('/OPERATION=modify','/COMPONENTS=library');Matching
 $null=Run (Join-Path $script:app 'unins000.exe') 'uninstall'
 Check (!(Test-Path -LiteralPath $binary) -and !(Test-Path -LiteralPath $script:db4) -and !(Test-Path -LiteralPath $reg)) 'Managed uninstall incomplete'
 PersonalIntact
}
"Dev.2 focused upgrade: $script:checks checks PASS ($($Cases -join ' / '); component maintenance, verified backups and personal protection)." | Tee-Object (Join-Path $root 'result.txt')