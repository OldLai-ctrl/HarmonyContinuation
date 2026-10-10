param(
    [string]$BuildDirectory = 'build-v3-plugin',
    [string]$OutputDirectory = 'build-installer/output',
    [string]$Iscc = 'build-installer/tools/Inno/ISCC.exe',
    [string]$RuntimeDirectory = '',
    [string]$FactoryDatabase = '',
    [string]$TestRoot = '',
    [switch]$LibraryOnly
)
$ErrorActionPreference = 'Stop'
if ($LibraryOnly) { throw 'Unified Setup uses /COMPONENTS=library. Separate library installers are no longer generated.' }
$repo = Split-Path -Parent $PSScriptRoot
function Absolute([string]$path) {
    if ([IO.Path]::IsPathRooted($path)) { return [IO.Path]::GetFullPath($path) }
    return [IO.Path]::GetFullPath((Join-Path $repo $path))
}
$buildPath = Absolute $BuildDirectory
$outputPath = Absolute $OutputDirectory
$compilerPath = Absolute $Iscc
if (!(Test-Path -LiteralPath $compilerPath)) { throw '需要提供 Inno Setup 6.7+ 的 ISCC.exe 路径。' }
if (!$RuntimeDirectory) {
    $redist = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio/2022/Community/VC/Redist/MSVC'
    $RuntimeDirectory = Get-ChildItem -LiteralPath $redist -Directory |
        Where-Object { $_.Name -match '^\d+\.' } | Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Join-Path $_.FullName 'x64/Microsoft.VC143.CRT' } |
        Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (!$RuntimeDirectory -or !(Test-Path -LiteralPath (Join-Path $RuntimeDirectory 'msvcp140.dll'))) {
    throw '找不到可随程序分发的 MSVC x64 运行库，请提供 RuntimeDirectory。'
}
if (!$FactoryDatabase) { $FactoryDatabase = Join-Path $buildPath 'factory.db' }
$FactoryDatabase = Absolute $FactoryDatabase
$header = Get-Content -LiteralPath (Join-Path $buildPath 'generated/ProductVersionGenerated.h') -Raw
$manager = Join-Path $buildPath 'Release/library_manager.exe'
if (!(Test-Path -LiteralPath $manager)) {
    if ($header -notmatch 'HC_PRODUCT_BUILD_TYPE\s+"Release"') {
        throw '单配置安装包必须来自 Release 构建。'
    }
    $manager = Join-Path $buildPath 'library_manager.exe'
}
$info = & $manager inspect $FactoryDatabase
if ($LASTEXITCODE -ne 0 -or $info -notmatch 'library_version=(\d+)') { throw '进行库检查失败。' }
$libraryVersion = $Matches[1]
if ($header -notmatch 'HC_PRODUCT_VERSION\s+"([^"]+)"') { throw '无法读取产品版本。' }
$productVersion = $Matches[1]
$stage = Join-Path $outputPath ('stage-' + $libraryVersion)
New-Item -ItemType Directory -Force -Path $stage,(Join-Path $stage 'runtime') | Out-Null
Copy-Item -LiteralPath $manager -Destination (Join-Path $stage 'library_manager.exe')
Copy-Item -LiteralPath $FactoryDatabase -Destination (Join-Path $stage 'factory.db')
Get-ChildItem -LiteralPath $RuntimeDirectory -Filter '*.dll' -File | Copy-Item -Destination (Join-Path $stage 'runtime')
if (!$LibraryOnly) {
    $bundle = Join-Path $buildPath 'VST3/Release/HarmonyContinuation.vst3'
    # Recreate only our verified staging bundle; never touch an installed plugin.
    $stageBundle = [IO.Path]::GetFullPath((Join-Path $stage 'HarmonyContinuation.vst3'))
    if (!$stageBundle.StartsWith($outputPath + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
        throw '暂存目录超出输出目录。'
    }
    if (Test-Path -LiteralPath $stageBundle) { Remove-Item -LiteralPath $stageBundle -Recurse -Force }
    Copy-Item -LiteralPath $bundle -Destination $stageBundle -Recurse
    Get-ChildItem -LiteralPath $RuntimeDirectory -Filter '*.dll' -File |
        Copy-Item -Destination (Join-Path $stageBundle 'Contents/x86_64-win')
}
# The Factory is an independent data component; do not ship a hidden fallback
# inside the VST3 component. Only the newly staged copy is changed here.
$bundledFactory = Join-Path $stageBundle 'Contents/Resources/factory.db'
if (Test-Path -LiteralPath $bundledFactory) { Remove-Item -LiteralPath $bundledFactory }
$manifest = Get-ChildItem -LiteralPath $stageBundle -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($stageBundle.Length + 1)
    (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '|' + $relative
}
[IO.File]::WriteAllText((Join-Path $stage 'plugin-files.txt'),($manifest -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
$factoryHash = (Get-FileHash -LiteralPath $FactoryDatabase -Algorithm SHA256).Hash.ToLowerInvariant()
$legacyFactoryHash = ''
$legacyDb = Join-Path $buildPath 'factory-v2.db'
if (Test-Path -LiteralPath $legacyDb) { $legacyFactoryHash = (Get-FileHash -LiteralPath $legacyDb -Algorithm SHA256).Hash.ToLowerInvariant() }
$common = @('/Qp',('/DStageDir='+$stage),('/DOutputPath='+$outputPath),('/DProductVersion='+$productVersion),('/DLibraryVersion='+$libraryVersion),('/DFactoryHash='+$factoryHash),('/DLegacyFactoryHash='+$legacyFactoryHash))
if ($TestRoot) { $common += '/DTestRoot='+(Absolute $TestRoot) }
& $compilerPath @common (Join-Path $repo 'packaging/UnifiedSetup.iss')
if ($LASTEXITCODE -ne 0) { throw 'Unified Setup compilation failed.' }
$suffix = if ($TestRoot) {'-IsolatedTest'} else {''}
$setups = @(Join-Path $outputPath "HarmonyContinuation-Setup$suffix.exe")
$checksums = @()
foreach ($setup in $setups) {
    $hash = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
    $line = $hash+'  '+[IO.Path]::GetFileName($setup)
    $checksums += $line
    [IO.File]::WriteAllText($setup+'.sha256', $line+"`n",[Text.UTF8Encoding]::new($false))
    Write-Host "已生成：$setup"
}
[IO.File]::WriteAllText((Join-Path $outputPath 'SHA256SUMS.txt'), ($checksums -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
