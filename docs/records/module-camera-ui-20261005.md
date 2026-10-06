# 2026-10-05 Camera / UI 语义消息

本批建立 Camera 发布端和 UI 消费端的语义消息边界；新 Camera producer 尚未接入生产，完整计划未完成。

- app_message 增加完整属性快照、连接阶段、命令状态及语义控制编号。快照只含语义值、可写/目标/状态与能力，不携带 Sony 属性码、backend 指针或枚举借用指针。通用 backend 追加只读 FLASH 属性，已有 setting 编号不变。
- app_camera 的 camera_outputs 从通用 property/kernel 构建快照；invalid properties 清可写与目标；missing extra 显式未知。发布时复制小快照到只读 lease，send 失败也按现有 router 所有权契约释放，最后引用归还才 free。状态与命令状态使用内联值。
- app_ui 的 ui_camera_messages 只由 UI endpoint 调用，整份检查属性编号与状态、非零/旧代数、来源和只读 lease 后更新 model；不完整/非法消息不会部分修改或推进代数。连接字符串须终止，LIVE/STOPPED 保留当前画面，未知录制状态保留最后已知红色。帧也先验证非空只读 lease 再接受代数。
- UI 订阅 Camera 属性/状态/能力/命令/帧事件；boundary scanner 增加 UI 禁止包含 Camera/backend/PTP/Sony/Wi-Fi/维护实现头。旧生产 controller 的直接 UI 调用仍待删除，本批不声称已满足全部跨组件消息规则。

主机 76/76 通过，原 54 保留。新增 UI fake Camera 测试不链接 app_camera/PTP/Sony，Camera 输出 fake Console 测试不链接 UI/PTP/Sony/Wi-Fi：signed EV、目标状态、电量、只读 Flash、missing extra、caps/未知录制、代数/非法整体拒绝、copy immutable、lease 最后释放与 send 失败清理。

Default 0x355ea0 / Stable 0x355080 / Release 0x349520 构建成功，三镜像均 <5MiB，Release 禁止模拟/JPEG encoder 符号不存在。日志 build/module-camera-ui-{default,stable,ci-lcd-release,host,host-build,symbols}.log。边界扫描、文档链接、git diff --check 通过。未烧录/提交/推送，实机效果与长时间稳定性待验证。

下一步：Camera endpoint/facade/producer 使用 session/discovery/properties 和当前语义输出；移除旧 fd/PTP transaction、直接 Wi-Fi/UI 路径；继续身份、动作、安全释放及 frame 槽租约，然后输入/独占维护/UART/组合根及兼容清理。见[完整清单](../development/module-split-checklist.md)。
