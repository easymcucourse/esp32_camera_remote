# 2026-10-05 PTP/IP 初始化握手接入实例

本批继续推进阶段4.2。新增实例握手和共享交换逻辑；Camera生产路径仍有旧fd/报文构造/事务计数，完整迁移尚未完成。

- 原main初始化exchange的send/receive/probe循环迁入唯一ptp_wire_initialization_exchange，保留最多16个probe和同一事务绝对deadline。main旧入口暂委托该逻辑，后续生产实例切换时删除。wire不含业务/UI/保存身份政策。
- ptpip_client_initialize_command在既有command token上构造同样的UTF16LE名称/GUID/协议版本请求，使用显式握手timeout（当前配对10秒/首次120秒）。验证InitCommandAck最小长度、偶数名称长度、终止符和版本；InitFail保留reason为实现诊断。成功把connection ID、peer GUID/name和初始化状态保存在同一client，首次标准transaction保持原值2。
- initialize_event通过event token发送该connection ID，要求8字节InitEventAck，拒绝重复初始化及未完成command的event初始化。基类不调用UI、不保存身份、不判断已配对peer是否匹配，这些政策仍应由Camera/backend处理。
- 关闭command成功清除握手/session状态和connection，重置next transaction；关闭失败保留状态与token可重试。握手后的标准command timeout仍须按既有时序恢复5秒。本批没有改变网络连接超时、配对确认时间、任务属性或重试参数。

主机69/69通过，原54保留；原协议/Sony断言未改。真实client/wire+fake Console回归扩展：68字节名称/GUID/version请求、probe回复、120秒整个握手deadline、peer字段、command/event token隔离、标准OpenSession transaction2/下一次3、InitFail、错版本/终止符/奇数字节、16 probe上限、event顺序/长度、重复初始化；close失败保留和成功清状态。fixture GUID为合成值，不是设备身份。fake调度器不证明硬件配对或RTOS时序。

Default 0x355670 / Stable 0x354850 / Release 0x348c90编译通过、均<5MiB；Release禁止模拟/JPEG编码符号不存在。日志build/module-ptpip-init-{host,host-build,default,stable,release,symbols}.log。边界、文档链接、diff通过；未烧录、提交或推送。

下一步实现message事件poll、Sony实例调用和Camera生产网络/发现消息，删除旧fd/transport/lwIP依赖并完成backend组合；继续输入/UI菜单/维护独占/Camera职责拆分/UART/完整组合根及兼容清理。见[完整清单](../development/module-split-checklist.md)。
