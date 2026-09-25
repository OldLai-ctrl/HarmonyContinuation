[CmdletBinding()]
param(
    [string]$WorkspaceBinary,
    [string]$InstalledBinary
)

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($WorkspaceBinary)) {
    $WorkspaceBinary = Join-Path $scriptDirectory '..\build-vst3\VST3\Debug\HarmonyContinuation.vst3\Contents\x86_64-win\HarmonyContinuation.vst3'
}
if ([string]::IsNullOrWhiteSpace($InstalledBinary)) {
    $InstalledBinary = Join-Path $env:ProgramFiles 'Common Files\VST3\HarmonyContinuation.vst3\Contents\x86_64-win\HarmonyContinuation.vst3'
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host '请关闭 Cubase，并以管理员身份打开 PowerShell 后重试。未修改安装文件。' -ForegroundColor Yellow
    exit 3
}

$cubase = @(Get-Process -Name 'Cubase*' -ErrorAction SilentlyContinue)
if ($cubase.Count -gt 0) {
    Write-Host '检测到 Cubase 正在运行。请先正常关闭 Cubase；未修改安装文件。' -ForegroundColor Yellow
    exit 4
}
if (-not (Test-Path -LiteralPath $WorkspaceBinary -PathType Leaf)) {
    Write-Host "找不到工作区构建：$WorkspaceBinary。请先完成插件构建。" -ForegroundColor Red
    exit 2
}

$sourceHash = (Get-FileHash -LiteralPath $WorkspaceBinary -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant()
$destinationDirectory = Split-Path -Parent $InstalledBinary
$temporaryBinary = "$InstalledBinary.new-$PID"
$backupBinary = $null

try {
    New-Item -ItemType Directory -Path $destinationDirectory -Force -ErrorAction Stop | Out-Null
    Copy-Item -LiteralPath $WorkspaceBinary -Destination $temporaryBinary -ErrorAction Stop
    $temporaryHash = (Get-FileHash -LiteralPath $temporaryBinary -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant()
    if ($temporaryHash -ne $sourceHash) { throw '临时副本 SHA256 与工作区构建不一致。' }

    if (Test-Path -LiteralPath $InstalledBinary -PathType Leaf) {
        $backupBinary = "$InstalledBinary.backup-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
        [System.IO.File]::Replace($temporaryBinary, $InstalledBinary, $backupBinary, $true)
    } else {
        [System.IO.File]::Move($temporaryBinary, $InstalledBinary)
    }

    $installedHash = (Get-FileHash -LiteralPath $InstalledBinary -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant()
    Write-Host "工作区 SHA256：$sourceHash"
    Write-Host "安装版 SHA256：$installedHash"
    if ($installedHash -ne $sourceHash) {
        throw "安装后 SHA256 不一致。原文件备份：$backupBinary"
    }
    if ($backupBinary) { Write-Host "已保留旧版备份：$backupBinary" }
    Write-Host '结果：MATCH（Cubase 安装版与最新构建一致）' -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "安装失败：$($_.Exception.Message)" -ForegroundColor Red
    if ($backupBinary -and (Test-Path -LiteralPath $backupBinary -PathType Leaf)) {
        Write-Host "旧版仍保存在：$backupBinary" -ForegroundColor Yellow
    }
    exit 1
}
finally {
    if (Test-Path -LiteralPath $temporaryBinary -PathType Leaf) {
        Remove-Item -LiteralPath $temporaryBinary -Force -ErrorAction SilentlyContinue
    }
}
