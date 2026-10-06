# 2026-10-05 Sony 数据发送事务收口到 PTP 基类

本批是完整拆分计划的继续实施，尚未完成阶段4或完整目标。

- 从sony_ext移除通用PTP data-out报文和response/probe处理，迁入唯一ptp_wire engine。Sony保留scalar宽度/符号位、按钮状态、变焦、对焦及相对步进的命令和值编码。原22字节operation、20字节START、DATA和12字节END以及日志字段保持。
- 新ptpip_client_send_data使用实例的command通道、自动transaction、最早绝对deadline和response诊断；拒绝值返回accepted=false但事务同步成功。错误transaction/报文/I/O返回失败，旧值不被当作接受。
- 原生产Sony暂通过ptp_send_data旧fd入口进入同一个wire。此适配用于逐职责迁移，尚须在Camera/Sony实例调用切换后删除；不作为最终架构。现有生产事务计数仍在main，PTP parent仍依赖lwIP。
- 控制payload保留原128字节包、最大116字节约束，实际scalar仍1/2/4字节。发送buffer沿既有报文组合，不改变协议，也不修改操作/输入映射、重试参数或任务属性。

主机69/69通过，原54保留。原test_sony_write逻辑和断言未改，仅增加真实ptp_send_data/ptp_wire源；仍验证全部Sonyscalar/对焦/按钮/录像/变焦/相对设置、拒绝/probe/错transaction等报文。真实消息client协议测试增加1/2/4字节DATA组合、START/END长度和transaction、拒绝/probe、错transaction、无效data/长度不分配新transaction；fake Console继续逐段验证同deadline和lease权限。

Default 0x355610 / Stable 0x3547f0 / Release 0x348c20编译通过、均<5MiB；Release禁止模拟/JPEG编码符号不存在。证据build/module-ptpip-send-{host,host-build,default,stable,release,symbols}.log。边界/文档链接/diff通过；未烧录、提交、推送。主机fake调度器不证明实机功能或稳定性。

下一步握手/event poll、Sony实例编码与生产Camera消息切换，删除旧fd接口/transport并实现完整backend组合；再继续输入/UI菜单/独占维护/UART/完整组合根及兼容清理。见[完整清单](../development/module-split-checklist.md)。
