# 2026-10-05 Camera 消息发现与通用 Probe

本批继续阶段4：消息发现与串行backend探测已建，纯候选/重试策略移入app_camera；生产控制器仍使用原fd/Wi-Fi直接路径，新scan/session/read尚未接入，Camera endpoint/facade仍缺。

- camera_backend追加可选PROBE能力及ops校验；Sony probe复用内嵌PTP client，仅命令lane TCP OPEN，不握手、不OpenSession、不分配PTP事务。保留token直到disconnect，拒绝仍拥有通道时再次probe/connect或destroy。probe本身不释放对象，清理失败可重试。
- camera_session统一create/版本能力校验与generation分配用于open/probe；probe要求PROBE能力，结束进入CLEANUP phase，不允许它作为已连接session借用backend。重复probe拒绝不改变原CONNECTED状态；原factory/cleanup所有权规则保持。
- camera_discovery通过CAMERA_DISCOVER request取得最多4个DHCP clients，只筛选已绑定MAC/关联租约、不扫描网段；通过通用backend probe顺序探测15740/800ms，每个对象明确disconnect1000ms并destroy后才下一个候选，避免两lane资源耗尽。候选mask复用原camera_select_candidate，多相机仍-2/不自动选一个，无相机仍-1；connect_failed保留原重试语义。清理失败中止并保留owner，不能覆盖指针继续扫。
- 唯一可达候选通过WIFI_SELECT_CAMERA request确认后才返回peer；无partial目标。新增camera_network_select(NULL)是忘记绑定后的消息清除入口，尚未接入旧forget。消息source固定Camera、target Wi-Fi，非零domain与绝对1s RPC deadline，取消通过owner predicate。Wi-Fi/driver/socket/UI实现头不出现。
- camera_link纯候选/重试源码与头迁入app_camera/private，main删除重复源编译，原controller暂从private compat路径使用。queued action type改为camera_queued_action_t，解决通用backend语义camera_action_t冲突；队列布局、协议与释放行为不变，原测试仅类型名字/include修改，逻辑断言保留。

主机74/74通过，原54保留。新fake Console+fake backend发现测试无Wi-Fi/PTP/Sony/lwIP：4客户端顺序唯一选择、多相机拒绝、绑定MAC过滤、无可达/空DHCP、count越界、RPC超时、取消、坏IP、probe清理失败保留/重试、不重叠backend对象、fresh lifetime generation与显式清选择。Sony backend复用原PTP fake Console测试证明probe只有OPEN/CLOSE无任何协议packet；重入/取消cleanup；generic缺probe ops拒绝；session拒绝active实例probe不损坏状态。原camera_actions和通用backend头同一TU编译验证无名称冲突。

Default 0x355a50 / Stable 0x354c30 / Release 0x3490e0编译通过，新camera_discovery与policy/probe真实编译，三镜像<5MiB；Release禁止模拟/JPEG编码符号不存在，boundary/doclinks/diff通过。build/module-camera-discovery-{host,host-build,config-default,config-stable,config-release,default,stable,release,symbols}.log；无烧录/提交/推送，无硬件/稳定性结论。

下一步建Camera endpoint/facade、producer接入session/scan/generic read及网络代数/RSSI事件；移除旧fd/外部transaction/直接Wi-Fi与UI路径；继续控制/identity/frame lease和Sony parser物理迁入。完整输入/UI/独占维护/UART/组合根/compat清理仍未完成，见[完整清单](../development/module-split-checklist.md)。
