# th13 适配检查

[开发入口](README.md) · [上游适配流程](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/README.md)

源码目录：`ports/th13`。构建模式：`normal`。实现、运行验证、行为保真和最终输出分别记录。

本次新增 Launcher 接入未运行构建或浏览器验证，保持 `待验证`；不从历史报告继承通过。

## 源码与构建

| 编号 | 检查项 | normal | 记录 |
| --- | --- | --- | --- |
| BUILD-01 | [源码目录与普通版／MP归属](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/build.md#build-01) | 待验证 | 本次不以静态实现替代运行验证 |
| BUILD-02 | [工具链与外部依赖](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/build.md#build-02) | 待验证 | 本次不以静态实现替代运行验证 |
| BUILD-03 | [对应模式独立构建](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/build.md#build-03) | 待验证 | 本次不以静态实现替代运行验证 |
| BUILD-04 | [Runtime 文件与来源记录](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/build.md#build-04) | 待验证 | 本次不以静态实现替代运行验证 |
| BUILD-05 | [前端搭配单个游戏运行](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/build.md#build-05) | 待验证 | 本次不以静态实现替代运行验证 |
| BUILD-06 | [源码与素材来源](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/build.md#build-06) | 待验证 | 本次不以静态实现替代运行验证 |

## 启动与资源

| 编号 | 检查项 | normal | 记录 |
| --- | --- | --- | --- |
| RUNTIME-01 | [协议身份与会话代次](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#runtime-01) | 待验证 | 本次不以静态实现替代运行验证 |
| RUNTIME-02 | [就绪、配置、启动、首帧](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#runtime-02) | 待验证 | 本次不以静态实现替代运行验证 |
| RUNTIME-03 | [命令响应与错误信息](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#runtime-03) | 待验证 | 本次不以静态实现替代运行验证 |
| RUNTIME-04 | [后台、恢复与退出清理](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#runtime-04) | 待验证 | 本次不以静态实现替代运行验证 |
| RUNTIME-05 | [Runtime 与资源管理分工](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#runtime-05) | 待验证 | 本次不以静态实现替代运行验证 |
| RUNTIME-06 | [重开与常显判定点](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#runtime-06) | 不适用 | 本作未声明判定点显示 |
| PACKAGE-01 | [共用游戏包存储](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#package-01) | 待验证 | 本次不以静态实现替代运行验证 |
| PACKAGE-02 | [DATA 身份与挂载目标](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#package-02) | 待验证 | 本次不以静态实现替代运行验证 |
| PACKAGE-03 | [完整更新后切换版本](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#package-03) | 待验证 | 本次不以静态实现替代运行验证 |
| PACKAGE-04 | [本地导入与在线安装一致](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#package-04) | 待验证 | 本次不以静态实现替代运行验证 |
| PACKAGE-05 | [已安装游戏离线启动](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#package-05) | 待验证 | 本次不以静态实现替代运行验证 |
| PACKAGE-06 | [统一安装和更新入口](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/runtime.md#package-06) | 待验证 | 本次不以静态实现替代运行验证 |

## 键盘、手柄与触控

| 编号 | 检查项 | normal | 记录 |
| --- | --- | --- | --- |
| INPUT-01 | [键盘逻辑输入](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#input-01) | 待验证 | 本次不以静态实现替代运行验证 |
| INPUT-02 | [启动器键盘改键](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#input-02) | 待验证 | 本次不以静态实现替代运行验证 |
| INPUT-03 | [启动器手柄映射](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#input-03) | 待验证 | 本次不以静态实现替代运行验证 |
| INPUT-04 | [手柄方向与按钮](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#input-04) | 待验证 | 本次不以静态实现替代运行验证 |
| INPUT-05 | [多种输入独立持有](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#input-05) | 待验证 | 本次不以静态实现替代运行验证 |
| INPUT-06 | [失焦与断开释放](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#input-06) | 待验证 | 本次不以静态实现替代运行验证 |
| INPUT-07 | [原作菜单与对话输入](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#input-07) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-01 | [限速滑动移动](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-01) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-02 | [无限速滑动移动](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-02) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-03 | [方向式虚拟摇杆](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-03) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-04 | [自由方向虚拟摇杆](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-04) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-05 | [按住低速](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-05) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-06 | [切换低速](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-06) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-07 | [双指低速](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-07) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-08 | [射击、Bomb 与暂停](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-08) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-09 | [Deathbomb 与移动保持](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-09) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-10 | [无限速移动落率记录](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-10) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-11 | [双击 Bomb](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-11) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-12 | [菜单与对话手势](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-12) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-13 | [确认键的新按下边沿](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-13) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-14 | [100–300% 灵敏度](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-14) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-15 | [触控与其他输入隔离](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-15) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-16 | [触控取消与生命周期](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-16) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-17 | [共用布局编辑器](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-17) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-18 | [共用重开按钮](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-18) | 待验证 | 本次不以静态实现替代运行验证 |
| TOUCH-19 | [共用双指放大镜](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/input.md#touch-19) | 待验证 | 本次不以静态实现替代运行验证 |

## 画面与声音

| 编号 | 检查项 | normal | 记录 |
| --- | --- | --- | --- |
| DISPLAY-01 | [固定逻辑步进](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#display-01) | 待验证 | 本次不以静态实现替代运行验证 |
| DISPLAY-02 | [跟随显示器刷新率](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#display-02) | 待验证 | 本次不以静态实现替代运行验证 |
| DISPLAY-03 | [额外绘制无逻辑副作用](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#display-03) | 待验证 | 本次不以静态实现替代运行验证 |
| DISPLAY-04 | [连续视觉字段插值](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#display-04) | 待验证 | 本次不以静态实现替代运行验证 |
| DISPLAY-05 | [出生、销毁和瞬移](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#display-05) | 待验证 | 本次不以静态实现替代运行验证 |
| DISPLAY-06 | [暂停画面的表现](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#display-06) | 待验证 | 本次不以静态实现替代运行验证 |
| DISPLAY-07 | [实际呈现后报告首帧](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#display-07) | 待验证 | 本次不以静态实现替代运行验证 |
| DISPLAY-08 | [限制到 60 Hz](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#display-08) | 待验证 | 本次不以静态实现替代运行验证 |
| AUDIO-01 | [正常 OGG 音乐](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#audio-01) | 待验证 | 本次不以静态实现替代运行验证 |
| AUDIO-02 | [无 BGM 模式](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#audio-02) | 待验证 | 本次不以静态实现替代运行验证 |
| AUDIO-03 | [切曲、渐变、暂停、停止](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#audio-03) | 待验证 | 本次不以静态实现替代运行验证 |
| AUDIO-04 | [返回前台后恢复声音](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#audio-04) | 待验证 | 本次不以静态实现替代运行验证 |
| AUDIO-05 | [音乐资源错误信息](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#audio-05) | 待验证 | 本次不以静态实现替代运行验证 |
| AUDIO-06 | [高刷新与音频事件分开](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#audio-06) | 待验证 | 本次不以静态实现替代运行验证 |
| MIDI-01 | [声明 MIDI 选项](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#midi-01) | 不适用 | 本作使用单流 Vorbis 音乐 |
| MIDI-02 | [可听到的 MIDI 输出](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#midi-02) | 不适用 | 本作使用单流 Vorbis 音乐 |
| MIDI-03 | [MIDI 生命周期](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#midi-03) | 不适用 | 本作使用单流 Vorbis 音乐 |
| MIDI-04 | [MIDI 与 OGG 分开](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/presentation.md#midi-04) | 不适用 | 本作使用单流 Vorbis 音乐 |

## 存档与 Replay

| 编号 | 检查项 | normal | 记录 |
| --- | --- | --- | --- |
| REPLAY-01 | [导入原版 Replay](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#replay-01) | 待验证 | 本次不以静态实现替代运行验证 |
| REPLAY-02 | [按原版格式导出可表达输入](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#replay-02) | 待验证 | 本次不以静态实现替代运行验证 |
| REPLAY-03 | [记录实际生效输入](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#replay-03) | 待验证 | 本次不以静态实现替代运行验证 |
| REPLAY-04 | [扩展格式与版本](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#replay-04) | 待验证 | 本次不以静态实现替代运行验证 |
| REPLAY-05 | [Replay 文件管理](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#replay-05) | 待验证 | 本次不以静态实现替代运行验证 |
| REPLAY-06 | [回放与实时输入隔离](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#replay-06) | 待验证 | 本次不以静态实现替代运行验证 |
| REPLAY-07 | [回放独立于显示刷新率](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#replay-07) | 待验证 | 本次不以静态实现替代运行验证 |
| SAVE-01 | [已声明的存储根](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#save-01) | 待验证 | 本次不以静态实现替代运行验证 |
| SAVE-02 | [用户文件允许范围](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#save-02) | 待验证 | 本次不以静态实现替代运行验证 |
| SAVE-03 | [明确持久化时机](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#save-03) | 待验证 | 本次不以静态实现替代运行验证 |
| SAVE-04 | [退出重开后数据保留](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#save-04) | 待验证 | 本次不以静态实现替代运行验证 |
| SAVE-05 | [离线读取已有数据](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#save-05) | 待验证 | 本次不以静态实现替代运行验证 |
| SAVE-06 | [游戏包与用户数据分开](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/storage.md#save-06) | 待验证 | 本次不以静态实现替代运行验证 |

## 语言与练习

| 编号 | 检查项 | normal | 记录 |
| --- | --- | --- | --- |
| PRACTICE-01 | [发布包中的练习能力](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#practice-01) | 不适用 | 该构建未提供 thprac |
| PRACTICE-02 | [开局前启用与语言](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#practice-02) | 不适用 | 该构建未提供 thprac |
| PRACTICE-03 | [游戏内练习状态](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#practice-03) | 不适用 | 该构建未提供 thprac |
| PRACTICE-04 | [关卡与资源参数](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#practice-04) | 不适用 | 该构建未提供 thprac |
| PRACTICE-05 | [练习 Replay 元数据](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#practice-05) | 不适用 | 该构建未提供 thprac |
| PRACTICE-06 | [触控功能键与指针](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#practice-06) | 不适用 | 该构建未提供 thprac |
| PRACTICE-07 | [练习语言跟随设置](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#practice-07) | 不适用 | 该构建未提供 thprac |
| PRACTICE-08 | [练习界面的可用功能](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#practice-08) | 不适用 | 该构建未提供 thprac |
| LANG-01 | [内置日语原文](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#lang-01) | 待验证 | 本次不以静态实现替代运行验证 |
| LANG-02 | [语言包身份与发布清单](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#lang-02) | 不适用 | 本作使用内置原文 |
| LANG-03 | [开局前挂载翻译与字体](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#lang-03) | 不适用 | 本作使用内置原文 |
| LANG-04 | [语言包失败时回到原文](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#lang-04) | 不适用 | 本作使用内置原文 |
| LANG-05 | [字体字符覆盖](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#lang-05) | 不适用 | 本作使用内置原文 |
| LANG-06 | [缺译资源使用原文](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/localization.md#lang-06) | 不适用 | 本作使用内置原文 |

## 多人接入与规则

| 编号 | 检查项 | normal | 记录 |
| --- | --- | --- | --- |
| MP-RULE-01 | [死亡、幽灵与救援](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-rule-01) | 不适用 | 单人模式 |
| MP-RULE-02 | [个人资源与团队资源](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-rule-02) | 不适用 | 单人模式 |
| MP-RULE-03 | [给予资源与反馈](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-rule-03) | 不适用 | 单人模式 |
| MP-RULE-04 | [对话、结算与全灭](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-rule-04) | 不适用 | 单人模式 |
| MP-RULE-05 | [自机可见性与多人HUD](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-rule-05) | 不适用 | 单人模式 |
| MP-01 | [独立 MP 产品变体](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-01) | 不适用 | 单人模式 |
| MP-02 | [房间人数、难度与机体](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-02) | 不适用 | 单人模式 |
| MP-03 | [所有席位准备并同步开局](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-03) | 不适用 | 单人模式 |
| MP-04 | [确定性输入与状态检查](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-04) | 不适用 | 单人模式 |
| MP-05 | [各席位输入归属](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-05) | 不适用 | 单人模式 |
| MP-06 | [旁观加入与输入隔离](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-06) | 不适用 | 单人模式 |
| MP-07 | [旁观只读取连续已确认历史](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-07) | 不适用 | 单人模式 |
| MP-08 | [慢旁观者与玩家隔离](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-08) | 不适用 | 单人模式 |
| MP-09 | [旁观尾帧与结束时机](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-09) | 不适用 | 单人模式 |
| MP-10 | [联机暂停与重开](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-10) | 不适用 | 单人模式 |
| MP-11 | [多人 Replay](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-11) | 不适用 | 单人模式 |
| MP-12 | [WebRTC 与 Relay 路径](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-12) | 不适用 | 单人模式 |
| MP-13 | [本地玩家可见性](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-13) | 不适用 | 单人模式 |
| MP-14 | [联机与回滚诊断](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-14) | 不适用 | 单人模式 |
| MP-15 | [游戏结束返回房间](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/multiplayer.md#mp-15) | 不适用 | 单人模式 |

## 验证与交付

| 编号 | 检查项 | normal | 记录 |
| --- | --- | --- | --- |
| VERIFY-01 | [全部内置 Demo 对照](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/validation.md#verify-01) | 待验证 | 本次不以静态实现替代运行验证 |
| VERIFY-02 | [长 Replay 样本](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/validation.md#verify-02) | 待验证 | 本次不以静态实现替代运行验证 |
| VERIFY-03 | [原版基准与内容哈希](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/validation.md#verify-03) | 待验证 | 本次不以静态实现替代运行验证 |
| VERIFY-04 | [读取固定基准](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/validation.md#verify-04) | 待验证 | 本次不以静态实现替代运行验证 |
| VERIFY-05 | [单独维护原版基准](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/validation.md#verify-05) | 待验证 | 本次不以静态实现替代运行验证 |
| VERIFY-06 | [逐逻辑帧比较](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/validation.md#verify-06) | 待验证 | 本次不以静态实现替代运行验证 |
| VERIFY-07 | [诊断构建与正常构建](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/validation.md#verify-07) | 待验证 | 本次不以静态实现替代运行验证 |
| VERIFY-08 | [本作状态映射与结束条件](https://github.com/Ritosa2333/eagler-touhou/blob/main/docs/adaptation/validation.md#verify-08) | 待验证 | 本次不以静态实现替代运行验证 |

<!-- manual-verification -->

## 当前发布与验证范围

- 两仓根源码树相同；monorepo 保留标准 Git subtree 来源父链，合并时使用 Create a merge commit。
- 运行时构建入口：`node portable/build-eagler.mjs`，输出代码型 `build-eagler/` 和 `runtime-files.json`。原版资产、字体、录像与编译产物不进入 Git。
- Launcher 接入源码包含托管 epoch / origin 校验、configure/resources/launch/keyboard、scoreth13.dat 与现有 localStorage 存档桥。原版资源通过 TH13PK01 容器装入 /assets，音乐保留 thbgm.fmt + 单流 thbgm.ogg。
- 定向命令 `node --test tests/rng.test.mjs tests/harness-paths.test.mjs tests/submission.test.mjs tools/run-child.test.mjs` 在新增接入前执行，20 项通过；不能证明后续修改。
- 全量 UI 命令 `TH13_TEST_WORKSPACE=/work node --test tests/ui.test.mjs` 失败。静态定位：结局测试在 op7 让帧后访问空记录数组，另有输入、MSG22、Continue、通关公式和存档版本的旧期望及资源夹具缺件。未为变绿修改这些测试或生产语义。
- 已静态修复结局脚本切换后的文字 VM 重建与 opcode 错误传播；按用户要求之后未运行任何测试、构建或浏览器。新增接入与修复尚无本轮运行验证。
- 实际 Launcher 启动、资源导入、键盘到开局、存档往返、Replay 和全游戏场景覆盖待验证；无独立原版 GPU 真值，最终输出认证保持 BLOCKED。

## 资源准备

在 monorepo 根目录使用 `node launcher/scripts/prepare-th13-content.mjs --raw=/path/to/extracted --original=/path/to/game --output=/path/to/assets-ogg`。提供合法的解包资源与完整连续 Vorbis 音乐流；支持 `thbgm.ogg` 或 `thbgmogg.dat`。标准 PCM thbgm.dat 不会被假装成 Vorbis。设置 `EAGLER_TH13_CONTENT_DIR` 指向输出目录。

## 维护边界

通过 `testOnly: true` 进入测试构建，不将未验证接入直接投入正式 Launcher 作品列表。PR 发布前按上游 CONTRIBUTING 的要求审阅标题与正文。
