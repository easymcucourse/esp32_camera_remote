# 2026-10-05 消息总线基础与初始 endpoints

用户目标升级为“完成完整计划并编译通过”，goal 保持 active。前一轮为实际进展；本轮继续落实原计划，没有缩小最终目标。

## 实际修改

- 独立 app_console，只依赖 FreeRTOS / esp_timer 和 common 值类型；不连接业务门面。versioned app_message.h 包含 Camera、Wi-Fi、Input、UI、System 语义 contract；公共帧/通道消息不含 socket/后端指针。
- 固定容量 endpoint control/bulk inbox；发送直接执行路由并入队，不调用业务 handler。独立内部 RAM message_router 任务（4096/priority5、25ms housekeeping）回收超时队列 / 取消 waiter；UART 故障不会停止它。
- 16 个 bounded pending waiter，32 个 buffer lease。correlation / domain generation / 注册 epoch / deadline 验证；endpoint stop/restart 取消旧 waiter 与旧 inbox。control 优先读取，队列满非阻塞返回失败。
- 发布者 / 订阅者的 lease 引用显式交接；多订阅最后一个消费者归还，pool 耗尽不夺走调用方 buffer；接收完成 / 错误 / 迟到 / 过期 / 停止均释放。正在被消费者使用的 lease 不强制撤销，quiesce 在其释放前返回超时。
- pad 的纯值类型提取至 common/pad_types.h，不改变原 gamepad state machine / 映射；主机 include 路径补充 common。
- app_core_messages_start 在 ATOM/Wi-Fi/相机前启动 router / System / UI；health 原有任务消费 System status/restart，UI 新 CPU1/priority4/32768 PSRAM endpoint worker 消费 UI status/frame/fault。前期正常 UI/JPEG 直接路径尚保留，Camera 等尚未注册。
- UI / System 生命周期、业务门面与维护入口迁移未完成；不能把已有 FRAME handler 当成生产取景已经过 message。
- 原任务创建实参未改，但新增两个任务：message_router 与 ui_endpoint。最终 JPEG worker 所有权转移后需统一记录任务表及资源开销，硬件时序仍未验证。

## 验证

- host 59/59：原54保留；新增 fake endpoint 测试独立编译 router/message，不链接 app_camera/UI/Wi-Fi 等。覆盖 request/reply、nested RPC、type/generation mismatch、迟到 reply、timeout、endpoint restart、控制优先、fanout 部分队列满、pool exhaustion、停止期间拒绝发送、in-flight lease quiesce、同一请求对象 reply。
- Default / Stable / Release 使用现有 build 目录的 cmake --build ... -j4 通过；最新大小见 build/module-router-default.log、module-router-stable.log、module-router-release.log。
- check_module_boundaries.py 补充 router 禁止 include 功能域头；实际扫描通过。原字体/协议及持久化未改，本轮不烧录。
- 三种固件均小于5MiB；生产模拟符号检查在后续命令日志中记录。主机 fake scheduler 不是实际 RTOS 并发/硬件性能证明。

## 未完成与下一步

main 仍含业务实现，完整组合根未迁入。Wi-Fi 接口/backend/message bridge → app_input/provider/sim → 维护独占 → camera backend/PTP 基类/控制器拆分 → UART gateway/兼容清理。完整条目见 [验收清单](../development/module-split-checklist.md)。
