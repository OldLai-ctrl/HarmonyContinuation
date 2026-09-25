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

$workspaceHash = $null
$installedHash = $null
$permissionIssue = $false

foreach ($entry in @(
    [pscustomobject]@{ Label = '工作区构建'; Path = $WorkspaceBinary; Target = 'Workspace' },
    [pscustomobject]@{ Label = 'Cubase 安装版'; Path = $InstalledBinary; Target = 'Installed' }
)) {
    if (-not (Test-Path -LiteralPath $entry.Path -PathType Leaf)) {
        Write-Host "$($entry.Label)：未找到文件 $($entry.Path)"
        continue
    }

    try {
        $fileHash = (Get-FileHash -LiteralPath $entry.Path -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant()
        Write-Host "$($entry.Label)：$($entry.Path)"
        Write-Host "SHA256：$fileHash"
        if ($entry.Target -eq 'Workspace') { $workspaceHash = $fileHash }
        else { $installedHash = $fileHash }
    }
    catch [System.UnauthorizedAccessException] {
        Write-Host "$($entry.Label)：无权读取 $($entry.Path)"
        $permissionIssue = $true
    }
    catch {
        Write-Host "$($entry.Label)：读取失败；$($_.Exception.Message)"
        if ($_.Exception -is [System.IO.IOException] -or $_.Exception -is [System.UnauthorizedAccessException]) {
            $permissionIssue = $true
        }
    }
}

if ($workspaceHash -and $installedHash) {
    if ($workspaceHash -eq $installedHash) {
        Write-Host '结果：MATCH（Cubase 安装版与最新构建一致）' -ForegroundColor Green
        exit 0
    }
    Write-Host '结果：MISMATCH（Cubase 安装版与最新构建不同）' -ForegroundColor Red
    exit 1
}

if ($permissionIssue) {
    Write-Host '请关闭 Cubase，再以管理员权限执行文件复制或重新运行本检查，然后核对 SHA256。' -ForegroundColor Yellow
} elseif (-not $workspaceHash) {
    Write-Host '请先成功构建 HarmonyContinuation，再运行本检查。' -ForegroundColor Yellow
} else {
    Write-Host '请关闭 Cubase，确认插件已复制到其扫描的 VST3 目录；若 Program Files 路径受限，请以管理员身份复制并重试。' -ForegroundColor Yellow
}
Write-Host '结果：无法比较（文件缺失或不可读）' -ForegroundColor Yellow
exit 2
