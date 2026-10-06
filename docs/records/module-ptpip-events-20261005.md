# 2026-10-05 消息事件通道轮询

本批继续阶段4.2，未完成完整计划。新PTP消息实例已能轮询/接收标准事件；旧Camera生产路径仍用fd和旧event select，后续必须替换并删除。

- app_wifi追加可选channel_poll驱动操作，沿同一single-owner busy/cancel/network generation/admission门禁执行，不消耗字节。旧fake driver可返回UNSUPPORTED，不扩大必选能力。wifi_esp32私有socket用零timeout select，前后检查deadline/cancel/generation；EINTR最多重试32次，耗尽返回无就绪供下次轮询，避免阻塞或无界循环。
- WIFI_CHANNEL_RECEIVE的poll语义为length0、无lease、非BULK；bridge拒绝带buffer/长度/bulk的poll，执行结果带readable。该操作按通道队列顺序运行，socket检查本身不等待数据；已有close独立控制队列/取消/排空规则不变。错误结果清readable。普通收发仍要求lease和正确读写权限。
- ptpip_client_poll只通过Console message取得就绪；client_next_event每次最多接收一个标准包，输出available、probe/code/transaction/最多3个params，probe由基类回复。无事件不读取buffer；header/body/probe回复共用同一packet transaction deadline；厂商自行按既有32包/帧上限排空并解释事件code。
- 标准EVENT decoder合并到一份ptp_wire，旧生产ptp_session与新client共用，长度14..26/4字节参数/类型校验保持。旧ptp_session仍含0xc203刷新政策及select，待生产迁移后移出/删除；新基类不解释Sony事件语义。没有新增task或改变原输入/相机重试参数。

主机69/69通过，原54保留。原事件/协议/Sony回归断言保持；新覆盖backend零等待/不读取、就绪/无数据、cancel/gen/过期deadline、EINTR有界/错误诊断，generic无poll op，真实bridge无lease poll/非法长度，真实client/protocol+fake Console无事件、三个参数、probe回复、错EVENT长度与事务depth释放。fake调度器不是RTOS或实机稳定性证明。

Default 0x355910 / Stable 0x354af0 / Release 0x348f60编译通过、均<5MiB；Release禁止模拟/JPEG编码符号不存在。日志build/module-ptpip-events-{host,host-build,default,stable,release,symbols}.log；边界/doclinks/diff通过，无烧录/提交/推送。

下一步Sony实例控制API与Camera生产message网络/发现/RSSI/事件接入，删除旧fd/transport/lwIP旁路并落实backend组合；再继续输入/UI菜单/独占维护/Camera职责拆分/UART/完整组合根及兼容清理。见[完整清单](../development/module-split-checklist.md)。
