# TH09 卡片与观战栏 WIP 截图验收

日期：2026-10-14。仅 TH09 卡片与观战播放栏，本轮未提交、合入或部署。

**Findings**

- [P2，已修复] 首字头像与选定样图差异过大。
  Location: `.th09-duel-avatar`。
  Evidence: `qa-c3` 中头像为粉色，桌面 24px / 手机 18px；样图为弱化的灰色圆底。
  Impact: 头像像额外强调的徽章，桌面比例过小。
  Fix: 保留原有昵称首字、准备与离线数据，改灰色圆底；桌面 44px / 手机 22px。`qa-c4` 两张卡片对照为修复后的证据。
- [P2，已修复] 手机播放栏上方与测试诊断信息重叠。
  Location: `.runtime-diagnostics-playback`。
  Evidence: `frontend-c2/playback-panel-mobile.png` 的右上角存在 ICE 文本。
  Impact: 分散注意，遮挡播放栏边缘。
  Fix: 诊断信息沿用现有播放栏尺寸观测，跟随实际高度移动；`frontend-c3/playback-panel-mobile.png` 和移动完整画面已无重叠，实际前端测试断言通过。
- [P2，已修复] 初版卡片的通用人物图标丢失原有昵称首字身份。
  Fix: 使用既有 `multiplayerDisplayInitial` 与 `lobby-seat` 状态；当前真实房间显示甲 / 乙，播放器显示 P1(甲) / P2(乙)。通用人物 SVG 只用于空席占位。
- [P2，已修复] 初版桌面加入按钮继承原列表 `width:100%`。
  Fix: 在 TH09 卡片内恢复自适应按钮宽度；最终桌面截图按钮位于右上，未挤占状态与对决布局。

**Comparison Target and Evidence**

证据根目录：`D:/workspace/eagler/worktrees/th09-managed-touch-4x/th09_web/artifacts/spectator-layout-20261014`。

| 对照 | 视觉来源 | 实现与视口 | 归一化 |
|---|---|---|---|
| 桌面卡片 | references/desktop-card.png，2076×757，卡片裁剪 1960×354 | cards-c5/1280-normal.png，视口 1280×460 | 两个卡片宽度均为 1000px；源高 181px，实际高 176px |
| 手机长名称卡片 | references/mobile-card.png，1774×887，卡片裁剪 1602×517 | cards-c5/390-long.png，视口 390×460 | 两个卡片宽度均为 350px；源高 113px，实际高 135px |
| 桌面播放栏 | references/desktop-player.png，1487×1058，面板裁剪 771×180 | frontend-c3/playback-marker-2.png，660×172，视口 1280×900 | 两个面板宽度均为 660px |
| 手机播放栏 | references/mobile-player.png，853×1844，面板裁剪 810×376 | frontend-c3/playback-panel-mobile.png，356×131，视口 390×844 | 两个面板宽度均为 356px |

浏览器实现截图为 deviceScaleFactor=1。裁剪坐标、实际像素、缩放比例记录在 `qa-c4/comparisons.json`。

完整组件对照：`qa-c4/desktop-card-comparison.png`、`mobile-card-comparison.png`。完整游戏画面对照：`desktop-player-full-comparison.png`、`mobile-player-full-comparison.png`。重点播放栏对照：`desktop-player-comparison.png`、`mobile-player-comparison.png`。原图和实现放在同一张对照图中审阅。

状态：卡片 2:1、正在对战、Normal、3 人旁观。长机签内容为明确标记的测试数据。真实房间卡片最终 1:0、已进行 2 分钟、2 人旁观。播放器桌面截图暂停于第 3 回合起点，手机截图为尚未收到回合标记的开局直播。样图使用 2:1 与占位昵称，动态值、进度位置和回合数量不作像素比较。

**Required Fidelity Surfaces**

- Fonts and typography: 明确复用原页面 ET Yatra / ET Chill Round 字体及字重，未引入样图的通用无衬线字体。字体规则由父页面进入同源游戏框架，实际浏览器等待字体加载后截图。比分强调、难度保留，机签保持单行省略；320/390/1280 宽度均检查。既有字体字形与样图的差异按用户“注意复用”保留。
- Spacing and layout rhythm: 两侧玩家对称，比分居中，状态在比分上方，时长在下方；手机旁观人数在右下角。手机卡片实测不超过 150px，不与长名称、比分或按钮重叠。桌面播放栏限制为 660px，比分/时长按最新要求在左上，收起按钮在右上。
- Colors and visual tokens: 使用原大厅深色、暖灰边框、粉色操作按钮；头像改灰色圆底，准备状态沿用原小标记。播放栏与触控 UI 保持深色半透明表面和浅紫未来区间；播放过的前段无填色。
- Image quality and asset fidelity: 原昵称头像为首字而非图片资源，真实身份内容按用户要求保留。空席、旁观人数、聊天、播放、暂停、回到直播、折叠图标使用既有 Phosphor 图标体系和 SVG 资产，无自绘人物或新增字体。包内图标均正常加载；图标按用途分别取尺寸。样图人物轮廓是占位数据，不覆盖真实昵称首字。
- Copy and content: 保留 Normal、“正在对战”、“已进行 N 分钟”和“X 人旁观”。外显名称只取可选机签，否则 P1 / P2；私有昵称不作为机签回退。播放器使用真实昵称首字 P1(甲) / P2(乙)。按后续“看着标”指示使用回合数字标记，已过去的标记禁用，后面的标记可以前跳；删去重复的下一回合操作。手机在没有回合数据时不展示空标记行。

**Comparison History**

1. cards-c1 / qa-c1：检查基础卡片结构、旧列表按钮继承、字体、移动长名称和空席。
2. cards-c2 / cards-c3 / qa-c2：收紧手机高度、限制昵称单行省略、调整桌面按钮和边框。通过实际组件浏览器截图检查。
3. cards-c4 / qa-c3：恢复原有首字头像，保留状态；用户指出粉色底与尺寸仍偏离样图。
4. cards-c5 / qa-c4：灰色头像和 44/22px 尺寸。重新截图并在相同视口对照，15 个状态通过。
5. frontend-c1：测试第二局前未展开播放栏，点击隐藏的暂停按钮导致测试失败；保留失败记录，修正操作顺序后 frontend-c2 通过。
6. frontend-c2：完整实际观战流程通过，手机重点截图发现诊断覆盖。
7. frontend-c3 / qa-c4：发布新的本地不可变预览，非空昵称复测通过，诊断不再遮挡；两个真实回合标记点击和跨整局连续观战均通过。

**Open Questions / Accepted Deviations**

- 机签的录入与身份功能属于未来功能。本轮只支持可选展示字段，截图中的长机签只存在于本地展示样例。
- 最新文字要求优先于旧样图：播放器标题左对齐、只保留关键操作、回合直接标在轨道上，因此面板高度和按钮数量存在预期差异。
- 暂停 / 直播的比分、分钟与进度均为真实数据，无强行填入样图的 2:1 和 44 秒。
- 测试模式诊断信息留在播放栏外；截图验证局限于本地 Chromium 实际流程和移动尺寸模拟，未新增真机或公网性能结论。

**Implementation Checklist**

- [x] 仅 TH09 替换卡片，其他作品保留原卡片。
- [x] 原昵称首字与房间状态映射经过真实房间验证。
- [x] 桌面 / 手机样图与实现同时对照，修复头像与遮挡问题。
- [x] 实际前端房间中途加入、暂停、手动快进、回合标记、收起、聊天、跨局、自动记胜负与分钟重置通过。
- [x] 字体与 SVG 正常加载，实际前端 pageerror 为 0；最终预览控制台为空，卡片预览只有 React 开发提示，没有警告或错误。

**Follow-up Polish**

- [P3] 如未来机签提供完整编辑流程，可在相同字体与尺寸下再验收真实长名称分布；本轮已有中英长文本及转义样例。

final result: passed
