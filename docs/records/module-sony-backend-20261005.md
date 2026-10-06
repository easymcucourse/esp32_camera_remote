# 2026-10-05 Sony backend 组合实例

本批建立camera_backend_sony实例及完整ops，继续完整计划。生产Camera尚未接入，旧fd/PTP/Sony入口和独立sony_camera解析component仍存在；不是完整阶段4或全计划完成。

- 私有factory绑定Camera endpoint与非零domain generation，分配sony_backend_t，内嵌唯一ptpip_client_t和通用camera_backend接口/const ops。没有自建socket、session、transaction、cancel或deadline；所有I/O经PTP基类消息路径。factory目录只登记PRIV_INCLUDE_DIRS，尚无生产调用方。
- 实现command/event打开握手、保存peer GUID在Event OPEN前校验、OpenSession和原ZV-E10八步初始化顺序。GetObjectInfo的0x2009/0x200f完整拒绝仍允许会话继续。使用caller scratch，不额外分配1MiB图像buffer。失败保留通道，须disconnect后destroy/retry；peer只在全部成功时返回。
- 属性从真实描述转换为语义setting/typed value/枚举/只读状态、focus/zoom/recording能力；保留原primary类型、64项上限、Mode完整枚举门禁、快门/光圈特殊值及无枚举相对控制。EV choice按signed INT16升序，bits不变；坏数据/重复已知字段清空缓存且不发布partial callback。设置拒绝类型/位宽/非真实枚举和只读/无能力写入。
- 五类语义动作复用已有唯一Sony encoder/client；标准事务→Sony控制事务连续。GetObject复用Sony JPEG边界解析，frame借用scratch；完整拒绝和无效JPEG分别REFUSED/DROPPED。事件最多32包，0xc203触发通用properties_changed，probe仍由PTP处理。
- PTP新增composite scope/cleanup scope，仍使用基类唯一deadline/depth；OPEN/CLOSE纳入整个操作绝对上界，嵌套wire request保存/恢复父deadline，过期不能新开transaction。cleanup scope允许取消/旧网络代数后释放token。正常健康disconnect发送标准CloseSession；损坏/取消路径直接释放通道；close失败保留token供重试，destroy拒绝仍拥有token的实例。
- backend边界禁止SDK网络、app_wifi/UI/维护/输入/旧PTP transport/session；generic camera_backend不依赖vendor。新Sony backend暂私有依赖sony_camera中的纯解析/编码源，S4.3仍待物理迁入/删除compat。

主机71/71通过，原54保留。新test_sony_backend复用完整test_ptpip_protocol fake Console fixture并先执行原回归，链接同一个真实PTP client/protocol/wire/lease与Sony parser/encoder，无Wi-Fi/lwIP/legacy fd：八步初始化、transaction2..10→控制11..17、属性/有符号EV排序、JPEGborrow、拒绝继续、事件、无效枚举不分配事务、坏属性不发布、保存身份提前拒绝、网络代数、取消、close失败保留/重试、destroy及健康CloseSession。新增PTP scope测试证明子请求恢复父上界、过期与取消cleanup。fixture中domain检查由固定9改为active client generation以覆盖新10，不降低source/target/domain校验。

Default 0x3559b0 / Stable 0x354b90 / Release 0x349050构建通过，日志有Sony backend源实际编译；三镜像<5MiB，Release禁用符号不存在。boundary/doclinks/diff通过。证据build/module-sony-backend-{host,host-build,config-default,config-stable,config-release,default,stable,release,symbols}.log。无烧录/提交/推送；没有实机/稳定性结论。

下一步生产Camera使用通用backend/消息发现与RSSI，建立app_camera endpoint并迁移session/properties/control/liveview pipeline，再移除旧fd/global PTP/Sony兼容component；输入/UI/独占维护/UART/组合根及全清单验证仍缺。见[完整清单](../development/module-split-checklist.md)。
