param([string]$BuildDirectory = 'build-v08')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$work=Join-Path $repo 'build-installer'
$root=[IO.Path]::GetFullPath((Join-Path $work 'smoke'))
$checks=0
function Check([bool]$ok,[string]$message) {
    if (!$ok) { throw $message }
    $script:checks++
}
function RunSetup([string]$exe,[string]$log,[bool]$success=$true) {
    $args=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+(Join-Path $work $log)+'"'))
    $process=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -Wait -PassThru
    Check (($process.ExitCode -eq 0) -eq $success) "Unexpected installer exit: $($process.ExitCode), $log"
}
# Test packages have a separate AppId, no registry/shortcuts, and compiled-in isolated paths.
# This script never calls production installers or deletes existing data.
Check ($root.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Invalid isolation directory'
if (Test-Path -LiteralPath $root) { throw '隔离目录已存在；请使用新的隔离构建目录，避免覆盖测试数据。' }
$header=Get-Content -LiteralPath (Join-Path $repo "$BuildDirectory/generated/ProductVersionGenerated.h") -Raw
if ($header -notmatch 'HC_PRODUCT_VERSION\s+"([^"]+)"') {throw 'Missing version'}
$version=$Matches[1]
$setup=Join-Path $work "test-output/HarmonyContinuation-$version-Setup-IsolatedTest.exe"
$update=Join-Path $work 'test-update/HarmonyContinuation-Library-3-Setup-IsolatedTest.exe'
$plugin=Join-Path $root 'VST3/HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3'
$store=Join-Path $root 'Data/Libraries/Factory'
RunSetup $setup 'smoke-install.log'
Check (Test-Path -LiteralPath $plugin) 'Plugin missing after install'
Check ((Get-Content -LiteralPath (Join-Path $store 'active.txt')).Trim() -eq '2') 'Initial active library'
$hash=(Get-FileHash -LiteralPath $plugin).Hash
$personal=Join-Path $root 'Data/personal-collection.keep'
[IO.File]::WriteAllText($personal,'Personal collection must survive all maintenance',[Text.UTF8Encoding]::new($false))
$personalHash=(Get-FileHash -LiteralPath $personal).Hash
$lock=[IO.File]::Open($plugin,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try { RunSetup $setup 'smoke-locked-update.log' $false } finally { $lock.Dispose() }
Check ((Get-FileHash -LiteralPath $plugin).Hash -eq $hash) 'Locked update changed plugin'
RunSetup $update 'smoke-library-update.log'
Check ((Get-Content -LiteralPath (Join-Path $store 'active.txt')).Trim() -eq '3') 'Independent update did not activate version 3'
Check ((Get-FileHash -LiteralPath $plugin).Hash -eq $hash) 'Library-only update replaced plugin'
Check (Test-Path -LiteralPath (Join-Path $store '2/factory.db')) 'Old library missing'
RunSetup $setup 'smoke-repair.log'
Check ((Get-Content -LiteralPath (Join-Path $store 'active.txt')).Trim() -eq '3') 'App repair downgraded library'
RunSetup (Join-Path $root 'HarmonyContinuation/unins000.exe') 'smoke-uninstall.log'
Check (!(Test-Path -LiteralPath $plugin)) 'Plugin still present after uninstall'
RunSetup (Join-Path $root 'HarmonyContinuation Library/unins000.exe') 'smoke-library-uninstall.log'
Check ((Get-FileHash -LiteralPath $personal).Hash -eq $personalHash) 'Personal data removed'
Check ((Test-Path -LiteralPath (Join-Path $store '2/factory.db')) -and (Test-Path -LiteralPath (Join-Path $store '3/factory.db'))) 'Library history removed'
Check ((Get-Content -LiteralPath (Join-Path $store 'active.txt')).Trim() -eq '3') 'Active data removed'
"Installer smoke $checks checks PASS (install, locked-host rejection, independent update, repair, uninstall, retained data)" |
    Tee-Object -FilePath (Join-Path $work 'installer-smoke-result.txt')
