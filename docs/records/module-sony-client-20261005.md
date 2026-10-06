# 2026-10-05 Sony 控制编码接入 PTP 实例

本批继续阶段4，完整目标保持进行。新Sony client控制接口已建，旧Camera生产路径仍fd；完整camera_backend_sony factory/ops与组合实例尚未实现，不能把控制接口当成完整backend。

- 将sony_ext的七类值编码原样迁入私有sony_control_encoder：scalar/曝光/对焦/快门/录像/变焦/相对设置。只有一套opcode/property/width/有符号位/合法方向校验逻辑，既有按键映射与取值不变。
- sony_client_controls只接受ptpip_client_t，调用基类send_data分配transaction并执行I/O，没有自己的session/transaction/deadline、socket或网络driver。只读writer绑定是编码器的私有短生命期调用上下文，不注册业务endpoint或拥有后端。
- 旧sony_ext暂绑定fd+外部transaction到同一编码器，供尚未迁移的生产Camera使用；将在生产切换后删除旧入口和适配，不作为最终结构。没有复制PTP data-out/response/probe基础能力。
- 新client/encoder边界扫描拒绝Wi-Fi/lwIP/UI/维护头和旧PTP transport/session入口。旧生产main、legacy Sony入口及ptpip parent的lwIP仍待删除；没有新增task或改变重试参数。

主机69/69通过，原54保留。原test_sony_write源逻辑/断言未改，仅增唯一编码器源。新Sony client控制与真实PTP client/protocol/wire共用同一fake Console fixture，不链接app_wifi/lwIP/legacy fd：验证曝光、正负对焦、三向变焦、半/全快门与释放、录像start/stop、快门/光圈步进、拒绝/probe、无效参数不分配事务；标准PTP与Sony连续使用同一个client计数器，消息权限/deadline校验沿原fixture。尚未证明完整Sony backend的能力/属性/取景生命周期，fake调度器不是实机证据。

Default 0x3559d0 / Stable 0x354bb0 / Release 0x349040编译通过、均<5MiB；Release禁止模拟/JPEG编码符号不存在。日志build/module-sony-client-{host,host-build,default,stable,release,symbols}.log；boundary/doclinks/diff通过，无烧录/提交/推送。

下一步创建/接入完整backend组合和Camera生产实例网络/发现/RSSI/协议/事件，移除旧fd/外部事务计数/transport；继续输入/UI菜单/独占维护/Camera职责拆分/UART/完整组合根及兼容清理。见[完整清单](../development/module-split-checklist.md)。
