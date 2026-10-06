# 2026-10-05 main 模块拆分基线与阶段回归

用户目标：按根目录 main-module-split-plan.md 整理代码。基线 HEAD：01d6b4e920e7fff2c9d4b9eb9f544d7524083365. 原工作区只有用户提供的拆分计划未跟踪，未发现其他业务改动。本轮未提交 / 推送 / 烧录。

## 实施边界

本轮落实阶段 0 和阶段 1 的显示边界、UI model / 页面 / JPEG renderer 以及 health / restart component。阶段 1 尚未整体完成，后续消息与功能 component 迁移未完成，见 [拆分进度](../development/module-split-status.md)。

协议、按键映射、NVS 格式、OTA 分区、页面布局、JPEG buffer 与任务参数保持原设置。字体资产及 OFL 许可证一起移动；dependencies.lock 仅因 manifest 目录迁移更新 hash，版本不变。

## 原任务与启动基线

下表由基线 HEAD 的 xTaskCreate 调用提取；不是 SDK 内部 HTTP / Wi-Fi 任务的完整清单。重构后逐项比较创建函数及全部实参，完全一致（只允许文件位置和空白改变）。这证明源码任务属性不漂移，不证明硬件时序和运行稳定性。

| 任务 | 原位置 | 栈（bytes） | 优先级 | 核心 | 分配属性 |
| --- | --- | --- | --- | --- | --- |
| lcd_status | `components/board_7b/board_7b.c` | `32768` | 2 | 1 | `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` |
| health | `main/app_main.c` | `4096` | 2 | 未绑定 | `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` |
| atom_link | `main/atom_link.c` | `3072` | 4 | 未绑定 | IDF 默认内部 RAM |
| camera_nvs | `main/camera_controller.c` | `4096` | 4 | 未绑定 | IDF 默认内部 RAM |
| jpeg_decode | `main/camera_controller.c` | `32768` | 4 | 1 | `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` |
| camera_pair | `main/camera_controller.c` | `32768` | 4 | 0 | `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` |
| display_bench | `main/display_bench.c` | `32768` | 4 | 1 | `MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT` |
| lcd_pad_player | `main/lcd_sim.c` | `3072` | 3 | 未绑定 | IDF 默认内部 RAM |
| maint_ctl | `main/maint_mode.c` | `3072` | 2 | 未绑定 | IDF 默认内部 RAM |
| maint_probe | `main/maint_probe.c` | `ota?8192:6144` | 2 | 未绑定 | `(ota?MALLOC_CAP_INTERNAL:MALLOC_CAP_SPIRAM)|MALLOC_CAP_8BIT` |
| ui_preferences | `main/ui_preferences.c` | `3072` | 2 | 未绑定 | IDF 默认内部 RAM |
| wifi_config | `main/wifi_ap.c` | `4096` | 2 | 未绑定 | IDF 默认内部 RAM |
| wifi_menu | `main/wifi_menu_ui.c` | `4096` | 2 | 未绑定 | IDF 默认内部 RAM |

原 LCD 启动顺序：NVS → Wi-Fi 配置读取 → I²C 扩展器 / 面板供电 → 字体与双 framebuffer 初始化 / 背光 → lcd_status → Wi-Fi 菜单 / UI 偏好 → ATOM → Wi-Fi → 维护控制 → Console → 相机 → 显示基准 ready → OTA startup ready → health。重构后保持次序，资源初始化回调在扩展器上电之后、面板首次绘制之前执行。main 返回后 IDF 回收初始化栈。

## 原头文件调用清单

清单限定为基线 main 与 board_7b 生产 include；主机测试仍按 CMake 注册，未删项。

| 原 main 公共 / 内部头 | 原 include 调用方 |
| --- | --- |
| `main/app_restart.h` | `main/app_main.c`, `main/app_restart.c`, `main/maint_ota.c`, `main/maint_probe.c`, `main/maint_web.c` |
| `main/atom_link.h` | `main/app_main.c`, `main/atom_link.c`, `main/camera_console.c`, `main/display_bench.c`, `main/lcd_sim.c`, `main/maint_web.c`, `main/ui_preferences.c` |
| `main/camera_actions.h` | `main/camera_actions.c`, `main/camera_controller.c` |
| `main/camera_console.h` | `main/camera_console.c`, `main/camera_controller.c` |
| `main/camera_identity.h` | `main/camera_controller.c`, `main/camera_identity.c`, `main/wifi_ap.c` |
| `components/app_camera/private/camera_link.h` | `main/camera_controller.c`, `components/app_camera/camera_link.c` |
| `components/app_camera/private/camera_menu.h` | `main/camera_controller.c`, `components/app_camera/camera_menu.c` |
| `main/camera_pair.h` | `main/app_main.c`, `main/atom_link.c`, `main/camera_console.c`, `main/camera_controller.c`, `main/display_bench.c`, `main/maint_mode.c`, `main/maint_ota.c`, `main/maint_web.c`, `main/wifi_ap.c` |
| `main/display_bench.h` | `main/app_main.c`, `main/camera_console.c`, `main/display_bench.c` |
| `main/factory_reset.h` | `main/factory_reset.c`, `main/wifi_ap.c` |
| `main/focus_input.h` | `main/focus_input.c` |
| `main/gamepad_input.h` | `main/atom_link.c`, `main/atom_link.h`, `main/camera_actions.h`, `main/camera_pair.h`, `main/gamepad_input.c`, `main/maint_mode.h`, `main/wifi_menu_ui.h` |
| `main/lcd_sim.h` | `main/atom_link.c`, `main/camera_console.c`, `main/display_bench.c`, `main/lcd_sim.c` |
| `main/liveview_pipeline.h` | `main/camera_controller.c`, `main/liveview_pipeline.c` |
| `main/maint_auth.h` | `main/maint_auth.c`, `main/maint_web.c` |
| `main/maint_confirm.h` | `main/maint_mode.c` |
| `main/maint_json.h` | `main/maint_json.c`, `main/maint_web.c` |
| `main/maint_mode.h` | `main/app_main.c`, `main/atom_link.c`, `main/camera_console.c`, `main/camera_controller.c`, `main/display_bench.c`, `main/maint_mode.c`, `main/maint_ota.c`, `main/maint_probe.c`, `main/maint_web.c` |
| `main/maint_notice.h` | `main/maint_mode.c` |
| `main/maint_ota.h` | `main/app_main.c`, `main/camera_console.c`, `main/maint_mode.c`, `main/maint_ota.c`, `main/maint_web.c` |
| `main/maint_probe.h` | `main/camera_console.c`, `main/maint_probe.c` |
| `main/maint_wifi.h` | `main/maint_web.c`, `main/maint_wifi.c` |
| `main/ota_header.h` | `main/maint_ota.c`, `main/ota_header.c` |
| `main/ota_health.h` | `main/maint_ota.c` |
| `main/restart_schedule.h` | `main/app_restart.c` |
| `components/app_camera/private/setting_control.h` | `main/camera_controller.c`, `components/app_camera/private/camera_menu.h`, `components/app_camera/setting_control.c` |
| `main/ui_preferences.h` | `main/app_main.c`, `main/atom_link.c`, `main/camera_console.c`, `main/maint_probe.c`, `main/maint_web.c`, `main/ui_preferences.c`, `main/wifi_ap.c` |
| `main/wifi_ap.h` | `main/app_main.c`, `main/camera_controller.c`, `main/maint_probe.c`, `main/maint_web.c`, `main/wifi_ap.c`, `main/wifi_console.c`, `main/wifi_menu_ui.c` |
| `components/app_wifi/include/wifi_apply.h` | `main/wifi_ap.c`, `components/app_wifi/wifi_apply.c` |
| `components/app_wifi/include/wifi_config.h` | `main/factory_reset.h`, `main/maint_wifi.h`, `main/wifi_ap.h`, `components/app_wifi/include/wifi_apply.h`, `components/app_wifi/wifi_config.c`, `main/wifi_menu.h` |
| `main/wifi_console.h` | `main/camera_console.c`, `main/wifi_console.c` |
| `main/wifi_menu.h` | `main/wifi_menu.c`, `main/wifi_menu_ui.c` |
| `main/wifi_menu_ui.h` | `main/app_main.c`, `main/atom_link.c`, `main/wifi_menu_ui.c` |

## 验证

以下结果由本轮实际命令取得，构建日志位于忽略的 build/ 目录。

- 原主机基线：cmake -S tests/host -B build/host；cmake --build build/host -j 4；ctest --test-dir build/host --output-on-failure，54/54 通过。
- 当前同一 host 构建命令：58/58 通过；保留原 54 项，新增 display_surface、初始化失败、UI model 与 module_boundaries。原测试逻辑仅做路径 / API 改名，不减少断言与告警。
- Default / Stable：tools/idf.ps1 build 与 tools/idf.ps1 build -Profile stable 完成；最终 C 文件命名及尺寸检查后的增量通过 cmake --build build -j 4、cmake --build build/stable -j 4 完成。两种均为 debug Kconfig，Stable 不是 Release 的别名。
- Release：激活 ESP-IDF 后运行 python tools/ci_build.py lcd release 通过，关闭模拟 / 故障注入、OTA rollback 和 5 MiB 门禁检查通过；nm 未发现模拟器 / JPEG encoder 测试符号。
- 原 Release 基线在 build/module-split-baseline-source/ 中用 git archive HEAD 的独立源码快照运行同一 CI 命令；未修改当前工作区。独立源码快照没有原 Git 版本元数据，不作为逐字节镜像对比。
- python tools/check_doc_links.py：74 文档、393 本地链接、0 问题；python tools/check_module_boundaries.py 与 git diff --check 通过。
- 三份字体二进制、三份许可证与 manifest.json 均与基线 git blob 的 SHA256 一致，ui_fonts.c / camera_settings.c 仅迁移路径。
- 可选 Windows 字体预览命令尝试未执行成功：当前默认 Python 缺少 freetype 包。本轮未安装依赖，未声明字体主机视觉预览通过。

| 配置 | 基线大小 | 当前大小 | 增量 |
| --- | --- | --- | --- |
| Default debug | 0x3515d0 (3478992 bytes) | 0x351c80 (3480704 bytes) | +1712 bytes |
| Stable debug | 0x3507b0 (3475376 bytes) | 0x350e60 (3477088 bytes) | +1712 bytes |
| Release（CI portable defaults） | 0x344d50 (3427664 bytes) | 0x3453a0 (3429280 bytes) | +1616 bytes |

日志：build/module-split-host.log、build/module-split-default-complete.log、build/module-split-stable-complete.log、build/module-split-release-complete.log；对应 baseline 日志使用 module-split-baseline-* 前缀。

新增测试覆盖：画布二次 acquire、锁 / 租约超时、取消后重取、复制 / 过期句柄拒绝、双 buffer 轮换、刷新失败阻止写入、恢复成功 / 失败、初始化失败；真实 UI model 的状态快照、负 EV、导航顺序、首尾循环、扩展参数与重复通知。硬件后端及 JPEG 流水线的原测试仍保留。

显示私有头越界检查纳入 CI 和 CTest。主机模拟未验证真实 JPEG decoder / DMA 扫描时序；构建 / 主机测试通过，硬件待验证。没有 LCD 实机冒烟及 30 分钟稳定性结论。
