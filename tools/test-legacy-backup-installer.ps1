$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$work=Join-Path $repo 'build-installer'
$root=[IO.Path]::GetFullPath((Join-Path $work 'duplicate-smoke'))
if (!$root.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $root)) {
    throw '需要新的工作区内隔离目录。'
}
$name='HarmonyContinuation-rc1-backup-20260929'
$legacy=Join-Path $root ('VST3/'+$name)
$binary=Join-Path $legacy 'Contents/x86_64-win/HarmonyContinuation.vst3'
New-Item -ItemType Directory -Force -Path (Split-Path $binary -Parent) | Out-Null
$fixture=Join-Path $work 'duplicate-test-output/stage-2/HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3'
Copy-Item -LiteralPath $fixture -Destination $binary
$oldHash=(Get-FileHash -LiteralPath $binary).Hash
$existing=Join-Path $root ('Data/Backups/Plugins/'+$name)
New-Item -ItemType Directory -Force -Path $existing | Out-Null
[IO.File]::WriteAllText((Join-Path $existing 'keep.txt'),'Existing backup stays untouched')
$setup=Join-Path $work 'duplicate-test-output/HarmonyContinuation-0.8.0-dev.1-installer.1-Setup-IsolatedTest.exe'
function Run([string]$log) {
    $p=Start-Process -FilePath $setup -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/TYPE=full',('/LOG="'+(Join-Path $work $log)+'"')) -WindowStyle Hidden -Wait -PassThru
    return $p.ExitCode
}
$lock=[IO.File]::Open($binary,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try {
    if ((Run 'duplicate-locked.log') -eq 0) {throw 'Locked backup was not rejected'}
} finally {$lock.Dispose()}
if (!(Test-Path -LiteralPath $binary)) {throw 'Locked backup was changed'}
if ((Run 'duplicate-move.log') -ne 0) {throw 'Duplicate cleanup installation failed'}
$saved=Join-Path $root ('Data/Backups/Plugins/'+$name+'-1/Contents/x86_64-win/HarmonyContinuation.vst3')
if ((Test-Path -LiteralPath $legacy) -or (Get-FileHash -LiteralPath $saved).Hash -ne $oldHash) {throw 'Legacy backup not safely moved'}
if ((Get-Content -LiteralPath (Join-Path $existing 'keep.txt')) -ne 'Existing backup stays untouched') {throw 'Existing backup overwritten'}
$remaining=@(Get-ChildItem -LiteralPath (Join-Path $root 'VST3') -Filter 'HarmonyContinuation.vst3' -File -Recurse)
if ($remaining.Count -ne 1 -or $remaining[0].FullName -ne (Join-Path $root 'VST3/HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3')) {throw 'Scanner still sees duplicate plugins'}
'Legacy backup targeted checks PASS: occupied backup rejected; backup preserved; existing backup retained; one scannable plugin' |
    Tee-Object -FilePath (Join-Path $work 'duplicate-smoke-result.txt')
