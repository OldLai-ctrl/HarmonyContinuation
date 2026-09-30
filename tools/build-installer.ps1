param(
    [string]$BuildDirectory = 'build-v08',
    [string]$OutputDirectory = 'build-installer/output',
    [string]$Iscc = 'build-installer/tools/Inno/ISCC.exe',
    [string]$RuntimeDirectory = '',
    [string]$FactoryDatabase = '',
    [string]$TestRoot = '',
    [switch]$LibraryOnly
)
$ErrorActionPreference = 'Stop'
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
$manager = Join-Path $buildPath 'Release/library_manager.exe'
$info = & $manager inspect $FactoryDatabase
if ($LASTEXITCODE -ne 0 -or $info -notmatch 'library_version=(\d+)') { throw '进行库检查失败。' }
$libraryVersion = $Matches[1]
$header = Get-Content -LiteralPath (Join-Path $buildPath 'generated/ProductVersionGenerated.h') -Raw
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
$common = @('/Qp',('/DStageDir='+$stage),('/DOutputPath='+$outputPath),('/DProductVersion='+$productVersion),('/DLibraryVersion='+$libraryVersion))
if ($TestRoot) { $common += '/DTestRoot='+(Absolute $TestRoot) }
$script = Join-Path $repo 'packaging/HarmonyContinuation.iss'
& $compilerPath @common '/DLibraryOnly' $script
if ($LASTEXITCODE -ne 0) { throw '进行库安装程序构建失败。' }
$suffix = if ($TestRoot) {'-IsolatedTest'} else {''}
$librarySetup = Join-Path $outputPath "HarmonyContinuation-Library-$libraryVersion-Setup$suffix.exe"
if (!$LibraryOnly) {
    Copy-Item -LiteralPath $librarySetup -Destination (Join-Path $stage 'LibraryUpdate.exe')
    & $compilerPath @common $script
    if ($LASTEXITCODE -ne 0) { throw '完整安装程序构建失败。' }
}
$setups = @($librarySetup)
if (!$LibraryOnly) { $setups += Join-Path $outputPath "HarmonyContinuation-$productVersion-Setup$suffix.exe" }
foreach ($setup in $setups) {
    $hash = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($setup+'.sha256', $hash+'  '+[IO.Path]::GetFileName($setup)+"`n",[Text.UTF8Encoding]::new($false))
    Write-Host "已生成：$setup"
}
