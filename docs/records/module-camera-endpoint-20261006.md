# 2026-10-06 Camera 独立消息接收

本批把生产Camera inbox从预览循环移至独立camera_endpoint任务。endpoint暂在main，与旧owner一起待迁app_camera；旧fd/PTP/Wi-Fi/UI路径仍在，camera_stream/backend生产替换尚未完成，不把此步当完整阶段4完成。

- startup注册Camera {control16,bulk2}，创建4096字节内部栈/prio4任务；allocation/register/task失败释放队列并停止endpoint。每25ms接收，保持网络IO等待期间可接收STOP/STATUS/caps/action/setting请求；producer不再竞争同一个Console inbox。
- UI_FRAME_RESULT仅接收UI来源、当前非零frame generation、非零token、无lease/REQUEST/REPLY/BULK的metadata，放入4项metadata队列；owner轮询队列并应用frames_result，metadata不是JPEG buffer所有权。两个真实frame槽最多两个未归还completion，generation在开始发布前建立、drain后清除。JPEG buffer仍等待metadata+last-ref，不撤销租借。人为伪造内部UI消息导致队列满返回NO_MEM；外部网关不得伪装UI来源。
- STOP立即触发旧owner的取消predicate，最多16个等待请求保留correlation/epoch/deadline并在原owner任务完成清理后reply；过期不回OK；new owner lifetime变化证明前一owner已完成，可确认原STOP而不等待新会话。saved request无lease，不持跨任务buffer。START在有待确认STOP时拒绝；STATUS/caps不等待网络完成。此机制不是实机停止时限证明。
- 旧owner新增worker_active/lifetime区分“真实运行任务”和维护busy reservation；start/finish在生命周期锁内更新，避免老任务清busy后覆盖新任务active。重复stop/maintenance stop保留首次stop时间，不延长原900ms释放边界。SETTING_ADJUST使用私有UI语义映射，不引入backend/PTP头；旧配对FORGET真实改为camera_identity_run内部RAM worker。
- 当前typed START command.flag=true表示配对验证，false表示预览；typed setting使用UI semantic property ID，direction±1与可选safety generation token。ACTION回复表示被原安全队列接收，非物理动作完成。STATUS保持原debug phase/last_io观察。新producer移入后这些私有调用会合并为本component生命周期/队列逻辑，不保留跨component转发层。

host83/83通过，原54保留。新增测试直接编译真实main/camera_endpoint.c，用fake RTOS/owner演示IO阻塞期间STOP与STATUS、owner完成后确认、过期不确认、新lifetime不阻塞旧确认、16等待上限、lease与伪来源/stale frame拒绝、metadata传递、setting安全代数、task create失败回滚。未模拟旧controller网络IO，不是RTOS调度/实机证明。Default/Stable/Release全部构建，镜像{"default": "0x3569a0", "stable": "0x355b80", "release": "0x34a070"}，均<5MiB；endpoint entry/frame bridge真实链接，旧worker/Release禁用符号无。日志build/module-camera-endpoint-{host-build,host,default,stable,release,symbols}.log。

boundary/doclinks/diff通过，所有build handles终态，未烧录/提交/推送。完整Camera facade/producer/backend切换、Input/provider/sim、独占维护、UART与组合根/compat清理仍待完成，见[完整清单](../development/module-split-checklist.md)。下一步将endpoint与owner一起迁入app_camera，endpoint只提交线程安全控制/owner command队列与取消信号，参数kernel/frame state仍由producer唯一持有；用新session/discovery/identity_work/stream实际替换旧协议主循环。
