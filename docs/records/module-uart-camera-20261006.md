# 2026-10-06 UART 相机命令与并行状态

main/camera_console.c 去除 Camera/UI/设置/偏好私有头及所有业务状态/动作直连。main/camera_commands.c 只编码 Camera START(pair/preview)、FORGET、STOP、MF_CANCEL、UI_MENU_ACTION 和 DISPLAY_FAULT。s 使用 UART-only admission ACK：确认停止请求已经接收，不宣称物理排空；Core 默认 STOP 继续等待 owner/lease 排空。S 切换设置后即使 UI 回复失败也尝试 MF_CANCEL，安全取消有独立有界期限。错误不再被格式化为 OK。

router 新增 request_many，共用原 scalar admission/correlation/epoch/deadline/lease 清理；所有 request 发出后才开始等待。逐项返回 transport 状态，endpoint result 留在 reply；一个端点超时不撤销其他已完成快照，等待不延长原绝对期限。主机实际 router 验证等待前两个 endpoint 已收到请求、倒序回复、lease 延迟归还及同期限一个超时另一个成功，原 scalar/cancelable 测试保持。

main/uart_status.c 对 Camera/UI/Input/capabilities/preferences/System 的六份快照并行请求，任一失败输出错误而不把零值冒充状态。UART 单任务独占静态 envelope 数组，避免内部 4096 字节栈承载十对消息；每次归还全部 reply。保留 OTA/heap SDK 诊断格式，但不再调用维护模块查询。extra status 使用 UI_PROPERTY_STATUS 读取九个显示参数的语义快照，UART 输出 property 名称，不携带厂商 code；UI 验证来源/lease/flags/范围/代数/期限。十个 UI 请求使用同期限，UI 原 control 深度16保持。

新增 uart_camera_commands、uart_status 测试并扩展原 router/Camera endpoint/UI menu 回归，保留原断言及 -Werror。覆盖 pair/preview/stop/forget/UI-MF 顺序、错误和 fault；状态/负 EV/SIM 不伪造 ATOM/OTA 状态、各端点 transport 与业务错误、extra 语义值、UI 过期/非法来源/代数/范围。

这是完整 gateway 的持续迁移，编码器尚在 main；maint/bench 旧调用、独立 UART 生命周期与 app_console 整体归属仍待完成。System mode 目前仍是旧组合根的占位快照，不证明旧维护的实时状态；最终 STARTUP/NORMAL/MAINT 状态机和独占维护尚未实现。未修改 ATOM、未烧录或实机验证。证据日志为 build/module-uart-camera-{host-build,host,default,stable,release}.log。

router 同时收紧 REQUEST 入队：关联槽/source/target/type/generation/两端 epoch 必须仍有效，防止 admission 与 enqueue 两次锁之间 endpoint 重启后执行旧请求。主机模拟该交错，确认拒绝、归还 lease 且新 endpoint 没收到旧请求。

最终验证：host 103/103；LCD Default `0x35be60` / Stable `0x35b040` / Release `0x34f1a0` 三构建成功，均小于 5 MiB；boundary/doclinks/diff 门禁通过（112 文档、506 链接、0问题）。全部本批构建会话已结束。ATOM 本批未改未重建，前批独立双模证据保留。
