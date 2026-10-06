# 2026-10-05 Camera 身份、动作队列与通用控制

继续阶段4。生产正在使用的身份存储和安全动作队列已迁入 app_camera；新通用控制执行器已建并独立测试，尚未接入生产主循环。完整计划仍未完成。

- camera_identity.c/.h 从 main 迁入 app_camera/source/private，生产与原 host 测试使用同一份实现；sony_remote namespace、GUID/peer 迁移、孤立记录拒绝、初始化完成后确认/commit，以及错误保留行为未改。NVS 身份保存仍需运行在原内部 RAM worker，不能在 PSRAM owner 栈执行 cache-off 写入。
- camera_actions.c/.h 移入 app_camera/source/private；header 只依赖 common pad_types，不再包含输入实现。32项队列、1000ms旧press取消、session/safety generation、满队列释放、S2→S1→zoom释放顺序、只在成功释放后清latch保持。main删除原源注册，原controller/app_main仍通过临时private include使用；不是公共转发头。
- camera_controls 是 Camera owner 的状态/执行逻辑，持有安全队列与caps/录像确认。每tick最多一个动作，record/zoom press写前重新读取通用properties并apply参数kernel，再校验safety generation及能力；未知/同目标录像或MF非电动镜头zoom拒绝并调度安全释放。已确认电动镜头policy由session显式传入，没有自动推断。
- 通用ops只在owner调用。refresh与action共用tick正timeout预算，不重复获得完整timeout；时钟wrap按uint32差值。press拒绝继续会话并调度release；release拒绝/IO失败返回错误、保留latch，由owner关闭会话。accepted录像变PENDING，10s内只有真实properties recording回读可变APPLIED，否则TIMEOUT；等待/已排队时不再接收录像命令。caps保留声明lens并同步safety generation；changed bitmask供后续producer发布语义command status。
- boundary scanner阻止app_camera依赖旧gamepad_input/atom_link/controller实现头。新控制器没有PTP/Sony/Wi-Fi/UI实现依赖，也没有协议属性码、fd或外部transaction。

主机77/77通过，原54保留。原identity/actions测试逻辑/断言保留，仅编译源/头路径调整。新增fake generic backend test无需PTP/Sony/Wi-Fi/UI/router实现：press/release顺序、zoom预读、能力拒绝、安全代数在read期间变更、录像重复合并/readback/10s超时、clock wrap、剩余timeout、property IO错误、press过期、release拒绝保留和重试、session关闭。

Default 0x355e60 / Stable 0x355040 / Release 0x349520构建成功，新identity/actions/controls编译真实参与三种component archive。三镜像<5MiB，Release模拟/JPEG encoder禁止符号不存在。新controls尚无生产调用，链接器可能移除它，不能把archive编译当作production接入。日志build/module-camera-controls-{default,stable,release,host-config,host-build,host,symbols}.log。boundary/doclinks/diff通过，所有build handles成功终态；未烧录/提交/推送，无实机/长稳结论。

下一步实现生产Camera endpoint/facade/主循环：session/discovery/property→controls→semantic UI消息；内部RAM identity worker、旧stop释放窗口、frame slots/lease归还、网络代数取消必须纳入；移除旧Sony/fd/PTP transaction和直接Wi-Fi/UI；再继续输入、独占维护、UART、组合根及compat清理，见[完整清单](../development/module-split-checklist.md)。
