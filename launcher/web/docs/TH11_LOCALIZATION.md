# TH11 THCRAP localization adaptation

TH11 follows the existing Host → static THCRAP ZIP → Launcher → Runtime
boundary used by TH06/07/08/10. The original Japanese game data remains the
fallback when a patch file or entry is absent. The Runtime does not fetch
THCRAP repositories.

## Source ownership

- Use the Japanese `th11.dat` to compile patches. Do not use `th11c.dat` or
  the prelocalized Traditional Chinese files from mixed retail folders.
- `thdat -x 11` extracts the original archive. `thmsg -d/-c 11` compiles
  `stNN_NN[a-c].msg.jdiff`; `thmsg -d/-c -e 11` compiles `eNN.msg.jdiff`.
  The opcode table is `MSG_TH11`/`END_TH10` from THCRAP's `th06_msg.cpp`,
  including `OP_DELETE` for dialogue opcode 25.
- The shared compiler prepares the THCRAP PNG replacements, `spells.js`,
  `themes.js`, `musiccmt.js`, and the TH11 `stringlocs.v1.00a.js` keys.
  Every published language ZIP includes a licensed, codepoint-subset OTF
  named by its own `localization/options.json`.
- The language ZIP contains neither retail `th11.dat`/`th11.data` nor
  `msgothic.ttc`. It does not reuse any locally pretranslated game archive.

## 准备与交付

在 Web 目录调用 `scripts/prepare-th06-language-pack.mjs`，传入本作 `--game`、语言、原版归档、thdat/thmsg、字体和明确输出目录。生成的 catalog 与语言 ZIP 作为 Host 输入，通过 `LanguagePackDirectories` 登记；具体步骤见[语言与资源教程](../../../docs/playbooks/localization-thcrap.md)及[新游戏接入](ADAPTING_A_GAME.md)。

当前源码应以这些输入重新构建语言包、组装站点并验证实际 Runtime 挂载。旧文档里的某次 ZIP 摘要、实验晋升和截图记录保留在比较证据中，不作为当前通过结果。相关 Web 编译用例在 `tests/test-thcrap-compiler.mjs`、`tests/test-thcrap-static-pack.mjs`；作品 Runtime 的文本排版、对话、结局和真实设备另行验收。
