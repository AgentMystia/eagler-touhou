# 语言与练习集成

本页说明 Host、Web 与游戏 Runtime 之间的语言和练习边界。可用游戏及能力以根 Product Catalog 和本次 Runtime Release 为准，不在文档再维护一份授权列表。

## 语言资源

`lib/content-definition.mjs` 声明各作准备方式。Host 取得运营者选择的 thcrap 输入，编译支持的图像、文本、消息和布局资源，生成有字节身份的静态语言包。站点只发布实际准备并选择的语言；Web 校验并挂载资源后才启动游戏。

Runtime 读取安装好的本作文件树和语言选项，不持有任意 thcrap URL 或 CDN 策略。缺失翻译按对应游戏的资源回退规则处理。日文原作字体和 Unicode 翻译字体分别准备；编译成功不代表缺字、对话推进和界面布局已验收。

`server/thcrap-service.mjs` 的在线编译端点用于开发/诊断，不是静态发布 Runtime 的外部依赖。

## 练习

Product Catalog 和具体 Runtime Release 都支持时，Web 在启动前发送 `configure` 的 `options.thpracEnabled` 和 `options.thpracLocale`。游戏拥有练习参数、章节/帧定位、资源、重开和 Replay 元数据，Web 不以模拟状态代替游戏生命周期。

实际选项由 `integrations/thprac.mjs` 和各作实现约束。基础练习不等于完整上游 thprac。TH06/TH07 可以使用显式 ImGui 依赖构建基础练习；额外 runtime 扩展须单独提供和验证。

已有练习 Replay 使用游戏自身的嵌入数据，不新增未发布的 `*.rpy.thprac.json` 旁文件约定。更改格式或重开流程时，验证编码/解码、旧 Replay 读取和真实游戏重开；不要检查源代码片段出现次数。
