# 2026-10-05 Camera 属性快照与参数状态机边界

本批继续生产Camera后端迁移的前置工作：参数状态机与Sony数据集解析分离，建立通用后端快照读取/应用。新read路径尚未被生产控制器调用；不能声称生产Camera已切换或完整app_camera组件已建立。

- common/camera_choice_state.h定义复制的scalar choices/current/count/writable；Sony解析类型暂alias到同一布局。setting_control不再包含Sony头，原目标合并、枚举、等待回读、超时、确认逻辑保持。
- camera_menu.c只消费camera_menu_snapshot_t；原Sony collector/类型/EV排序/特殊值校验和raw数据集入口移至独立camera_menu_sony_legacy.c/.h。旧生产Camera明确包含legacy头；kernel头不暴露raw parser。legacy适配把原状态复制给同一kernel，fixture断言/行为不改。
- 新camera_properties仅依赖通用camera_backend/parameter kernel，通过properties visitor复制语义setting/typed value/choice/relative/capabilities。校验重复、未知type、位宽、choice上限/NULL/异型、非法relative；完整错误不应用partial snapshot。只保留值，不保留backend借用descriptor或buffer。menu type是本地opaque身份tag，不向网络输出protocol code；新路径write.code为0，legacy路径仍原code，后续controller用semantic setting调用backend。
- read失败清空快照；apply缺失/坏快照清空writable能力、撤销pending，协议接受后的旧回读仍PENDING，真实目标回读才APPLIED；相对快门/光圈等待真实变化，missing descriptor不保留旧目标。scope/interface/private factory和网络I/O不在该层重复实现。
- established边界禁止这些kernel文件包含Sony/PTP/Wi-Fi/UI/维护；新generic fake backend测试不链接任何Sony/PTP/parser/network代码。main中临时源/依赖将随完整app_camera迁移，旧代码仍明确待删除。

主机72/72通过，原54保留；原test_setting_control/test_camera_menu只新增fixture类型/legacy头include，断言和测试逻辑不变。新fake backend覆盖选择复制不借用、Mode/EV接受后旧回读与确认、相对光圈方向/确认、缺失字段、duplicate/type/width/choices/relative/timeout/version等无效快照，pending取消和协议隔离。证据build/module-camera-properties-{host,host-build}.log。

Default 0x355a30 / Stable 0x354c10 / Release 0x3490e0编译通过，日志确认新properties/kernel/legacy适配源真实编译，均<5MiB；Release禁止模拟/JPEG编码符号不存在。build/module-camera-properties-{config-default,config-stable,config-release,default,stable,release,symbols}.log；boundary/doclinks/diff通过。未烧录/提交/推送，主机测试不证明实机时序或稳定性。

下一步生产控制器使用backend实例和generic properties、建Camera endpoint/消息发现/RSSI；拆分runtime/session/properties/control/JPEG lease，删除Sony legacy adapter/旧fd/global PTP。完整输入/UI/维护/UART/组合根及清理仍缺，见[逐项清单](../development/module-split-checklist.md)。
