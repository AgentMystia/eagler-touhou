# Web 测试

`test-plan.mjs` 调度核心测试，`run-ui.mjs` 调度 UI 单元组和产物组，`browser/` 与根测试脚本覆盖实际浏览器。正式测试范围取自 Git 受管文件；开发者的未跟踪实验不应阻止项目检查。

修改模型使用当前行为测试；修改 DOM 生命周期使用 mounted tests；修改离线、资源安装或 Runtime 启动使用真实浏览器。合成 Runtime 只证明所模拟的协议行为。具有原版数据、字体或诊断产物要求的测试应显式提供输入。

`tests/ui/` 不读取历史界面源码作正确性基准。样式、函数个数和源文件哈希不是业务验收；协议数据和实际文件身份可按真实边界检查。

命令及证据分级见 [Web 开发与验证](../docs/DEVELOPMENT.md)和[项目验证流程](../../../docs/playbooks/verification.md)。
