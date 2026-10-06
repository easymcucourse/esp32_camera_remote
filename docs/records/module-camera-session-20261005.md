# 2026-10-05 Camera 私有组件与会话所有者

本批继续阶段4.6/4.7：已建立app_camera独立构建单元、移入参数内核并实现通用backend会话所有者。尚无完整app_camera facade/Camera endpoint，生产camera_controller仍在main走fd/PTP/Wi-Fi/UI旧路径；新session和generic read尚未接入生产。

- camera_properties/camera_menu/setting_control及头移入app_camera，头只在private include目录；通用choice state留common。main删除这些源的编译，暂通过显式private include路径访问原kernel与legacy dataset适配入口，待producer/调用方迁移后删除该跨模块兼容路径。原主机fixture改源路径/include，逻辑断言保留；README/docs源码引用更新。
- camera_session只包含通用backend契约，实现fresh factory绑定、独立domain lifetime generation、连接、cleanup和destroy phase；不保存PTP transaction/session/deadline，不增加纯转发操作层。只有CONNECTED可借用已校验backend；同owner直接调用其ops。
- generic camera_backend_validate检查factory返回对象/ops的版本与能力一致性和全部必需/广告ops。坏factory对象在I/O前被拒绝但不丢所有权，current-version cleanup callbacks仍可清理；无法安全使用的ops保持对象，不能假装销毁。
- 每次创建分配新非零generation；取消predicate在创建前检查，predicate/context持续有效到close。新peer先写局部值，仅成功连接后发布。factory失败带已分配handle、connect失败、disconnect失败、destroy失败均保留指针并禁止覆盖。断开成功/销毁失败有独立DESTROY phase，重试只销毁，不重复执行disconnect。无对象close幂等；异步stop只经task-safe predicate，不能从其他task与destroy竞态使用backend指针。
- camera_backend_binding是唯一私有Sony build-selection源，用const factory表选择Sonycreate/必需能力，无app_core持有的factory/后端对象。app_camera PRIV_REQUIRES camera_backend_sony，factory头只作为其private include。runtime/session/properties不包含PTP或Sony实现头，CI只给该selection源的factory头例外。

主机73/73通过，原54保留；fake backend session测试无PTP/Sony/Console/Wi-Fi：初始factory错误/未知能力、取消、创建无内存/带对象失败、失败连接不发布partial peer、清理超时、destroy失败仅重试destroy、禁止覆盖/reconnect、fresh generation、接口版本/必需能力在connect前拒绝；generic bind测试新增对象/ops版本能力一致性验证。现有generic properties/menu/setting和Sony协议回归均通过。build/module-camera-session-{host,host-build}.log。

Default 0x355a50 / Stable 0x354c30 / Release 0x3490e0成功，新app_camera session/binding/kernel实际编译，镜像均<5MiB；Release禁用模拟/JPEG编码符号不存在。boundary/doclinks/diff通过；build/module-camera-session-{config-default,config-stable,config-release,default,stable,release,symbols}.log。无烧录/提交/推送；没有硬件/长稳结论。

下一步生产producer接入session/generic properties和Camera endpoint、message发现/RSSI；旧队列camera_action_t与backend动作enum重名须先改为queued type。发现不能复制socket：后端两lane限制需要通用probe/串行探测保留多相机拒绝与已绑定MAC策略，然后销毁probe实例再创建正式会话。继续runtime/control/frame lease拆分、旧fd/PTP/Sony compat清理及完整输入/UI/维护/UART/组合根。见[完整清单](../development/module-split-checklist.md)。
