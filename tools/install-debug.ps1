[CmdletBinding()]
param(
    [string]$WorkspaceBinary,
    [string]$InstalledBinary,
    [string]$WorkspaceFactoryDb,
    [string]$InstalledFactoryDb
)

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($WorkspaceBinary)) {
    $WorkspaceBinary = Join-Path $scriptDirectory '..\build-vst3\VST3\Debug\HarmonyContinuation.vst3\Contents\x86_64-win\HarmonyContinuation.vst3'
}
if ([string]::IsNullOrWhiteSpace($InstalledBinary)) {
    $InstalledBinary = Join-Path $env:ProgramFiles 'Common Files\VST3\HarmonyContinuation.vst3\Contents\x86_64-win\HarmonyContinuation.vst3'
}
if ([string]::IsNullOrWhiteSpace($WorkspaceFactoryDb)) {
    $WorkspaceFactoryDb = Join-Path (Split-Path -Parent (Split-Path -Parent $WorkspaceBinary)) 'Resources\factory.db'
}
if ([string]::IsNullOrWhiteSpace($InstalledFactoryDb)) {
    $InstalledFactoryDb = Join-Path (Split-Path -Parent (Split-Path -Parent $InstalledBinary)) 'Resources\factory.db'
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
if (-not (Test-Path -LiteralPath $WorkspaceFactoryDb -PathType Leaf)) {
    Write-Host "找不到工作区曲库：$WorkspaceFactoryDb。请先完成 Phase 3 构建。" -ForegroundColor Red
    exit 2
}

$sourceHash = (Get-FileHash -LiteralPath $WorkspaceBinary -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant()
$sourceDbHash = (Get-FileHash -LiteralPath $WorkspaceFactoryDb -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant()
$destinationDirectory = Split-Path -Parent $InstalledBinary
$temporaryBinary = "$InstalledBinary.new-$PID"
$temporaryDb = "$InstalledFactoryDb.new-$PID"
$backupBinary = $null
$backupDb = $null

try {
    New-Item -ItemType Directory -Path $destinationDirectory -Force -ErrorAction Stop | Out-Null
    New-Item -ItemType Directory -Path (Split-Path -Parent $InstalledFactoryDb) -Force -ErrorAction Stop | Out-Null
    # Install the resource first. An old binary can ignore a new resource;
    # a new binary must never be exposed with a missing resource.
    Copy-Item -LiteralPath $WorkspaceFactoryDb -Destination $temporaryDb -ErrorAction Stop
    if ((Get-FileHash -LiteralPath $temporaryDb -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant() -ne $sourceDbHash) {
        throw '临时曲库 SHA256 与工作区构建不一致。'
    }
    if (Test-Path -LiteralPath $InstalledFactoryDb -PathType Leaf) {
        $backupDb = "$InstalledFactoryDb.backup-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
        [System.IO.File]::Replace($temporaryDb, $InstalledFactoryDb, $backupDb, $true)
    } else {
        [System.IO.File]::Move($temporaryDb, $InstalledFactoryDb)
    }
    if ((Get-FileHash -LiteralPath $InstalledFactoryDb -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant() -ne $sourceDbHash) {
        throw '安装曲库 SHA256 不一致。'
    }
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
    Write-Host "曲库 SHA256：$sourceDbHash"
    if ($installedHash -ne $sourceHash) {
        throw "安装后 SHA256 不一致。原文件备份：$backupBinary"
    }
    if ($backupBinary) { Write-Host "已保留旧版备份：$backupBinary" }
    if ($backupDb) { Write-Host "已保留旧曲库备份：$backupDb" }
    Write-Host '结果：MATCH（插件及曲库与最新构建一致）' -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "安装失败：$($_.Exception.Message)" -ForegroundColor Red
    if ($backupBinary -and (Test-Path -LiteralPath $backupBinary -PathType Leaf)) {
        Write-Host "旧版仍保存在：$backupBinary" -ForegroundColor Yellow
    }
    if ($backupDb -and (Test-Path -LiteralPath $backupDb -PathType Leaf)) {
        Write-Host "旧曲库仍保存在：$backupDb" -ForegroundColor Yellow
    }
    exit 1
}
finally {
    if (Test-Path -LiteralPath $temporaryBinary -PathType Leaf) {
        Remove-Item -LiteralPath $temporaryBinary -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $temporaryDb -PathType Leaf) {
        Remove-Item -LiteralPath $temporaryDb -Force -ErrorAction SilentlyContinue
    }
}
