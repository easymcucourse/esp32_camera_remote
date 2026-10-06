# 2026-10-06 Core 独占维护接入与旧控制器删除

Core 新增 app_core_maintenance.c/app_core_shutdown.c，生产启动注入独立维护 lifecycle、Web settings/restart 与 OTA ops，打开真实 SoftAP:80 trigger。首次请求的回调只争抢 STARTUP→ACTIVATING、关闭新工作许可并等待 health；原4096内部RAM/priority2/250ms health任务执行全局停止，没有新协调任务、队列或画布。UI ENTER_NORMAL 走同一原子胜者判断，关闭 gate 并实际 httpd_stop 成功后才许可绘制；失败转 RESTART，不能回到 STARTUP。维护没有空闲TTL，也没有正常模式恢复。

normal_close 只执行一次，原子完成标志防止 HTTP callback 与 health 同时删除同一 owner。首先关闭 Camera 普通操作、factory/UART/Input/bench/menu admission；UI 新增 close_admission，拒绝绘制/普通消息但保持 endpoint 活着归还 JPEG lease/完成 metadata。最初直接用 UI quiesce(0) 会过早退休 endpoint，已改为此分离接口。health 等待关闭完成并回复已排队的 session/正常请求，随后等待 factory、UART、Input、providers、bench/menu、Camera物理停止、Camera endpoint、正常配置与Wi-Fi bridge、preferences、UI endpoint、renderer、System/router。任一步失败不得激活 Web，只走重启；依赖未满足不释放其下游资源。

Camera 原 quiesce 仍为正常事务可恢复预约，不能替代独占停止。新增 Core-only close_admission/stop：普通 start/control/setting/focus/forget 拒绝，安全释放/metadata继续；stop 等 busy/physical worker 返回，超时永久闭锁，成功重复幂等。旧预约的 release/display_end/timeout 均不能解除 exclusive gate。endpoint停止后释放原 focus queue；两个1MiB JPEG槽、backend/channel的真实归还仍由原物理owner负责，未强删任务。新 UI close 后禁止本次boot重建endpoint；通用 quiesce 的旧普通stop/restart回归仍保留。

所有 normal owners/router停止后，Core调用现有 app_ui_enter_maintenance，在原画布发布固定MAINTENANCE；之后仅 app_wifi_config_start 重启同一个配置worker，保留AP/current/saved/token history，不恢复正常bridge/TCP；最后先激活维护gate，再提交Core MAINTENANCE，HTTP等待者才能看到一致成功并返回302。这个顺序避免先提交Core状态、HTTP先醒来却看见未激活gate的竞态。重启/OTA rollback使用新关闭流程，维护写偏好只调用共享schema1存储，不再局部调用正常UI preferences quiesce。

旧 main/maint_mode.c/.h、Core ota_compat/private maintenance_compat 删除；Camera不再注入旧session_changed/maintenance_active callbacks，session admission由Core真实mode判断。maint_confirm/maint_notice原文件逐字迁至tests/support/legacy，原fixture断言未变。无调用方的 main camera_pair/ui_preferences/wifi_ap 兼容头删除。main/CMakeLists现在只注册 app_main.c、依赖app_core，临时WHOLE_ARCHIVE删除；构建图与边界门禁验证，不只是入口函数改名。维护未初始化的旧Web放行分支也已删除。

仍有明确余项：当前保留原ATOM先于AP、Camera/UART初始化先于trigger发布的硬件安全顺序，HTTP在所有正常owner初始化完成后、health创建前打开。不是计划A35最终“基础UI/Wi-Fi→trigger→正常服务”顺序；需要拆开启动许可/准备阶段、完整partial-init unwind及异常UART固定订阅freeze。Web factory尚未接，正常factory worker/请求仍是过渡代码；UI旧维护菜单文本和model兼容setter/clear未清完。最终依赖/资源/逐项验收仍需全量审计。生产源码绑定不是实际设备进入维护或视觉/稳定性证明。

204/204主机通过，原54保留且没有降低告警。新增Core真实maintenance/shutdown/mode目标24场景：逐个factory/UART/Input/providers/bench/menu/Camera/endpoint/network/preferences/UI/renderer/router失败、fixed/config/activate失败、timeout、Normal先胜与HTTPstop失败、配置stop失败和初始化失败；验证没有等待者内stopHTTP、完整顺序、停止成功缓存、只能重启、偏好/上传互斥。physical服务及独立lifecycle回调为fake，不证明RTOS并发。另保留真实原子300争抢、独立HTTP/SDKcJSON全路由测试及原workers/drain回归。Core boot/policy fixture按删除的旧callback接口更新断言，保留初始化/重试/失败与事务测试，并新增trigger-open失败；没有删除测试目标。

真实Camera producer fixture增加安全释放、20ms stop超时保持闭锁、worker仍在时拒成功、旧release不能重开、成功幂等与focus queue归还。真实UI endpoint fixture增加关闭许可仍保持endpoint/结果归还、最终stop、禁止重开。新增UI断言最初误以为旧frame5未增加render次数，已改为记录该阶段的实际计数并证明关闭后不增长；旧断言未修改。首个新target host include顺序导致两个FreeRTOS stub宏重复，修正include顺序后-Werror通过，未屏蔽告警。

最终 LCD Default0x35bdc0、Stable0x35afb0、Release0x34feb0成功，均<5MiB/6MiB分区。三compile graph新Core/lifecycle单一owner、main仅entry、旧controller/adapter无源；实际ELF有Core init/poll/normal stop、trigger init/open/activate、fixed UI、Camera/UI close、config_start调用，旧maint_mode/auth/adapter符号全无。Release完整SIM/bench/encoder/debugfault/维护UART禁用符号检查通过，维护trigger对象无正常功能引用。日志build/module-core-maintenance-{host-build,host,default,stable,release,graph}.log；ATOM本批未涉及未重建。

HTTP shutdown唤醒客户端后使用SDK同步httpd_stop，SDK没有timeout参数；Core stop传入budget仅适用于配置worker。真实HTTP join、SMP/cache-off、硬件时序、字体像素、30分钟稳定性和真实OTA/Flash仍待最终验证，不能以fake证明deadline。全部本批build handles终态，无本批后台构建/串口，其他聊天未检查；没有烧录、实机、提交或推送。完整目标active。

最终边界/文档/diff门禁通过：127文档、542本地链接、0问题。
