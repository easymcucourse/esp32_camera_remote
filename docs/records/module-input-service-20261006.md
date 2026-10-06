# 2026-10-06 统一输入服务接入生产

Core 在 router/System/UI 之后启动 app_input。新增 Core-only app_input_start/quiesce；唯一 input_owner 任务4096字节内部RAM/prio4、50ms节拍，report ring16及独立断开通知继续由 app_input 所有。main 不再包含输入 private目录；atom_link/maint_mode/camera_pair 的共享值只引用 pad_types。没有新增像素buffer、JPEG slot或NVS worker。

ATOM 原3072/prio4任务只完成 I²C/HELLO/POLL/重试/ack/原始report，删除 gamepad 状态机、Camera caps/action、UI getters/menu转换及维护业务回调。真实ATOM与Debug本地SIM分别注册ATOM/UART_SIM handle，normalized source_epoch/report_id独立正数不回绕；periodic id与cached edge ack_id分开，reboot/改型/换transport/source_tag提升epoch并断开旧输入，gap/缓存丢弃语义保持。旧诊断快照只供过渡UART；两个provider仍共用 main 内的transport task，独立 app_input_atom/app_input_sim组件和transport生命周期待完成，不能称S2.5/S2.6完成。

Input service通过Console RPC读取 Camera capabilities（50ms轮询）与 UI status/preferences（250ms）；只有service运行 gamepad/report owner。所有 Camera_ACTION、Camera_MENU_ACTION、UI_MENU_ACTION、UI_PREFERENCES和INPUT_STATE来自服务。UI导航回复semantic property后发Camera菜单消息；Wi-Fi route已由UI内部消费。Input不引用Camera/UI/维护实现头或函数、NVS/driver。旧MAINT_TOGGLE动作暂消费无操作，不再调用维护；纯kernel枚举/长按及旧UI维护行仍待最终启动:80策略删除。

RPC使用原Input端点lifecycle generation、目标epoch复核、100ms绝对deadline；普通请求随Core停止取消。Camera action回复post-admission caps包括record_pending/safety generation；安全释放使用最新generation，失败保持reserved release barrier重试。早期deadline/envelope错误没有caps，只有非零caps.generation才是有效快照，空错误reply不能确认安全释放；offline有效快照可确认没有held controls。动作成功仅代表Camera owner接管，物理释放和JPEG drain仍由后续Core Camera STOP确认。

UI RELEASE_ALL为无deadline非REQUEST通知，保留pending bit直到router admission成功，不堵塞Camera释放。quiesce立即关闭report入口、选择NONE/完整释放、排空断开通知、重试UI取消再stop Input endpoint；超时保留运行worker不能重复启动。重新start要求旧provider均注销，opaque handle跨生命周期不复用。Core health重启先quiesce输入再Camera drain，输入失败单独记录且仍尝试Camera排空。完整 provider/UI/router/UART/configjobs stop尚未集成。

registry timestamp在入队时采集（mux外）；owner拒绝队列积压超过1000ms的report并释放，service在1000ms无新报告时断开，避免旧RT/肩键滞留。connected报告必须atom_online或sim且无mismatch。pad配置通过INPUT_ATOM_COMMAND送provider，来源/代次/deadline校验；初次UI GET成功前不发送默认改型，provider等pad配置再首次HELLO，重注册后重新发配置。Debug UART切SIM用INPUT_SELECT RPC，接管后才改transport；UART端点现由过渡camera_console启动注册，独立gateway待迁。

主机94/94通过，HEAD原54名称全保留，未删断言/降低告警。新增test_input_service编译真实service/registry/owner/gamepad+fake router/RTOS，验证创建失败清理/固定任务与queue属性、typed caps/menu/info/preferences、providerepoch补发、维护不旁路、UI队列满独立安全重试、Camera timeout/emptyexpired reply/epoch变更、来源权限、完整停止/超时及注册未归还拒重启。registry增加connected/SIM/mismatch、注销后restart旧handle拒绝；owner新增>1s backlog释放；Camera endpoint新增菜单semantic alias与action拒绝时cap回复测试。fake非RTOS/SMP/cache-off/硬件证据。

最终五构建成功终态，尺寸 {"default": "0x35ad80", "stable": "0x359f60", "release": "0x34e2e0", "atom-debug": "0x1042a0", "atom-release": "0x101690"}；LCD均<5MiB，Stable80MHz，ATOM dual Debug/Release BTDM/BLE/GATTC配置与Release符号门禁通过。ELF确认app_input_start/quiesce/input_provider_publish/input_owner_tick实际链接，没有旧input_action/menu_action。日志 build/module-input-service-{host-build,host,default,stable,release,atom-debug,atom-release,symbols,test-preservation}.log。移除main private include后camera_pair旧头导致三LCD重建失败，已改为pad_types，失败日志保留*-private-header-failure.log。边界/document links/diff另有最终验证。无活跃build/串口会话，其他聊天未检查；未提交、推送、烧录或实机验收。

完整目标active且范围不变：独立provider组件/transport stop、UART全消息gateway/bench/故障、UI/preferences/menu全quiesce、启动:80无认证独占维护、完整Core组合根/固定订阅冻结、legacy接口/死代码/资源与编译运行图和[全清单](../development/module-split-checklist.md)仍待完成。
