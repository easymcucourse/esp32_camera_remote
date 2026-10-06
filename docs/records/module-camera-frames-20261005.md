# 2026-10-05 生产 JPEG lease 消息路径

本批已把原生产 controller 的取景 JPEG 交接改为 Camera → Console → UI frame lease event。旧 Camera 的 PTP/fd/session/参数与生命周期编排仍在 main；完整 Camera facade/runtime 和其他计划余项未完成。

- app_camera 的 camera_frames 管理两个现有 1MiB 对象槽，不复制 JPEG、不新增全屏画布/worker；readonly lease 只覆盖 Sony parser 确认后的 JPEG 子区间。callback 只在最后引用归还时 release-store returned，之后不再访问 slot/context。reuse 同时要求引用归还和匹配 UI_RESULT metadata；结果可以乱序，但质量统计按 token 顺序应用。
- CAMERA_FRAME 使用非零 domain generation、单调 token、read latency 以及 readonly JPEG lease。UI_FRAME_RESULT 是 UI→Camera 控制 metadata，只有 generation/token/result，无 JPEG 指针。槽号、vendor property、fd、implementation pointer 不进消息。frame token 不允许在同一生命周期 wrap后复用；domain generation 与 session 共用唯一序列，不是PTP事务或input safety generation。
- app_ui 的 ui_frames 执行 generic JPEG/render/recover，检查只读lease/来源/代数/token。成功 recovery 后结果 NOT_FINISHED 表示当前帧丢弃，未误计为已显示；invalid JPEG为INVALID_RESPONSE；其他fatal错误传回owner。Camera保留十个连续damage才重连、成功帧清streak、恢复/队列丢帧不冒充成功帧。backend/parser确认坏envelope时在Camera本地按同一token顺序计damage，不把协议bytes发给UI。
- UI最多保存两个待发送结果。Camera控制inbox满时用metadata重试，不持有JPEG lease，生产槽在结果消费前不再提交下一帧；停止router/endpoint后丢弃失效结果。控制和bulk仍为独立inbox，没有新的跨component回调或大buffer复制。
- 原 read_liveview 真正调用camera_frames_publish，注册Camera inbox并由原owner轮询UI结果；当前仅frame results和拒绝未迁移request，完整Camera START/STOP/ACTION/SETTING/status endpoint仍待接入。bulk满则丢帧，部分fanout失败也等待剩余refs，晚result不能匹配新token。停止/错误cleanup保持stack owner和buffers直到所有refs归还，然后才用data[0]发送控制release或free；maintenance超时可返回失败但不撤销仍在UI使用的buffer。
- 生产独立 jpeg_decode worker/free_slots/ready/done删除，原源码/头移动tests/support/legacy供原回归，逻辑/断言不变。解码执行在现有UI endpoint：CPU1/prio4/32768 PSRAM栈，Camera RX原CPU0/prio4/32768 PSRAM与双1MiB保持。UI侧保留原LIVEVIEW frames/fps/read/display/stack日志前缀，工具test_camera_connection.py无需弱化或改匹配规则。架构任务/槽表与serial-log更新，scanner拒绝生产legacy pipeline header。

host79/79通过，原54保留。新Camera fake Console与真实lease测试不链接UI/PTP/Sony：slot/exact range/readonly、两槽backpressure、乱序result、last fanout reference、late/duplicate/wrong generation、partial send失败token隔离、lease池耗尽恢复、stopped/fatal drain、envelope和JPEG十damage顺序、display recovery neutral、token wrap/overlap拒绝。UI fake Camera/Console测试不链接app_camera/backend/PTP/Sony：只读/旧代数/显示错误/recovery、两结果队列满重试、不持有lease、endpoint停止。原legacy damaged-frame/recovery/fatal-drain断言仍通过。

Default 0x356230 / Stable 0x355410 / Release 0x3498f0构建通过，三镜像<5MiB。三ELF均链接camera_frames_publish/result、ui_frames_handle、app_ui_show_jpeg，不含jpeg_decode_task；Release模拟/JPEG encoder禁用符号不存在。日志build/module-camera-frames-{host-build,host,default,stable,release,symbols}.log。边界扫描、文档链接与diff检查通过；所有build handles终态。无烧录/提交/推送，实机FPS/画面/停止耗时/长稳待验证。

下一步完整Camera endpoint/facade/producer迁入app_camera，session/discovery/properties/controls接入，删除旧fd/transaction/直接Wi-Fi与UI状态/参数调用；内部RAM identity worker、stop释放窗口和新frame leases保留。继续完整输入/providers、独占维护、UART、组合根与兼容清理，见[完整清单](../development/module-split-checklist.md)。
