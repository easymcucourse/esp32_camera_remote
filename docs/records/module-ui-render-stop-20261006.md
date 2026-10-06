# 2026-10-06 渲染停止与固定维护画面

新增私有 ui_render_lifecycle，关闭绘制入口后等待已有绘制与通知调用归还，以及连接页刷新 task 自退出。通知调用计数保护 refresh_task 的生命周期，退出前等待计数归零；关闭后的连接页/JPEG/Debug JPEG/恢复/fault/刷新通知拒绝执行。原连接页 task 32768 PSRAM、core1、priority2、250ms通知等待不变。超时保持关闭状态和仍在使用的资源，不强删 task、不撤销画布 lease，后续调用继续等待；只重启可恢复正常绘制。

Core 重启在 Input/providers/Camera/bench/menu/preferences/UI endpoint 已停后调用 app_ui_renderer_quiesce。它在 display mutex 下释放 JPEG work/cache/decoder，保留原 panel、两个 framebuffer、字体和 mutex。新增 Core-only app_ui_enter_maintenance 在同样前置条件下使用已有画布及字体，黑底仅居中白色 MAINTENANCE；失败保留关闭状态，重试恢复时 prepare 回调也仅画 MAINTENANCE，成功重复调用不再绘制。没有新增生产 framebuffer 或像素持久副本。48px 在原字体8..96范围内。

固定画面 API 尚未接入 Core 的最终独占维护切换，因此当前 ELF 链接回收了未引用的 app_ui_enter_maintenance；三种 ELF 均保留生产 renderer_quiesce/enter/close 路径。不能据此宣称启动网页触发维护已实现。模型兼容 setter 尚未全部删除，现阶段关闭的是显示入口；S3.5 仅部分完成。最终 Core 必须先停止所有模型/帧/bench producers 和 workers，再设置固定画面。当前 Core 重启超时仍最终重启，不是维护进入成功证明。

新增 ui_render_stop fixture 编译真实 ui_renderer.c 和 lifecycle/model，验证原 task 参数、已有绘制期间停止超时、关闭后拒绝通知/恢复/连接页、刷新 task 自退出、JPEG reset、publish失败 lease 归还、恢复重试只画固定文本、成功后幂等及模型变化不能刷新画面。字体使用 fake 测量和单像素 raster，JPEG reset 使用 fake；不能证明实际字体视觉、SDK JPEG释放、SMP/RTOS/cache-off与硬件时限。恢复/发布后端有各自耗时，参数提供入口等待期限，不宣称整个硬件切换严格在该期限完成。

主机113/113通过，保留原54注册和旧回归断言；-Werror保持。LCD Default `0x35ba00`、Stable `0x35abe0`、Release `0x34f920` 编译成功，均低于5MiB及6MiB app分区。Release ELF 无 SIM/bench/JPEG encoder/Debug fault/维护UART探针符号；模块边界及 diff 检查通过。日志 build/module-ui-render-stop-{host-build,host,default,stable,release}.log。

ATOM本批无修改未重建，前批双模Debug/Release证据仍在独立UART记录。所有本批构建会话已结束，无本批后台串口/构建；其他聊天未检查。未烧录、未实机、未提交推送。完整计划目标仍 active，后续 Core组合根/全局配置jobs停止/启动SoftAP:80无认证不可逆独占维护/兼容API清理仍必须完成。

最终文档检查118文档/518链接/0问题，diff检查通过。
