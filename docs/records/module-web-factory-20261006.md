# 2026-10-06 Web 恢复出厂与正常入口删除

独立维护 Web 新增 POST /api/factory。严格接受 scope=wifi/all 与 confirm=true 两个字段，拒绝缺失、重复、未知范围和额外字段；仍经过统一 first-request gate，首次 POST 只切换维护并重定向，不能执行重置。网页提供两个确认按钮及配对/热点重连说明，HTTP route 数量由14增至15，原6144内部RAM栈/pr3/3 sockets保留，无新增任务或队列。页面 OTA 上限同步为固件实际5MiB。

Core 注入持久化 reset callback，只允许 MAINTENANCE 且无上传；新增 app_core_factory_reset.c 在 HTTP 内部栈执行事务。先冻结唯一配置 worker（5000ms），再读取最新 saved Wi-Fi record，而非运行时 active snapshot；写默认热点，all 额外清除共用 sony_remote identity store 并将 ui_prefs schema1写为DS/完整。没有正常 Camera/UI message/runtime调用。成功保留冻结并响应后1500ms重启，即使最终ACK丢失也提交重启；调度commit失败将Core转RESTART，由既有health重启。

失败取消重启预约、恢复维护配置 admission，正常 owners 永不重开。复用原 factory_reset_all 错误/顺序及 Wi-Fi rollback 语义；不声称跨 namespace 原子性，身份或偏好可能部分变化。freeze超时不写任何存储，saved read失败也不执行重置。事务日志不输出密码、GUID或设备身份。

正常生产删除 app_factory_service worker、启动注入、Core停机等待及 System factory执行分支；原service/stop回归移至 tests/support/legacy，原断言保留。UART factory命令、LCD RESET WI-FI/RESET ALL行/确认状态、Core兼容reset接口删除。另删除UART u与Camera FORGET执行分支，避免绕过Web重置配对。退休message数值保留ABI但拒绝执行；私有Camera忘记函数尚有历史测试，当前ELF未引用，最终dead-source清理仍待审计。

原54基线保留；其中原wifi_menu纯核移至tests/support/legacy保留原reset回归，生产纯核新增独立test_wifi_menu_current。正常System/UART/Camera/UI fixture改验已移除入口、不发送factory及导航到BACK；Core启动/停机fixture移除已不存在的factoryowner阶段。UI菜单首轮失败是测试仍导航到旧第7行；更新新第6行后保留其队列满、取消、过期、代数和COMMIT超时覆盖。曾误将queries改为4，实测6来自Wi-Fi查询（旧factory不计入），已恢复原6断言。

HOST最终227/227，含Web scopes/failed/timeout/lostACK/duplicate/missing/unconfirmed/unknown/extra/busy/unavailable/first factory request、Core freeze/read/write/identity/preferences/rollback失败及fallback reboot；测试SDK/HTTP与physical lifecycle是fake，不能证明真实Flash/cache-off或浏览器效果。Node --check通过。LCD最终Default0x35b190、Stable0x35a380、Release0x34f200，三构建终态0，均<5MiB/6MiB分区；三compile graph各有一个Core持久化reset/纯事务对象，无factoryservice，实际ELF没有旧worker/request/result/quiesce/compat reset或camera_forget_pairing符号。日志build/module-web-factory-{host-build,host,default,stable,release,graph}.log。

ATOM未涉及未重建；无烧录、实机、提交或推送。完整计划仍在进行：正常Wi-Fi/UI preference写入口尚须按Web-only配置要求继续清理，A35启动顺序/partial-init、UI模型清空与最终S/V/A38审计及实机验收未完成。
