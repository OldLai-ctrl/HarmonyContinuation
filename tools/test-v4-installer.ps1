param([string]$PackageDirectory='build-installer/v4-0.10.0-dev.1',
      [string]$TestRoot='build-installer/v4-product-smoke')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$root=[IO.Path]::GetFullPath((Join-Path $repo $TestRoot))
if (!$root.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $root)) { throw 'Use a fresh workspace-owned isolated root.' }
$reg='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/HarmonyContinuation-Application-IsolatedTest_is1'
if (Test-Path $reg) { throw 'Existing isolated registration must be retained; choose a clean test environment.' }
$stage=Join-Path $repo "$PackageDirectory/stage-4"
$oldStage=Join-Path $repo 'build-installer/release-0.9.0/stage-3'
$factoryHash=(Get-FileHash -LiteralPath (Join-Path $stage 'factory.db')).Hash
New-Item -ItemType Directory -Path $root | Out-Null
$compile=@('/Qp',"/DStageDir=$stage","/DOutputPath=$root",'/DProductVersion=0.10.0-dev.1',
    '/DLibraryVersion=4','/DFactoryEntryCount=657',"/DFactoryHash=$factoryHash",'/DLegacyFactoryHash=',"/DTestRoot=$root")
& (Join-Path $repo 'build-installer/tools/Inno/ISCC.exe') @compile (Join-Path $repo 'packaging/UnifiedSetup.iss') *> (Join-Path $root 'compile.log')
if ($LASTEXITCODE -ne 0) { throw 'Isolated installer compilation failed.' }
$setup=Join-Path $root 'HarmonyContinuation-Setup-IsolatedTest.exe'
$app=Join-Path $root 'HarmonyContinuation'
$bundle=Join-Path $root 'VST3/HarmonyContinuation.vst3'
$binary=Join-Path $bundle 'Contents/x86_64-win/HarmonyContinuation.vst3'
$store=Join-Path $root 'Data/Libraries/Factory'
$active=Join-Path $store 'active.txt'
$db4=Join-Path $store '4/factory.db'
$db3=Join-Path $store '3/factory.db'
$script:checks=0
function Check([bool]$ok,[string]$message) { if (!$ok) { throw $message }; $script:checks++ }
function Run([string]$exe,[string]$name,[string[]]$extra=@(),[bool]$success=$true) {
    $log=Join-Path $root ($name+'.log')
    $hcArgs=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LANG=english',('/LOG="'+$log+'"'))+$extra
    $p=Start-Process -FilePath $exe -ArgumentList $hcArgs -WindowStyle Hidden -Wait -PassThru
    Check (($p.ExitCode -eq 0) -eq $success) "Unexpected exit $($p.ExitCode): $name"
    if (!$success -and $name -match 'guard') { Check ((Get-Content -LiteralPath $log -Raw).Contains('requires plugin 0.10.0-dev.1')) 'Compatibility rejection must explain the required upgrade.' }
}
# These fixtures and all executable installers are redirected to TestRoot/HKCU.
# No production installation, LocalAppData or published package is modified.
Run $setup 'guard-no-plugin' @('/COMPONENTS=library') $false
Check (!(Test-Path $db4)) 'Data-only installation unexpectedly allowed without engine.'
New-Item -ItemType Directory -Force (Split-Path $bundle),(Split-Path $db3) | Out-Null
Copy-Item -LiteralPath (Join-Path $oldStage 'HarmonyContinuation.vst3') -Destination $bundle -Recurse
Copy-Item -LiteralPath (Join-Path $oldStage 'factory.db') -Destination $db3
[IO.File]::WriteAllText($active,"3`n")
# Register only the isolated legacy fixture so Detect can apply its existing
# ownership/location policy before checking the actual old module version.
New-Item -ItemType Directory -Force $app | Out-Null
New-Item -Path $reg -Force | Out-Null
New-ItemProperty -Path $reg -Name 'Inno Setup: App Path' -Value $app -PropertyType String -Force | Out-Null
$oldPluginHash=(Get-FileHash -LiteralPath $binary).Hash
$oldDbHash=(Get-FileHash -LiteralPath $db3).Hash
$personal=Join-Path $root 'Data/User/user.db';$settings=Join-Path $root 'Data/User/settings.keep'
New-Item -ItemType Directory -Force (Split-Path $personal) | Out-Null
[IO.File]::WriteAllText($personal,'Personal progressions and favourites sentinel')
[IO.File]::WriteAllText($settings,'Personal settings sentinel')
$userHash=(Get-FileHash -LiteralPath $personal).Hash
$settingsHash=(Get-FileHash -LiteralPath $settings).Hash
Run $setup 'guard-v0.9.0' @('/COMPONENTS=library') $false
Check ((Get-FileHash -LiteralPath $binary).Hash -eq $oldPluginHash -and !(Test-Path $db4) -and (Get-Content $active -Raw).Trim() -eq '3') 'Rejected V4 update changed old plugin or Factory.'
Run $setup 'plugin-only' @('/COMPONENTS=plugin')
Check ((Get-Content $active -Raw).Trim() -eq '3' -and (Get-FileHash -LiteralPath $db3).Hash -eq $oldDbHash -and !(Test-Path $db4)) 'Plugin-only update changed installed Factory.'
$pluginHash=(Get-FileHash -LiteralPath $binary).Hash
Check ($pluginHash -ne $oldPluginHash) 'Plugin fixture was not upgraded.'
Run $setup 'add-v4-compatible' @('/COMPONENTS=library')
Check ((Get-Content $active -Raw).Trim() -eq '4' -and (Get-FileHash -LiteralPath $db4).Hash -eq $factoryHash -and (Get-FileHash -LiteralPath $binary).Hash -eq $pluginHash) 'Compatible library-only update failed or changed plugin.'
$lock=[IO.File]::Open($binary,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try { Run $setup 'locked-update' @('/COMPONENTS=plugin,library') $false } finally { $lock.Dispose() }
[IO.File]::WriteAllText($db4,'Damaged managed Factory fixture')
Run $setup 'repair-both' @('/OPERATION=repair','/COMPONENTS=plugin,library')
Check ((Get-FileHash -LiteralPath $db4).Hash -eq $factoryHash -and (Get-FileHash -LiteralPath $binary).Hash -eq $pluginHash) 'Repair did not restore the matching components.'
Run (Join-Path $app 'unins000.exe') 'uninstall'
Check (!(Test-Path $binary) -and !(Test-Path $db4) -and !(Test-Path $reg)) 'Managed uninstall incomplete.'
Check ((Get-FileHash -LiteralPath $db3).Hash -eq $oldDbHash -and (Get-FileHash -LiteralPath $personal).Hash -eq $userHash -and (Get-FileHash -LiteralPath $settings).Hash -eq $settingsHash) 'Historical Factory or personal sentinels changed.'
Run $setup 'fresh-matching-both' @('/COMPONENTS=plugin,library')
Check ((Get-FileHash -LiteralPath $db4).Hash -eq $factoryHash -and (Get-FileHash -LiteralPath $binary).Hash -eq $pluginHash) 'Installing both matching components failed.'
Run (Join-Path $app 'unins000.exe') 'fresh-uninstall'
Check (!(Test-Path $reg) -and (Get-FileHash -LiteralPath $db3).Hash -eq $oldDbHash -and (Get-FileHash -LiteralPath $personal).Hash -eq $userHash -and (Get-FileHash -LiteralPath $settings).Hash -eq $settingsHash) 'Fresh lifecycle changed historical or personal data.'
"V4 focused installer: $checks checks PASS; absent/0.9.0 guards, plugin-only preservation, matching library-only, lock rejection, repair and uninstall protection." | Tee-Object (Join-Path $root 'result.txt')
