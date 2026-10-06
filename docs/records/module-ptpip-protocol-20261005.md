# 2026-10-05 标准 PTP 数据事务接入消息实例

本批推进阶段4.2，完整目标仍进行。生产相机仍经旧fd入口；标准报文校验已合并，不能据此声称完整网络/会话迁移完成。

- 原ptp_session的receive_packet、operation和request_data_result原样迁入私有ptp_wire，通过短生命期I/O上下文收发。wire不持有网络、session或transaction状态，不包含Wi-Fi/lwIP/Sony。旧生产入口和新client调用同一个wire engine，没有复制数据阶段解析器；旧event select仍保留在待删除的ptp_session兼容入口。
- 新ptpip_protocol只操作实例：command通道、事务号分配、OpenSession成功设置session1/CloseSession成功清零、PTP response诊断、读取数据结果和任一通道的小包接收。拒绝CloseSession不清会话；事务号到UINT32_MAX拒绝继续发送，避免回绕复用。实例初始化的next_transaction为1，标准协议与后续厂商扩展共享它。
- 多次header/body/data/probe消息收发使用同一个最早绝对deadline，嵌套receive_packet不刷新预算。输出buffer直接用lease写入；SDK/fd不进入新接口。原拒绝可重用与IO/PROTOCOL须关闭的分类保持。
- ptp_result是纯值类型；新增边界扫描禁止新client/protocol/wire包含app_wifi、esp_wifi/esp_netif/lwIP。ptpip父component仍因旧生产transport/event要求lwIP；这条依赖尚未删除。握手、事件poll和Sony/Camera生产切换仍缺，旧main仍有事务计数器，单一生产会话所有者尚未达成。

主机69/69通过，原54保留，原test_ptp_session断言和测试逻辑未改，只增加同一wire源编译。新test_ptpip_protocol链接真实client+protocol+wire+lease和fake Console，不链接app_wifi/lwIP；覆盖拒绝后继续同实例、分段对象拼接、probe响应、错transaction、未完成data phase、短END/超capacity/64位长度/错误response尺寸、EOF、Open/CloseSession状态、拒绝关闭不清session、事务号不回绕。fake Console检查每个分段request的channel/domain/network generation、只读/可写lease及相同deadline。

Default 0x355560 / Stable 0x354740 / Release 0x348b80编译通过、均<5MiB；Release禁止模拟/JPEG编码符号不存在。证据build/module-ptpip-protocol-{host,host-build,default,stable,release,symbols}.log。边界、文档链接和diff检查通过；没有烧录、硬件验证、提交或推送。fake调度器不证明RTOS并发或硬件稳定性。

下一步迁移握手、event poll与Sony写事务，生产Camera切换到同一client后删除旧fd入口/transport/lwIP依赖；随后继续输入、UI菜单、维护独占、Camera backend/职责拆分、UART、完整组合根和兼容清理。见[完整清单](../development/module-split-checklist.md)。
