# 2026-10-05 Wi-Fi TCP 通道与消息桥

完整目标保持进行。本批建立通道接口、ESP32实现和消息桥；PTP生产路径仍使用旧socket，不能据此宣称完整计划完成。

- app_wifi追加TCP能力和不透明channel。连接捕获network generation，所有收发沿用绝对deadline，不因分段重置；部分进度由bytes返回。缓冲区不复制，send只读，receive写入调用方空间。单channel只允许一个在途I/O；cancel任务安全且永久有效；close先取消，忙时保留句柄供重试。未归还channel阻止Wi-Fi destroy；stop关闭新通道入口，失败时仍保留对象。
- wifi_esp32/private/wifi_tcp独占fd/nonblocking socket/select。连接与每个分段检查network generation、online、取消和deadline，等待切片最多100ms；EINTR/EAGAIN不会延长deadline。失败connect关闭私有fd，EOF与timeout/stale/cancel分类，SDK/errno留在backend diagnostic。业务接口不暴露fd/lwIP类型。
- app_wifi_messages的TCP OPEN/SEND/RECEIVE/CLOSE已转到独立worker服务。固定两个单所有者通道供相机命令/事件流，token属于source+业务generation，网络generation单独验证。无driver指针进入message。每个通道有bulk工作队列4和独立close队列1；dispatcher仅做有界入队，不等待socket。close即时取消，worker先归还该通道排队租约再确认关闭；旧token拒绝，失败open释放slot。
- 收发要求BULK+lease，receive必须有写权限；完成/错误/过期/迟到reply均交回或释放同一引用。请求超时不等于buffer可复用，生产方必须等待lease callback。停止关闭入口、取消全部I/O并排空队列/租约/通道，join全部worker后才清除Wi-Fi对象引用；超时可重试。
- 新增wifi_tcp_command和wifi_tcp_event各4096内部RAM栈、priority2、未绑核；bulk队列各4、close各1、registry固定2。既有wifi_endpoint4096/priority2继续处理配置/状态/RSSI，原wifi_config等任务属性未改。任务/队列创建失败清理已创建资源，不留下后台worker。

主机67/67通过，原54保留。新增generic channel+fake driver、真实私有TCP+fake socket、真实channel worker/lease+fake RTOS测试：能力缺ops拒绝、开始前拒绝/stop超时关闭入口、owner存活保护、关闭与在途I/O、readonly send零拷贝、receive权限、部分传输/EAGAIN、绝对超时/取消/网络变更/EOF、connect故障清理、队列满租约保留、close优先排空、source/业务generation/network generation隔离、迟到reply归还、停止排空、queue/task创建失败。fake调度器不证明真实RTOS并发、LCD时序或硬件稳定性。

Default 0x3553c0 / Stable 0x3545a0 / Release 0x3489c0编译通过、均<5MiB；Release禁止模拟/JPEG编码符号不存在。证据build/module-wifi-tcp-{host,host-build,default,stable,release,symbols}.log。边界/文档链接/diff检查通过。没有烧录、硬件验证、提交或推送。

下一步S4.2 PTP/IP基类与Camera生产网络message路径，仍需完成输入/provider/sim、UI菜单、独占维护、Camera backend/控制器职责拆分、UART、完整组合根及兼容API清理。见 [完整清单](../development/module-split-checklist.md)。
