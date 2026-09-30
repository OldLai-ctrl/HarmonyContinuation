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