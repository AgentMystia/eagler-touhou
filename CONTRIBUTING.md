# 参与开发

先阅读[工程架构](docs/ARCHITECTURE.md)和[开发指南](docs/DEVELOPMENT.md)，确定改动的状态与产物由哪个组件拥有。游戏使用根目录 Git 子模块，Web、协议和服务使用根 npm workspace。

## 环境与验证

源码 Node 要求以根 package.json 为准，从根目录执行 npm ci --ignore-scripts。Python、Emscripten、CMake、浏览器和原版资源按实际任务提供；原版 DATA、个人文件和本地配置不提交。

选择能证明本次行为的验证：模型/生命周期、原生逻辑、真实浏览器、Runtime 或交付产物。完整入口为 npm run check；需要原生组件时运行 npm run check:workspace。细分命令和证据边界见[验证 playbook](docs/playbooks/verification.md)。

测试保护实际行为、数据格式、协议、ABI 和发布边界，不固定正常重构会改变的源文件字节、函数数量或整份样式哈希。对于失败先复现并判断测试是否有效，不让生产代码迁就无意义断言。

## 改动边界

- Web UI、会话和资源安装位于 launcher/web。
- 共同数据格式位于 contracts；C++ 公共算法位于 eagler-common。
- 房间/中继/遥测位于 services/netplay。
- 游戏对象、规则和原生 Replay 语义留在对应游戏。
- 通用构建与交付使用 Node/Python/CMake，平台特有工作保留必要适配。

按实际消费者抽取公共模块。资源准备、Runtime 构建、站点组装与部署分别处理，不在 Host 中隐式重建游戏。

## 提交与说明

保留无关未提交工作。子模块先提交，再由主仓库记录新的 gitlink；不要为了完成主仓库提交而覆盖子仓库工作。提交文案简短中文，不加类型前缀。

提交前检查差异、生成物与文档链接。公开说明写清问题、改变、实际验证及未覆盖范围，不能把结构检查写成完整游戏验收。按改动更新架构、playbook 和能力进度；历史过程由 Git 保留。

生成文件、工具链、缓存、原版资源、存档、密钥和私有运行记录不作为源码提交。推送、公开发布和部署按项目明确流程执行。
