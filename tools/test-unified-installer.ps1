param([string]$PackageDirectory='build-installer/rc-0.9.0-rc.3-unified', [string]$TestRoot='build-installer/unified-smoke', [string]$BuildDirectory='build-v3-plugin')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$root=[IO.Path]::GetFullPath((Join-Path $repo $TestRoot))
if (!$root.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $root)) { throw 'A fresh workspace-owned isolated root is required.' }
$reg='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/HarmonyContinuation-Application-IsolatedTest_is1'
$oldReg='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/HarmonyContinuation-FactoryLibrary-IsolatedTest_is1'
if ((Test-Path $reg) -or (Test-Path $oldReg)) { throw 'An isolated registration already exists; do not overwrite it.' }
New-Item -ItemType Directory -Path $root | Out-Null
$stage=Join-Path $repo "$PackageDirectory/stage-3"
$iscc=Join-Path $repo 'build-installer/tools/Inno/ISCC.exe'
$version=(Select-String -Path (Join-Path $repo "$BuildDirectory/generated/ProductVersionGenerated.h") -Pattern '^#define HC_PRODUCT_VERSION "([^"]+)"').Matches.Groups[1].Value
$factoryHash=(Get-FileHash (Join-Path $stage 'factory.db')).Hash
$legacyHash=(Get-FileHash (Join-Path $repo "$BuildDirectory/factory-v2.db")).Hash
$script:checks=0
function Check([bool]$ok,[string]$message) { if (!$ok) { throw $message }; $script:checks++ }
function Compile([string]$where,[bool]$legacy=$false,[bool]$library=$false) {
 $output=Join-Path $where 'packages'; New-Item -ItemType Directory -Force -Path $output | Out-Null
 $source=Join-Path $repo 'packaging/UnifiedSetup.iss'; $payload=$stage
 if ($legacy) {
  $payload=Join-Path $repo 'build-installer/rc-0.9.0-rc.2/stage-3'
  $source=Join-Path $output 'Legacy.iss'
  # Only the isolated copy gets per-user uninstall records. Git history stays intact.
  $text=(Get-Content (Join-Path $repo 'packaging/HarmonyContinuation.iss') -Raw).Replace('CreateUninstallRegKey=no','CreateUninstallRegKey=yes')
  $text=$text.Replace('InfoBeforeFile=installer-info.txt',('InfoBeforeFile='+$repo+'\packaging\installer-info.txt')).Replace('MessagesFile: "ChineseSimplified.isl"',('MessagesFile: "'+$repo+'\packaging\ChineseSimplified.isl"'))
  [IO.File]::WriteAllText($source,$text,[Text.UTF8Encoding]::new($false))
 }
 $hcArgs=@('/Qp',"/DStageDir=$payload","/DOutputPath=$output","/DProductVersion=$version",'/DLibraryVersion=3',"/DFactoryHash=$factoryHash","/DLegacyFactoryHash=$legacyHash","/DTestRoot=$where")
 if ($library) { $hcArgs+='/DLibraryOnly' }
 & $iscc @hcArgs $source *> (Join-Path $output ('compile-'+$legacy+'-'+$library+'.log'))
 if ($LASTEXITCODE -ne 0) { throw 'Isolated compiler failed; see compile log.' }
 if (!$legacy) { return Join-Path $output 'HarmonyContinuation-Setup-IsolatedTest.exe' }
 if ($library) { return Join-Path $output 'HarmonyContinuation-Library-3-Setup-IsolatedTest.exe' }
 return Join-Path $output "HarmonyContinuation-$version-Setup-IsolatedTest.exe"
}
function Run([string]$exe,[string]$log,[string[]]$extra=@(),[bool]$success=$true) {
 $hcArgs=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LANG=english',('/LOG="'+$log+'"'))+$extra
 $process=Start-Process -FilePath $exe -ArgumentList $hcArgs -WindowStyle Hidden -Wait -PassThru
 Check (($process.ExitCode -eq 0) -eq $success) "Unexpected exit $($process.ExitCode): $log"
}
function Paths([string]$where) {
 return @{app=(Join-Path $where 'HarmonyContinuation');plugin=(Join-Path $where 'VST3/HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3');db=(Join-Path $where 'Data/Libraries/Factory/3/factory.db');personal=(Join-Path $where 'Data/User/user.db')}
}
$fresh=Join-Path $root 'fresh';$setup=Compile $fresh;$p=Paths $fresh
New-Item -ItemType Directory -Force (Split-Path $p.personal) | Out-Null
[IO.File]::WriteAllText($p.personal,'User collection, favourites and preferences: preserve')
$userHash=(Get-FileHash $p.personal).Hash
Run $setup (Join-Path $fresh 'install.log')
Check ((Test-Path $p.plugin) -and (Test-Path $p.db) -and (Test-Path $reg)) 'Fresh installation missing component or registration'
Check (!(Test-Path (Join-Path (Split-Path (Split-Path $p.plugin)) 'Resources/factory.db'))) 'Hidden Factory remains in VST3 component'
$pluginHash=(Get-FileHash $p.plugin).Hash
$foreign=Join-Path $fresh 'VST3/OtherPlugin.keep';[IO.File]::WriteAllText($foreign,'Unrelated VST3 must survive')
$inside=Join-Path $fresh 'VST3/HarmonyContinuation.vst3/personal.keep';[IO.File]::WriteAllText($inside,'Unmanaged file must survive')
Run $setup (Join-Path $fresh 'remove-library.log') @('/OPERATION=modify','/REMOVE=library','/COMPONENTS=""')
Check (!(Test-Path $p.db) -and (Get-FileHash $p.plugin).Hash -eq $pluginHash) 'Removing Factory affected plugin or failed'
Run $setup (Join-Path $fresh 'add-library.log') @('/OPERATION=modify','/COMPONENTS=library')
Check ((Test-Path $p.db) -and (Get-FileHash $p.plugin).Hash -eq $pluginHash) 'Adding Factory affected plugin'
[IO.File]::WriteAllText($p.db,'damaged official Factory fixture')
Run $setup (Join-Path $fresh 'repair-library.log') @('/OPERATION=repair','/COMPONENTS=library')
Check ((Get-FileHash $p.db).Hash -eq $factoryHash -and (Get-FileHash $p.plugin).Hash -eq $pluginHash) 'Repair did not restore only the managed Factory'
Run $setup (Join-Path $fresh 'remove-plugin.log') @('/OPERATION=modify','/REMOVE=plugin','/COMPONENTS=""')
Check (!(Test-Path $p.plugin) -and (Test-Path $p.db)) 'Removing VST3 affected Factory or failed'
Run $setup (Join-Path $fresh 'add-plugin.log') @('/OPERATION=modify','/COMPONENTS=plugin')
Check ((Get-FileHash $p.plugin).Hash -eq $pluginHash -and (Get-FileHash $p.db).Hash -eq $factoryHash) 'Adding VST3 changed Factory'
Run (Join-Path $p.app 'unins000.exe') (Join-Path $fresh 'uninstall.log')
Check (!(Test-Path $p.plugin) -and !(Test-Path $p.db) -and !(Test-Path $reg)) 'Full uninstall left managed components or registration'
Check ((Get-FileHash $p.personal).Hash -eq $userHash -and (Test-Path $foreign) -and (Test-Path $inside)) 'Uninstall touched personal/unmanaged data'
"Fresh and maintenance checks passed: $checks" | Set-Content (Join-Path $root 'fresh-result.txt')
$migration=Join-Path $root 'migration';$p=Paths $migration
$legacyFull=Compile $migration $true $false;$legacyLibrary=Compile $migration $true $true;$setup=Compile $migration
New-Item -ItemType Directory -Force (Split-Path $p.personal),(Join-Path $migration 'Data/Libraries/Factory/2') | Out-Null
[IO.File]::WriteAllText($p.personal,'Legacy user collection unchanged')
$userHash=(Get-FileHash $p.personal).Hash
Copy-Item -LiteralPath (Join-Path $repo "$BuildDirectory/factory-v2.db") -Destination (Join-Path $migration 'Data/Libraries/Factory/2/factory.db')
Run $legacyFull (Join-Path $migration 'legacy-main.log')
Run $legacyLibrary (Join-Path $migration 'legacy-library.log')
Check ((Test-Path $reg) -and (Test-Path $oldReg)) 'Dual legacy registrations not created'
$oldPluginHash=(Get-FileHash $p.plugin).Hash
Run $setup (Join-Path $migration 'library-only-migration.log') @('/COMPONENTS=library')
Check ((Test-Path $reg) -and !(Test-Path $oldReg) -and (Get-FileHash $p.plugin).Hash -eq $oldPluginHash) 'Legacy migration touched unselected plugin or left dual records'
$libraryHash=(Get-FileHash $p.db).Hash
Run $setup (Join-Path $migration 'plugin-only-update.log') @('/COMPONENTS=plugin')
Check ((Get-FileHash $p.plugin).Hash -eq (Get-FileHash (Join-Path $stage 'HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3')).Hash -and (Get-FileHash $p.db).Hash -eq $libraryHash) 'Plugin-only update changed Library'
$lock=[IO.File]::Open($p.plugin,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try { Run $setup (Join-Path $migration 'locked-update.log') @('/COMPONENTS=plugin') $false } finally { $lock.Dispose() }
Check ((Get-FileHash $p.personal).Hash -eq $userHash) 'Migration changed User Library'
Run (Join-Path $p.app 'unins000.exe') (Join-Path $migration 'uninstall.log')
Check (!(Test-Path $reg) -and !(Test-Path $oldReg) -and !(Test-Path $p.plugin) -and !(Test-Path $p.db)) 'Migrated uninstall left registered/managed components'
Check ((Get-FileHash $p.personal).Hash -eq $userHash -and (Test-Path (Join-Path $migration 'Data/Libraries/Factory/2/factory.db'))) 'Historical Factory or personal collection lost'
"Unified isolated lifecycle $checks checks PASS; fresh, dual legacy migration, independent update/remove/repair, lock rejection, uninstall and preserved personal/history/unmanaged files" | Tee-Object (Join-Path $root 'result.txt')
