# TH09 THCRAP localization adaptation

This title uses the same Host → static THCRAP ZIP → Launcher → Runtime boundary
as TH06/07/08/10. The Runtime never downloads patch repositories itself.
The unmodified Japanese files remain the fallback when a patch file or entry
is absent.

## Source ownership

- TH09 v1.50a archive extraction uses `thdat -x 9`; dialogue compilation uses
  `thmsg -d/-c 9` and the TH09 opcode/crypt table from `thcrap_tsa`.
- Endings (`endNN.end`) use the base_tsa `.end.jdiff` patch format; TH09 story
  and match dialogues use `plNN.msg.jdiff` and `plNN_match.msg.jdiff`.
- `spells.js`, `themes.js`, `musiccmt.js`, TH09 stringlocs and `data/**/*.png`
  are compiled or copied by the shared THCRAP pack preparation path.
- A local, licensed Unicode font is subset to the codepoints in each selected
  patch. The selected `options.json` names that subset. Do not publish retail
  `th09.dat`, `th09.data`, or `msgothic.ttc` as part of the language ZIP.

## 准备与交付

在 Web 目录调用 `scripts/prepare-th06-language-pack.mjs`，传入本作 `--game`、语言、原版归档、thdat/thmsg、字体和明确输出目录。生成的 catalog 与语言 ZIP 作为 Host 输入，通过 `LanguagePackDirectories` 登记；具体步骤见[语言与资源教程](../../../docs/playbooks/localization-thcrap.md)及[新游戏接入](ADAPTING_A_GAME.md)。

当前源码应以这些输入重新构建语言包、组装站点并验证实际 Runtime 挂载。旧文档里的某次 ZIP 摘要、实验晋升和截图记录保留在比较证据中，不作为当前通过结果。相关 Web 编译用例在 `tests/test-thcrap-compiler.mjs`、`tests/test-thcrap-static-pack.mjs`；作品 Runtime 的文本排版、对话、结局和真实设备另行验收。
