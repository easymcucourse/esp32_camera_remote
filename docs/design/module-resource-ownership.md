# 当前 LCD 资源归属与停止约束

[English](../en/design/module-resource-ownership.md) · **简体中文** · [日本語](../ja/design/module-resource-ownership.md)

> 2026-10-10：下方为有日期的详细台账，旧路径/件数/未完成项按当时范围解释；最新源码与验证以 [当前状态](../development/current-status.md)为准。当前main启动栈24576字节，旧32768表已被取代；Host基线267、四个新构建通过，未烧录。


采集日期：2026-10-06，按当前源码而非旧任务表核对。数值为代码配置，不能代替实机栈水位、调度、Flash/cache-off 或 SMP 时限证明。正常维护切换只允许重启退出；超时保留活动 owner，不强删任务。

## 任务

栈单位为字节；“未绑定”指普通 xTaskCreate，无指定 CPU。普通 FreeRTOS task 使用内部 RAM；PSRAM 与显式内部栈见 caps 参数。

| owner / task | CPU / priority / stack | 创建与停止证据 | 退出约束 |
| --- | --- | --- | --- |
| Core health | 未绑定 / 2 / 4096，显式内部 | app_core/app_health.c | boot barrier 释放后工作；维护与重启时持续控制 owner 停止 |
| Console router | 未绑定 / 5 / 4096 | app_console/app_message_router.c | 25ms housekeeping；关闭接收，等待 request owner 确认取消、lease=0，再退出 |
| UART gateway | 未绑定 / 2 / 4096 | common/debug_console.c、app_console_uart.c | UART0 RX 512、读取20ms；join后删除driver，失败保留待重试 |
| Input owner | 未绑定 / 4 / 4096 | app_input/input_service.c | 50ms；先完整 release，再停止；等待期间保留 Camera/Router |
| Physical ATOM | 未绑定 / 4 / 3072 | app_input_atom/atom_link.c | 50ms poll、写后15ms；设备归 task，rm_device失败重试；prepared无task时由Core stop归还 |
| Debug SIM | 未绑定 / 3 / 3072 | app_input_sim/input_sim.c | 10ms player / 50ms report；停止先释放；Release无实现代码 |
| Camera producer | CPU0 / 4 / 32768，PSRAM | app_camera/camera_runtime.c | 单owner；等待 backend cleanup、JPEG lease/completion后释放任务栈；两个开机buffer保留到重启 |
| Camera endpoint | 未绑定 / 4 / 4096 | app_camera/camera_endpoint.c | 物理owner停止后retire/join并释放result queue，超时不删除活动owner |
| Camera identity work | 未绑定 / 4 / 4096，内部 | app_camera/camera_identity_work.c | 单次NVS任务；输入复制到内部栈，完成信号后不再访问caller context |
| UI endpoint | CPU1 / 4 / 32768，PSRAM | app_ui/ui_message_endpoint.c | 承担JPEG renderer；停止先归还metadata/lease再退出 |
| UI connection refresh | CPU1 / 2 / 32768，PSRAM | app_ui/ui_renderer.c | 250ms；renderer gate关闭后join所有drawing/notification users |
| Debug display benchmark | CPU1 / 4 / 32768，PSRAM | app_ui/ui_bench.c | 与真实JPEG共用renderer；取消后free512KiB合成JPEG并释放精确Camera预约 |
| Wi-Fi normal endpoint | 未绑定 / 2 / 4096 | app_wifi_messages/app_wifi_messages.c | 状态200ms/RSSI2s；取消两TCP lane并等待退出后清理 |
| TCP command / event lanes | 各未绑定 / 2 / 4096 | app_wifi_messages/wifi_channel_messages.c | 两sole channel owners，取消IO并归还lease，全部idle后删lane queues |
| Wi-Fi config | 未绑定 / 2 / 4096 | wifi_esp32/wifi_config_jobs.c | 正常关闭后维护重新启动；token/current snapshot/result history保留 |
| Maintenance HTTP | SDK默认未绑定 / 3 / 6144，内部 | app_maintenance/maintenance_web.c、SDK HTTPD_DEFAULT_CONFIG | 3 sockets、15 routes、recv/send各10s；关Web gate→SDK queued session-close→httpd_stop同步join，无项目层有界停止保证；失败保留closed server |

2026-10-06 main初始化栈配置32768；2026-10-10 defaults已调整为24576，以上其余任务值需按实际sdkconfig核对。main初始化，初始化调用返回由SDK回收；Core只创建一个health。基础准备/trigger/normal/health顺序见[实际关系图](module-dependency-graph.md)。

## 队列、缓冲和持久对象

| 资源 | 唯一 owner / 配置 | 成功停止后的状态 |
| --- | --- | --- |
| Router endpoint inbox | Console，control/bulk：System8/1、UART8/1、Input8/1、ATOM4/1、SIM8/4、Camera16/2、UI16/2、Wi-Fi16/4；SDK队列控制块内部RAM、task-only envelope存储PSRAM | endpoint inactive、queued leases purge；队列分配保留到重启，不能描述成全部free；首次部分分配失败同时释放两类内存 |
| Request waiters / leases | Console固定16waiter、32lease slots；pending由request caller确认取消 | quiesce成功保证pending=0、lease=0，mutex/wake对象保留 |
| Input reports | Input静态16report ring，provider各有disconnect通知/handle/epoch | release并关闭registry；静态存储保留，无heap report队列 |
| Camera focus / results | Camera focus queue1，endpoint results4 | physical drain后释放；包括init成功但endpoint失败的focus queue路径 |
| JPEG frame slots | Camera开机两槽，各512KiB PSRAM | readonly lease最后引用+完成metadata确认后复用；停止必须等两者，缓存保留到重启 |
| UI JPEG cache | UI decoder handle、4096-byte PSRAM work；pixels借用当前canvas | 开机创建decoder/work，renderer排空/恢复只重置状态，保留到重启；不持有第三份全屏画布 |
| LCD frames | board两个1024×600 RGB565 PSRAM FB，两块10行内部bounce共40960字节；surface管理唯一写lease/generation | panel/FB/mutex/frame_done保留用于固定MAINTENANCE，正常写入永久关闭 |
| Font/cache/model | UI字体、384项glyph cache；model原子/短临界区 | 固定画面需要字体；normal model清空且frozen，保留健康标志/同步锁/单调generation |
| TCP lane queues | Wi-Fi bridge每lane jobs4、control1 | workers=0后free queues、clear channels；SDK socket归各lane/driver取消流程 |
| Wi-Fi config queue/history | ESP32 backend queue2、enqueue mutex、results8 | stop完成待写事务后冻结；保留config对象/当前快照/历史，维护可重新start |
| AP/netif/NVS locks | ESP32 backend及Core顶层Wi-Fi对象 | AP仍在线服务维护；正常bridge停止；radio生命周期不由Web直接创建 |
| OTA temporary buffer | HTTP handler 4096-byte PSRAM chunk；OTA SDK handle | 成功end或失败abort后free、upload_end；存储操作在内部HTTP栈 |
| NVS namespaces | wifi_ap由Wi-Fi backend；sony_remote/ui_prefs由common_runtime单一primitive | Core boot无直接配置writer；backend read仍修复无效blob；配对确认原内部worker保持，Web修改后reboot |

## Core 排空顺序与错误范围

normal_close 先关闭普通 Camera/UART/Input/benchmark/UI admission，保留安全 release 与完成消息。normal_stop 顺序：UART、Input、providers、benchmark → Camera physical → Camera endpoint → normal Wi-Fi config/bridge → preferences/UI endpoint → renderer → System/router。前置失败会阻止依赖owner提前删除；所有失败仅进入重启。单项预算来自 app_core_shutdown.c 的1000/3000ms，不是全流程严格单一deadline。HTTP activation等boot屏障且全normal停止后才可发布固定画面/启用完整路由。

初始化组合错误先等待 Input/preferences/UI，再关闭router；顶层失败先RESTART+boot release，关闭HTTP后尝试normal drain，再返回原错误由app_main fatal。未归还的owner保留至reset，禁止将fake时钟覆盖写成实机SMP/缓存禁用安全证明。未发生原地normal恢复。

核对来源为上述任务源、app_message.h/app_console.h、camera_frames.c、input_provider.c、wifi_config_jobs.c、ui_fonts.c 与 SDK esp_http_server 配置。实现/主机/构建证据见[本批记录](../records/module-symbol-owner-20261006.md)，完整验收见[清单](../development/module-split-checklist.md)。

## 事务、画布与持久化 owner 复核

| 资源 | 实际唯一写入口 / 执行 owner | 防止重叠的范围 |
| --- | --- | --- |
| PTP session/transaction | Sony backend只内嵌一个ptpip_client_t；Camera producer调用ops | endpoint只入队动作/设置或设置原子取消条件；会话close成功后才可创建下一实例，失败保留cleanup状态。网络command/event各lane拥有自己的channel，不能由外部close/free active I/O。 |
| 显示back buffer | display_surface.active_owner唯一canvas对象，board backend拥有两帧 | available semaphore限制一个lease；mutex检查对象地址/lease/generation/pixels/尺寸。refresh无论backend成功失败均清空active并归还；cancel仅匹配owner可归还；recover拒绝active lease。UI renderer再串行化普通绘制/解码。 |
| wifi_ap/cfg | wifi_saved_config.c；ESP32 backend storage mutex包围read/write | boot read可能修复坏blob；config worker与维护factory保存均经同mutex。factory先freeze config worker并等退出，成功保持frozen直到reboot，失败只尝试Wi-Fi rollback。 |
| sony_remote/guid、peer | camera_identity_store.c；正常one-shot camera_nvs内部RAM worker | producer同步等待完成后才继续，single Camera lifecycle拒绝重复start；维护只有全normal stop完成后可forget。primitive自身没有额外mutex，合法调用者必须维持phase隔离。 |
| ui_prefs/info、pad、schema | preferences_store.c；维护HTTP handler经Core callbacks写/reset | 普通UI只读；Core只在MAINT且无upload时允许写；当前HTTP server一个handler task串行处理，三个client socket不等于三个writer。primitive没有private mutex，不允许未来多handler绕过调用者序列化。 |

此处“唯一”指每个资源/namespace的实现入口与明确执行/阶段owner，不声称所有Flash操作来自同一任务，也不声称多namespace factory具备整体原子回滚。NVS SDK内部线程/Flash仲裁不属于项目源码扫描的证明范围。

LCD direct NVS set/erase/commit共13处，只在上述三primitive。`check_module_boundaries.py`新门禁拒绝其他main/components/common C源直接写及全局nvs_flash_erase；它是词法检查，不识别宏别名/函数指针，不证明SMP可达路径。正反例、owner回归及完整262host见[写入owner核对](../records/module-storage-owner-20261006.md)。

## 2026-10-06 开机固定图片与画布缓冲

生产图片包在 app_camera_init 一次性分配 512 KiB × 2（合计 1 MiB），启动/停止/重连不再分配或释放。超过单槽容量的数据由现有 backend/PTP 容量检查拒绝，不扩容。UI 开机分配 4096 字节 JPEG 工作区并打开唯一解码器，帧路径不再创建或销毁解码器。两块 1024×600 RGB565 画布（合计 2,457,600 字节）由 RGB 驱动开机创建，运行恢复只重启原 panel，三次失败后请求系统重启，不再替换画布。SDK 内部行为与真实坏帧后的解码恢复待实机验证。
