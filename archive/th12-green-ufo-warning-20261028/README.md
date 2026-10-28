# Extra 绿 UFO 激光预告线

TH12 普通版与多人版恢复 Extra 绿 UFO 发射前的细预告线。旋转 ANM 四边形在提交 WebGL 顶点时补齐 D3D9 到 WebGL 的半像素转换，保留原脚本的线宽、颜色、透明度和旋转。

## 定位与修复

原版 `stage07boss.ecl` 的 `BossCard6Laser` 绑定 `stgenm07.anm` 脚本 48。其子脚本 44 使用 sprite 51：512×32 的区域、`scale(1, .032)`、左侧锚点、mode 1 和加法混合。预告线由动画子节点绘制。

原有 mode 1 几何保留原版 CPU 顶点坐标，但 WebGL 像素中心与 D3D9 相差半像素。约 1 像素宽的水平预告线因此采样到纹理上下透明边缘。修复在 `Renderer.region` 提交 mode 1 顶点时统一增加 `(0.5, 0.5)`；mode 0 已通过现有取整路径处理该转换。

像素中心依据：[Microsoft 坐标系说明](https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-coordinates)。原版顶点依据为两侧既有的 `native-quad-original.json`；逻辑坐标和 CPU 顶点计算继续沿用该夹具，WebGL 提交测试核对转换后的坐标。

## 验证

- Chromium WebGL2 使用本地原版贴图，水平预告线峰值亮度由 6 恢复为 254；结果见 [retail-pixels.json](retail-pixels.json)。原版资源未进入提交。
- 两侧新增浏览器回归覆盖水平、竖直、斜向预告线和完整、部分、零透明度，并检查绘制过程不改变动画状态。旧代码在水平线用例失败，修复后通过。
- 单人、多人共 126 项绘制相关测试通过，覆盖原版四边形、浏览器预告线、批处理、图层、相机、背景表面、符卡、UFO、呈现连续性和节奏。

范围为绘制修复与局部浏览器验证，未发布站点。普通版和多人版在同一主分支基点上各自修改所属目录，提交不引入目录投影历史。
