param(
    [string]$BuildDirectory = 'build-v3-plugin',
    [string]$PackageDirectory = 'build-installer/v3-output',
    [string]$Iscc = 'build-installer/tools/Inno/ISCC.exe',
    [string]$TestRoot = 'build-installer/v3-smoke'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
function Absolute([string]$path) {
    if ([IO.Path]::IsPathRooted($path)) { return [IO.Path]::GetFullPath($path) }
    return [IO.Path]::GetFullPath((Join-Path $repo $path))
}
$root = Absolute $TestRoot
if (!$root.StartsWith($repo + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $root)) { throw '需要新的项目内隔离目录。' }
$build = Absolute $BuildDirectory
$package = Absolute $PackageDirectory
$compiler = Absolute $Iscc
$header = Get-Content -LiteralPath (Join-Path $build 'generated/ProductVersionGenerated.h') -Raw
if ($header -notmatch 'HC_PRODUCT_VERSION\s+"([^"]+)"') { throw 'Missing product version' }
$version = $Matches[1]
$isolatedOutput = Join-Path $root 'packages'
$store = Join-Path $root 'Data/Libraries/Factory'
New-Item -ItemType Directory -Path $isolatedOutput,(Join-Path $store '2') | Out-Null
# A historical Factory fixture and a personal-data sentinel must survive the
# install/uninstall lifecycle. No production user directory is read or written.
$oldDb = Join-Path $store '2/factory.db'
Copy-Item -LiteralPath (Join-Path $build 'factory-v2.db') -Destination $oldDb
[IO.File]::WriteAllText((Join-Path $store 'active.txt'),"2`n")
$personal = Join-Path $root 'Data/personal-collection.keep'
[IO.File]::WriteAllText($personal,'Personal data sentinel: preserve on install and uninstall')
$oldHash = (Get-FileHash -LiteralPath $oldDb).Hash
$personalHash = (Get-FileHash -LiteralPath $personal).Hash
$compilerArgs = @('/Qp',('/DStageDir='+(Join-Path $package 'stage-3')),
    ('/DOutputPath='+$isolatedOutput),('/DProductVersion='+$version),'/DLibraryVersion=3',('/DTestRoot='+$root))
$compileLog = Join-Path $root 'compile.log'
& $compiler @compilerArgs (Join-Path $repo 'packaging/HarmonyContinuation.iss') *> $compileLog
if ($LASTEXITCODE -ne 0) { throw '隔离测试包编译失败。' }
$checks = 0
function Check([bool]$ok,[string]$message) {
    if (!$ok) { throw $message }
    $script:checks++
}
function Run([string]$exe,[string]$log) {
    $args=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+(Join-Path $root $log)+'"'))
    $process=Start-Process -FilePath $exe -ArgumentList $args -WindowStyle Hidden -Wait -PassThru
    Check ($process.ExitCode -eq 0) ('Installer exit: '+$process.ExitCode)
}
$setup = Join-Path $isolatedOutput "HarmonyContinuation-$version-Setup-IsolatedTest.exe"
Run $setup 'install.log'
$binary = Join-Path $root 'VST3/HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3'
$stagedBinary = Join-Path $package 'stage-3/HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3'
Check ((Test-Path -LiteralPath $binary) -and
    (Get-FileHash -LiteralPath $binary).Hash -eq (Get-FileHash -LiteralPath $stagedBinary).Hash) 'Plugin differs from staged build'
Check ((Get-Content -LiteralPath (Join-Path $store 'active.txt')).Trim() -eq '3') 'Library 3 is not active'
$manager = Join-Path $root 'HarmonyContinuation/library_manager.exe'
$info = & $manager inspect (Join-Path $store '3/factory.db')
Check ($LASTEXITCODE -eq 0 -and $info -match 'library_version=3 schema=2 progressions=629') 'Installed catalogue differs'
Check ((Get-FileHash -LiteralPath $oldDb).Hash -eq $oldHash -and
    (Get-FileHash -LiteralPath $personal).Hash -eq $personalHash) 'Install changed retained data'
Run (Join-Path $root 'HarmonyContinuation/unins000.exe') 'uninstall.log'
Check (!(Test-Path -LiteralPath $binary)) 'Plugin remains after uninstall'
Check ((Get-FileHash -LiteralPath $oldDb).Hash -eq $oldHash -and
    (Get-FileHash -LiteralPath $personal).Hash -eq $personalHash -and
    (Test-Path -LiteralPath (Join-Path $store '3/factory.db'))) 'Uninstall removed retained data'
Check ((Get-Content -LiteralPath (Join-Path $store 'active.txt')).Trim() -eq '3') 'Uninstall removed active pointer'
"V3 isolated installer $checks checks PASS: install, 629-row catalogue, uninstall, retained Factory history and personal sentinel" |
    Tee-Object -FilePath (Join-Path $root 'result.txt')
