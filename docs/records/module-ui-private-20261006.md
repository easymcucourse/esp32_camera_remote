# 2026-10-06 UI 私有实现接口与 Camera 死兼容清理

UI 公共目录只保留 app_ui.h：Core init/启动元数据、endpoint/renderer/benchmark/preference 停止、维护固定画面与健康信号。model/setters、菜单/状态类型、JPEG 绘制/恢复、调试合成输入归 private/app_ui_internal.h；扩展参数格式化 camera_settings.h 也收回 private。生产只有 UI 包含这些私有实现头；正常功能仍走消息契约。公共生命周期注释同步只读 preferences、永久 model 清空/冻结及停止失败语义，删除旧菜单/偏好 worker 注释。

Camera 删除无生产调用的 app_camera_quiesce/quiesce_release/messages_stop、camera_controller_forget/forget_pairing 及旧 maintenance_acquire/release 预约 primitive。仅历史 producer fixture 使用的旧辅助逻辑保留在 tests/support/legacy/camera_runtime_obsolete.c，不参与固件；原 fixture 断言不减少，不能把这部分历史预约行为当作当前生产 API。当前不可逆 close/stop/endpoint drain 仍验证真实生产函数。Web factory 继续通过共享 identity primitive 在正常 owners 停止后执行。

审计发现真实泄漏：app_camera_init 分配 focus queue 后，endpoint 启动失败时 messages_quiesce 因 messages_started=false 直接返回，没人释放队列。现在物理 stop 确认 exclusive_drained 后，即便没有成功启动 endpoint 也释放 focus queue。新增 Debug/Release partial 初始化失败场景，直接验证真实 init→endpoint失败→stop→quiesce、重复 stop 与不可重启，不调用历史辅助逻辑。

HOST 243/243（原 54 保留）；UI 当前 fixtures 改 include 路径，历史 fixture 使用其 legacy header。新增边界门禁拒绝 UI public implementation 头、跨组件 app_ui_internal，以及退休 Camera API 重新进入生产。LCD Default `0x3587b0`、Stable `0x3579a0`、Release `0x34c7a0`，三构建终态 0，均小于 5MiB。日志 `build/module-ui-private-{host-build,host,default,stable,release}.log`。ATOM 固件未改未重建；无烧录、实机、提交或推送。

本批完成指定旧接口清理并修复 focus 队列 partial-init leak。完整 goal active：其余公共契约/资源退出仍需全量审计，实际依赖/运行关系图、旧当前文档与 S/V/A38 完整验证仍待同步；真实硬件启动、安全切换、cache-off/稳定性未验证。

最终三 compile graph/NM 检查通过，见 build/module-ui-private-graph.log；boundary/doclinks（137 文档、557 链接、0 问题）与 diff 通过。新增 gate 后重跑完整 243 项通过，所有本批 handles 终态。
