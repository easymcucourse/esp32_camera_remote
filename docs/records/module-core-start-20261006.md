# 2026-10-06 Core正常启动组合根

main/app_main.c现在只有app_core.h和ESP_ERROR_CHECK(app_core_start())。新增Core app_core_start owns chip/NVS/UI初始化、router/System/UI/Input、ATOM/SIM providers、AP/message bridge、factory/旧维护、Camera/UART、health启动。运行在原ESP-IDF main任务栈，没有新建初始化任务或改变字体初始化栈；正常ATOM在AP启动前的原硬件安全顺序保持。一次attempt后禁止部分初始化重试；返回错误由main保持原fatal启动语义，不擦NVS，不静默丢弃已初始化owners。NVS不可用仍保留内容并使用默认配置。完整startup失败cleanup与最终maintenance优先启动时序还未完成。

main/wifi_ap.c移为Core app_core_wifi_compat.c，Wi-Fi对象由Core创建/保存配置读取/AP启动并绑定同一对象。创建/初始化/启动失败改为向Core返回原分类错误，默认配置与saved写/reconfigure/token语义保留。Core自己的private兼容头声明旧适配；main/wifi_ap.h暂仅供旧维护Web调用。去除main的Wi-Fi启动声明及不再需要的Camera/Input/ATOM/UART/I2C/JPEG/PSRAM/Wi-Fi backend依赖；该target仍包含旧Web/OTA，不能宣称main只注册入口的全部计划要求已满足。

Factory Camera reservation通过app_camera的Core-only lifecycle quiesce/release，失败release只允许未来start、不自动恢复producer，替代Core包含main/camera_pair.h。原forget/UI reset typed System requests、五秒deadline、reply release和saved写保持。旧维护仍依赖旧camera_pair/偏好函数，不是最终公共facade清理。

正常boot固定endpoints/订阅建立后Core冻结router订阅。UART初始化失败不抑制Camera启动；此时Core延迟最终冻结，避免尚未分配UART endpoint时冻住后无法重试。异常路径最终freeze/retry政策仍需最终组合根落实，不能以正常freeze证明全部failure lifecycle。

第一次固件链接失败，原因Core root引用的旧maint_mode/OTA仍在main archive中、已无app_main直接引用来拉入对象。迁移期main CMake使用ESP-IDF支持的WHOLE_ARCHIVE加载这些旧对象，section GC仍开；main/Web→Core、Core→main legacy服务的运行时临时关联显式记录在app_core_maintenance_compat.h。这是尚待消除的跨域旧依赖，不是最终无环架构；迁入隔离app_maintenance后必须同时移除compat contract、main旧源/资源与WHOLE_ARCHIVE。初次失败日志build/module-core-start-{default,stable,release}-initial.log保留，不隐藏该差异。

新增真实Core start fixture Debug/Release与四个变体（NVS失败继续、UI初始化失败、provider失败、UART未ready推迟冻结），共六注册。验证启动顺序/one-shot拒重试、失败不运行后续阶段、Core factory ops使用相同Wi-Fi对象及Camera reservation/message/reply归还。fake不运行SDK/NVS/硬件，主机121/121通过；原54注册与旧断言保留，-Werror保持。新增main entry/no main Wi-Fi composition静态边界门禁，不把该窄门禁当S1.6全部完成证明。

LCD Default `0x35bed0`、Stable `0x35b0b0`、Release `0x34fef0`当前源码构建成功，均<5MiB/6MiB分区。三个compile graph Core startup/Wi-Fi object source唯一owner、无main/wifi_ap.c；ELF保留生产app_main/app_core_start/quiesce-release/freeze路径，Release无SIM/bench/Debugfault/encoder/维护UART探针源/符号。boundary/diff通过，最终文档检查补充如下。最终日志build/module-core-start-{host-build,host,default,stable,release}.log。

ATOM本批未改未重建；无烧录、实机、提交或推送。所有本批构建会话终态，无本批后台串口/构建，其他聊天未检查。完整目标active；下一步必须完成独立app_maintenance/无认证SoftAP:80 startup trigger/不可逆mode、Core固定画面绑定、配置Web-only存储、旧认证菜单compat/WHOLE_ARCHIVE清理，完成最终依赖/资源/全项审计。

最终文档检查121文档/524链接/0问题，diff通过。
