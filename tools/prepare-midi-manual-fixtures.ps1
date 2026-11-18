param([string]$BuildDirectory='build-v08', [string]$OutputDirectory='manual-tests/dev2-midi')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$output=[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
if (!$output.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw '验收文件必须生成在项目目录内。'
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
function AddBE($buffer, [uint32]$number, [int]$size) {
    for($i=$size-1;$i -ge 0;$i--) {$buffer.Add([byte](($number -shr ($i*8)) -band 255))}
}
function AddVLQ($buffer,[int]$number) {
    $reverse=[Collections.Generic.List[byte]]::new();$reverse.Add([byte]($number -band 127))
    while(($number=$number -shr 7) -gt 0) {$reverse.Add([byte](($number -band 127) -bor 128))}
    for($i=$reverse.Count-1;$i -ge 0;$i--) {$buffer.Add($reverse[$i])}
}
function AddEvent($events,[int]$tick,[int]$rank,[byte[]]$payload) {
    $events.Add([pscustomobject]@{Tick=$tick;Rank=$rank;Payload=$payload})
}
function AddNote($events,[int]$pitch,[int]$start,[int]$duration,[int]$channel=0) {
    AddEvent $events $start 2 ([byte[]]@((144+$channel),$pitch,80))
    AddEvent $events ($start+$duration) 1 ([byte[]]@((128+$channel),$pitch,0))
}
function MakeTrack($events,[int]$end,[string]$name) {
    $text=[Text.Encoding]::ASCII.GetBytes($name)
    if($text.Length -gt 127) {throw 'Track name too long'}
    AddEvent $events 0 0 ([byte[]](@(255,3,$text.Length)+@($text)))
    AddEvent $events $end 99 ([byte[]]@(255,47,0))
    $data=[Collections.Generic.List[byte]]::new();$last=0
    foreach($event in ($events | Sort-Object Tick,Rank)) {
        AddVLQ $data ($event.Tick-$last);$data.AddRange([byte[]]$event.Payload);$last=$event.Tick
    }
    $track=[Collections.Generic.List[byte]]::new();$track.AddRange([Text.Encoding]::ASCII.GetBytes('MTrk'))
    AddBE $track $data.Count 4;$track.AddRange($data.ToArray());return ,$track.ToArray()
}
function MakeHeader([int]$format,[int]$tracks) {
    $bytes=[Collections.Generic.List[byte]]::new();$bytes.AddRange([Text.Encoding]::ASCII.GetBytes('MThd'))
    AddBE $bytes 6 4;AddBE $bytes $format 2;AddBE $bytes $tracks 2;AddBE $bytes 480 2
    return ,$bytes.ToArray()
}
$fixtures=Join-Path $repo "$BuildDirectory/midi-import-fixtures"
foreach($name in @('pop_block.mid','inversion_block.mid','sevenths_block.mid')) {
    Copy-Item -LiteralPath (Join-Path $fixtures $name) -Destination (Join-Path $output $name)
}
# Existing export has a conductor track and a chord track. Add distinct melody/drum tracks.
$base=[IO.File]::ReadAllBytes((Join-Path $output 'pop_block.mid'))
if($base.Length -lt 22 -or [Text.Encoding]::ASCII.GetString($base,0,4) -ne 'MThd' -or
    $base[7] -ne 6 -or $base[9] -ne 1 -or $base[11] -ne 2 -or $base[12] -ne 1 -or $base[13] -ne 224) {
    throw 'Expected existing Format 1, two-track, 480 PPQ export fixture.'
}
$melody=[Collections.Generic.List[object]]::new();$drums=[Collections.Generic.List[object]]::new()
for($i=0;$i -lt 14;$i++) {
    AddNote $melody (@(72,74,76,79)[$i%4]) ($i*240) 180 1
    AddNote $drums (@(36,42)[$i%2]) ($i*240) 48 9
}
$multi=[Collections.Generic.List[byte]]::new();$multi.AddRange((MakeHeader 1 4));$multi.AddRange([byte[]]$base[14..($base.Length-1)])
$multi.AddRange((MakeTrack $melody 3360 'Melody'));$multi.AddRange((MakeTrack $drums 3360 'Drums channel 10'))
[IO.File]::WriteAllBytes((Join-Path $output 'multi_chords_melody_drums.mid'),$multi.ToArray())
$passing=[Collections.Generic.List[object]]::new();foreach($pitch in @(48,64,67,70)) {AddNote $passing $pitch 0 1920}
AddNote $passing 74 480 24
[IO.File]::WriteAllBytes((Join-Path $output 'short_passing_C7.mid'),[byte[]]((MakeHeader 0 1)+(MakeTrack $passing 1920 'C7 with short D')))
$single=[Collections.Generic.List[object]]::new();for($i=0;$i -lt 4;$i++) {AddNote $single (@(60,64,67,72)[$i]) ($i*480) 240}
[IO.File]::WriteAllBytes((Join-Path $output 'no_chords_melody.mid'),[byte[]]((MakeHeader 0 1)+(MakeTrack $single 1920 'Single notes')))
[IO.File]::WriteAllBytes((Join-Path $output 'malformed_truncated.mid'),[byte[]]$base[0..20])
$smpte=[byte[]]$base.Clone();$smpte[12]=231;$smpte[13]=40
[IO.File]::WriteAllBytes((Join-Path $output 'unsupported_smpte.mid'),$smpte)
$readme=@'
# dev.2 Cubase 验收 MIDI 文件

关闭 Cubase 后安装 dev.2；使用测试工程。以下文件只供人工验收，不是用户曲库。

| 文件 | 预期结果 |
|---|---|
| pop_block.mid | C → Am → F → G；起点 0、1、2.5、4.5 QN，持续 1、1.5、2、2.5 QN，总长 7 QN。 |
| inversion_block.mid | C → G/B → Am；起点 0、1、2.5 QN，持续 1、1.5、2 QN，总长 4.5 QN。 |
| sevenths_block.mid | Cmaj7 → Am7 → Dm7 → G7；节奏与 pop 文件相同。 |
| multi_chords_melody_drums.mid | Format 1：沿用 pop 和弦轨，另加 Melody 与鼓通道 10。应选原和弦轨，还原 pop；高级信息可核对选轨，不混合旋律和鼓。 |
| short_passing_C7.mid | Format 0：C7 持续 4 QN；第 1 QN 有持续 0.05 QN 的 D 经过音。应维持 C7，不出现大量新和弦。 |
| no_chords_melody.mid | 单音旋律，应提示未识别到有效进行，并保留原 Current Phrase。 |
| malformed_truncated.mid | 故意截断的损坏文件，应拒绝并保留原进行，不崩溃。 |
| unsupported_smpte.mid | 故意使用 SMPTE 时基，应提示不支持并保留原进行。 |

OPEN：pop 文件分别以完整片段和“作为未完成进行”导入。完整模式 G = 2.5 QN；未完成模式 G = OPEN。

四处拖出与同候选保存的 MIDI 对照，由插件实际操作产生文件。本目录不预设这两项已经通过。
'@
[IO.File]::WriteAllText((Join-Path $output 'README.md'),$readme,[Text.UTF8Encoding]::new($false))
Write-Host "已准备 8 个人工验收 MIDI 文件：$output"
