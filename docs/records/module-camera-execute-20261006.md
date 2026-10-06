# 2026-10-06 Camera 参数执行、控制同步与身份 Worker

完整计划继续推进，Camera整体生产主循环/facade尚未切换。本批有实际生产identity worker迁移，以及供通用主循环使用的参数/控制执行约束与backend结果分类；不把新执行器archive编译当作生产接入。

- camera_settings_execute使用generic backend semantic set/step，复用原setting_control/camera_menu。每个新read/apply的fresh permit只消费一次、最多一项写入；Mode优先，Mode仍待实际回读时整个menu保持pending。写入仅表示接受，不修改actual；完整拒绝更新REJECTED并保留健康会话，IO错误返回给cleanup owner。值类型来自复制的generic snapshot，EV保留signed16 bits；relative只发±1，拒绝缺descriptor/type-tag不一致/缺ops；invalid snapshot清可写和target。私有menu→backend ID映射由collection/execution共用，不带Sony/PTP码。
- camera_controls增加可选成对guard供Camera消息任务与backend owner共用安全队列/caps。所有网络/property I/O在guard外；read后及write前重校验safety generation。执行中的录像在出队后仍保持record_executing/record_pending，read和write期间都拒绝重复录像；guard异常拒绝开始IO；错误/过期/cancel路径清executing，仍保留必须释放的latch。只有backend owner读写参数kernel与调用ops；与纯component内部同步相关，不开放跨component数据指针。
- 内部RAM camera_nvs worker迁入app_camera/camera_identity_work.c，原main控制器真实调用camera_identity_run LOAD/CONFIRM，删除自己的worker/create/semaphore段。保持4096字节内部栈/prio4、同NVS namespace与完整Sony初始化成功后才confirm；参数/身份在worker内部栈复制，NVS返回后才复制结果到caller，completion signal后不再访问context。create/semaphore/NVS失败保持所有权/回收信号资源，无超时撤销stack。FORGET支持供完整Camera endpoint后续使用，原factory reset当前仍直接低层forget。
- PTP client将完整InitFail分类为INIT_REJECTED，保留32-bit reason诊断；Sony backend映射IDENTITY，包括reason0，供owner明确停止自动重试/要求用户动作。不另存第二套持久握手状态。Sony GetObject只有0x200F映射generic NOT_READY（完整响应可短暂重试）；其他完整拒绝仍REFUSED，避免迁移后把所有错误都按50次/100ms重试。原生产fd路径行为未改。

host81/81通过，原54保留。新generic settings fake backend独立测试不链接PTP/Sony/Wi-Fi/UI/Console：Mode屏障/回读、一次read只发一次、signed EV/relative、拒绝/网络错误/缺descriptor/type/ops、invalid snapshot清目标、10s超时。control测试加入guard成对/所有IO不持锁、read期间代数变更、录像read/write执行中重复拒绝，原断言保持。identity worker fake scheduler验证内部worker调用参数/本地identity副本/成功publish/失败不覆盖/create失败/semaphore失败/销毁/forgets；不是硬件cache-off证明。Sony/PTP真实wire+fake Console新增InitFail reason0和0x2009 vs0x200F分类测试；已有新backend预览拒绝断言改为更精确NOT_READY，原54原始协议断言未改。

Default 0x356310 / Stable 0x3554f0 / Release 0x3499e0构建通过，三镜像<5MiB。三ELF真正链接camera_identity_run和前批frame/UI消费路径；旧identity_worker/jpeg_decode_task无符号。Release禁用模拟/JPEG encoder符号不存在。settings/controls新路径尚无生产调用，链接器可能移除，完整producer仍旧fd/PTP/直接Wi-Fi和UI状态调用。日志build/module-camera-execute-{host-build,host,default,stable,release,symbols}.log；boundary/doclinks/diff通过，所有handles终态，无烧录/提交/推送/新实机效果或长稳结论。

下一步完整Camera endpoint/facade/producer替换，直接复用session/discovery/read/settings_execute/controls/identity_work/frame leases及semantic outputs。STOP必须由独立消息接收与取消predicate保持IO等待期间可处理，完成排空才确认；NVS保持内部worker；健康协议边界保留原900ms控制释放窗口。旧维护reservation/Resume仅属现行实现兼容，最终独占维护必须另按计划替换，不能将旧授权扩大为新维护产品策略。继续输入/provider/sim、独占维护、UART、组合根与compat清理，见[完整清单](../development/module-split-checklist.md)。
