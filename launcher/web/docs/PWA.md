# PWA、离线运行与更新

PWA 安装把 Launcher 作为应用入口。离线游戏还需要完整缓存的 Runtime 和已安装的 DATA、字体、语言及所选音乐。多人连接和新的资源下载仍需要网络。

## 拥有者

| 状态 | 拥有者 |
| --- | --- |
| 安装提示与持久存储请求 | Launcher 的用户操作 |
| App Shell 注册与更新 | `src/launcher/app-shell-client.mts` 和 `app-shell-sw.js` |
| 游戏 Runtime 缓存与选中代次 | Runtime cache / generation 模型 |
| DATA、音乐、语言组件 | Package Store |
| 用户存档和 Replay | 用户文件与持久化存储 |

`src/app-shell-sw.js` 和 `src/runtime-cache-sw.js` 合成唯一缓存 Worker；PWA UI 不注册第二个 Worker。安装或 `persist()` 成功都不保证用户数据永不被清除，用户仍应能导出存档。

## 更新

候选 Shell 先验证并缓存自己的完整 Launcher 文件。活动游戏和资源操作推迟当前页面的切换；替换 Worker 与当前页面是否可以重载分别处理。运行中的旧标签页和 iframe 必须继续持有自己的资源代次。

Shell 更新不下载所有作品的 Runtime。Runtime 按用户准备或启动的作品选择完整代次；未选作品的缺失不应阻止 Launcher 更新。首次访问、已等待的候选、激活中断和重新加载各自需要观察实际状态，不能用一个定时器代替激活或首帧。

资源摘要检查代码、WASM 和依赖是否属于同一代次。坏的新候选、断网或部分下载失败时，继续使用完整旧代次；不能把新 JS 与旧 WASM 混合。安装中的 Package 也只有完整校验后才能成为 current。

## 排错

先区分：Launcher Shell 未缓存、选中 Runtime 未准备、游戏 Package 缺项、存储被清理，还是当前网络路径不可用。记录失败文件、代次与阶段，保留存档后再处理可重建缓存。具体模型见[缓存与兼容](CACHE_AND_COMPATIBILITY.md)。

## 验证

从 Web 目录运行：

```text
node tests/runtime-cache-cases.mjs
node tests/shell-handoff-cases.mjs
python tests/browser/test-pwa-boot.py --browser=chromium
python tests/browser/runtime-recovery-cases.py --browser=chromium
```

浏览器依赖按 `tests/requirements-browser.txt` 的锁安装。实际用例验证新旧资源代次、损坏候选回退、原服务器断连、离线新浏览器进程、多个标签页和嵌套路径。合成 Runtime 验证资源与启动协议，不代表真实游戏、手机或实际飞行模式通过。

真实交付还需目标浏览器/设备上完成安装、游戏资源准备、离线启动、进程关闭后重载、后台音频恢复和中断更新。结果保存实际产物身份和环境；Shell、Runtime、Package 与游戏各自的验证范围分开记录。
