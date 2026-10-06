# 开机图片包与画布预分配 — 2026-10-06

用户要求开机分配 512 × 2 图片包内存及画布，运行不动态分配。按每包 512 KiB 实施。

- Camera：`camera_runtime.c` 将原相机 worker 的 1 MiB × 2 改为 `app_camera_init` 开机分配 512 KiB × 2；分配失败回滚缓冲与 focus queue。worker 停止/重连保留原内存，仍等待 readonly lease 与结果 metadata 全部归还。PTP 对象不能超过槽容量，不扩容。
- UI：`ui_jpeg_renderer.c` 新增开机初始化 4096 字节 PSRAM 工作区和唯一 RGB565 decoder；帧、模式切换、错误及恢复路径保留资源，每帧重新解析 header。`ui_renderer.c` 在启动刷新任务前初始化。
- LCD：两块 1024×600 RGB565 画布由原 RGB SDK 在开机创建，合计 2,457,600 字节。`board_lcd_recover` 只对原 panel 重启/复用，三次恢复失败保留内存并交 Core 重启，不再 delete/create。初次启动失败仍清理半初始化资源。
- 范围：生产取景图片包、画布与解码工作区；Debug benchmark 的独立合成 JPEG、字体缓存、消息 metadata 与任务/SDK 其他分配不属于这两个取景槽。

验证：`cmake --build build/host -j 4` 成功；`ctest --test-dir build/host --output-on-failure` 263/263；`./tools/idf.ps1 build` 成功，应用 0x358690；`python tools/check_module_boundaries.py` 通过；`git diff --check` 通过。日志 `build/boot-buffers-host-build.log`、`build/boot-buffers-host.log`、`build/boot-buffers-idf.log`。最初编辑 LCD 恢复分支留下旧分支导致构建失败，已修正；原动态分配的 fixture 预期同步改为开机失败和运行复用，不把最初失败当通过。

未烧录、未实机验证。decoder/SDK 内部实现、超过 512 KiB 的真实相机对象、坏帧后真实 codec 恢复以及长期稳定性待验。
