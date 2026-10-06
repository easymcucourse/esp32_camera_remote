# 2026-10-06 间接 ops/callback 源码绑定核对

直接archive符号图不识别函数指针实际目标。本次读取components公共/私有头的function-pointer契约、生产注册点与调用点，补齐运行图中的反向回调；不把词法清单当作动态调用证明。以下是当前项目自有的跨component回调及相关资源规则。

| 路径 | 注册／绑定和调用点 | 当前目标与上下文／寿命 |
| --- | --- | --- |
| Camera → generic backend → Sony | camera_backend_binding.c默认factory；camera_session.c create；camera_backend_sony.c静态ops；camera_backend.c分派 | 只有Camera绑定factory；producer调用Sony的open/close/property/action/liveview等ops；实例内嵌唯一PTP client，失败cleanup保留规则不变 |
| Sony → Camera property visitor | camera_properties.c collect；Sony properties visitor | 同步更新调用者提供的属性集合，借用context到ops返回；不向UI直接回调 |
| Sony/PTP → Camera cancel | camera_runtime.c cancelled → session → Sony factory → ptpip_client.c stopped | 原子读取runtime停止/会话失效条件；factory传入，不形成第二个控制owner；同步调用，Camera实例寿命覆盖client |
| app_wifi → wifi_esp32 ops | wifi_esp32.c静态ops/create → app_wifi_driver_bind；app_wifi.c分派 | Core选择实现，facade保存const表与opaque context；channel busy与持有对象数防止active I/O被close/destroy |
| ESP TCP → app_wifi → bridge cancel | wifi_channel_messages.c lane cancelled/deadline；app_wifi.c channel_cancelled/operation；wifi_tcp.c deadline检查 | 同步I/O借用栈operation/context，wrapper组合channel cancelled和原lane predicate；返回前不保存栈地址到异步任务 |
| app_maintenance → Core system/Web/OTA ops | app_core_maintenance.c注入三组表；maintenance_web.c、maintenance_ota.c、maintenance_trigger.c消费 | exclusive/reboot、phase/settings/factory/config completion/upload/restart；表内容复制到服务，NULL context不借用初始化栈。维护无Camera/Input/UI/Console对象或API |
| board → UI prepare/fonts | ui_renderer.c display_surface_init(prepare_connection,ui_fonts_init)；surface保存prepare；board_7b_backend.c资源初始化；board_lcd.c init/recover | task中调用字体初始化/绘制；初始两buffer准备及恢复后已确认可写back准备。不是每帧ISR绘制，不读取Camera或NVS |
| lease machinery → original owner | app_message.c最后ref returned；camera_frames.c、camera_outputs.c、lcd_sim.c、ptpip_client.c注册 | Camera帧仅release-store returned标志（最后context访问）；Camera view/UART SIM副本free；PTP atomic returned借用栈且transfer等待最后归还再return。可能执行于consumer/reply/error drain任务，不能假定router任务 |
| common UART runtime → Console gateway | app_console_uart.c debug_console_start_owner(command,poll,retire_uart)；common/debug_console.c读取任务 | 三个static gateway callbacks生命周期覆盖UART owner；retire在reader退出时停UART endpoint，不从common直接调用Camera/Input/UI门面 |
| Console request wait → cancellation owner | app_message_router.c request_cancelable；Camera discovery/PTP、Input service、UI bench传predicate | waiter循环task中执行，owner提供的context覆盖同步等待；PTP cancel_wait还发送非阻塞typed CLOSE，无裸socket操作 |

同component内部函数指针也逐类核对：Core health表由app_core_start绑定到Core OTA/maintenance/Camera编排；factory_reset_ops表由Core注入Wi-Fi保存/共享identity/prefs primitives（reserved/release是最终MAINT下的内部兼容no-op，不是可逆Camera lease）；Camera controls enter/leave绑定本模块短critical、stream时钟绑定本模块；Sony descriptor/scalar visitors与control writer绑定本模块，writer只调用同一个PTP实例；PTP wire IO/packet receiver绑定本模块；Wi-Fi jobs saved_write/reconfigure、TCP network/diagnostic均由同backend绑定；Input pad_action sink归本owner/service；SIM player apply/done归本provider；公共codec random等纯值helper不连接业务component。

SDK回调单列：board_lcd.c RGB frame_complete在ISR仅GiveFromISR后端frame_done；wifi_esp32.c事件注册指向本backend event；HTTP route/task、I²C/FreeRTOS任务入口均归各自owner。SDK内部任意回调、第三方codec与动态运行时的全路径未由本轮证明。运行图补画back→UI、Sony/PTP→Camera、lease/request→owner及deadline取消链；这些运行反向边不是CMake依赖环。

已有真实源码主机fixture覆盖各契约的成功/失败：camera backend/Sony+PTP、wifi facade/channel/bridge、Core maintenance、Web/OTA、display surface、board LCD、frame bus/router/Input/UI bench。沿用262/262完整回归；本批只有文档与本地源inventory，不改实现，因此不重复构建。A37仍部分：生产绑定源码与当前图对齐的证据增强，真实SMP/硬件/SDK动态路径尚待验，不能用callback表宣称没有所有可能旁路。S5.8静态门禁只覆盖其显式规则，远程CI未执行。

本地build/module-indirect-callback-inventory.json保存当前components/common C/H哈希与函数指针声明行，作为本次源码快照索引；匹配结果不是完整C语义解析或绑定验证，以上表格为人工逐注册/调用点核对。
