# TH15 THCRAP localization (functional adaptation)

TH15 uses the existing TH10/11 boundary: official THCRAP patches → host
compiler → immutable language ZIP/catalog → validated Launcher installation →
Runtime `/thcrap/th15/` overrides. No retail translation archive is used and
the Runtime does not fetch THCRAP servers. This is functional support, not
original GDI/pixel parity or a completed all-scenes acceptance claim.

## Sources and exact title differences

- Official sources: `thpatch/thcrap`, `thcrap_tsa/src/th06_msg.cpp`,
  `layout.cpp`, and `spells.cpp`;
  `https://mirrors.thpatch.net/nmlgc/base_tsa/th15.js` and
  `th15/stringlocs.js`; `https://srv.thpatch.net/lang_zh-hans/` and
  `https://srv.thpatch.net/lang_en/`.
- TH15 uses `MSG_TH14`, not the TH10/11 opcode mapping: auto line 17;
  auto-end 7/8/9/11/32; delete 25; bubble position 28. The existing shared
  compiler preserves timecodes, wait behavior, untranslated boxes and
  inserts extra translated lines before the original box boundary.
- Endings use `END_TH10`: auto line 3, auto-end 5/6/9. Compile with
  `thmsg -e 15`, while stage scripts use `thmsg 15` and `thdat 15`.
- THCRAP's `|\tbegin\t,\tbase\t,annotation` Ruby syntax is not a pair of
  numbers. The Runtime uses measured layout widths and the TH15 +4 full-pixel
  correction from `ruby_offset_half`; original numeric Ruby remains intact.
  In the current complete English stage-patch set, three source rows mean
  one Ruby plus two ordinary rows, not a three-line ordinary bubble.
- `box_end` uses a 512 logical-pixel width cap, 19-pixel padding and 640-pixel
  logical screen. The Runtime applies that positioning calculation before
  drawing using look-ahead to the same box boundary. Text measurement is SDL
  TTF rather than GDI; equivalent font metrics are not assumed.
- Runtime JP 1.00b mounts only official `text.v1.00b.anm` as `text.anm`;
  `text.v1.00a.anm` is not substituted into a 1.00b runtime.
- Shared `th15/stringlocs.js` supplies Result and Music Room format IDs and
  full-width digits. Player Data consumes the actual THCRAP Result templates,
  not a new translated-name column format. Spell names are translated for
  display only; score, Replay, RNG and authoritative cadence are unchanged.

## 准备与交付

在 Web 目录调用 `scripts/prepare-th06-language-pack.mjs`，传入本作 `--game`、语言、原版归档、thdat/thmsg、字体和明确输出目录。生成的 catalog 与语言 ZIP 作为 Host 输入，通过 `LanguagePackDirectories` 登记；具体步骤见[语言与资源教程](../../../docs/playbooks/localization-thcrap.md)及[新游戏接入](ADAPTING_A_GAME.md)。

当前源码应以这些输入重新构建语言包、组装站点并验证实际 Runtime 挂载。旧文档里的某次 ZIP 摘要、实验晋升和截图记录保留在比较证据中，不作为当前通过结果。相关 Web 编译用例在 `tests/test-thcrap-compiler.mjs`、`tests/test-thcrap-static-pack.mjs`；作品 Runtime 的文本排版、对话、结局和真实设备另行验收。
