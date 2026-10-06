# 2026-10-06 Wi-Fi 菜单归 UI 与消息配置

main/wifi_menu.c/.h、wifi_menu_ui.c/.h 已移入 components/app_ui，头为UI private；main/CMake/app_main不再编译/单独启动菜单，ATOM不再包含或调用旧Wi-Fi菜单API。app_ui_messages_start拥有菜单启动。编辑/草稿/SSID/九行文本/随机密码/默认提示/信道/显示密码/三秒双确认/APPLY/RESET ALL/返回逻辑由UI私有kernel+worker完成。原4096字节内部RAM/prio2任务、队列16/50ms节拍保持；每份job复制动作/sourcegen/UIepoch/deadline，静态限制<=48字节（队列metadata上限768）。无新增任务/像素buffer，完整UI生命周期停止尚待完成。

UI_MENU_ACTION在UI endpoint验证后入本组件队列。页面reply包含opening admission；输入依semantic Camera property回复发相机命令，Wi-Fi route已在UI消费，不转回main。RELEASE_ALL非REQUEST/无deadline通知仍校验sourcegen与deliveryepoch；私有cancel flag不受菜单队列满影响。旧ATOM保留单bit pending取消，在原50ms循环/退避25ms片非阻塞重试router admission，不延迟Camera安全释放。UI取消未被worker处理前拒新job，避免ACK后被reset队列删除。

打开/保存用typed WIFI_STATUS读取实际config/max_channel，再PREPARE→COMMIT，轮询RESULT确认最终结果。UI不包含app_wifi/wifi_ap/驱动/NVS头或调用其函数。配置冲突检查、display-only保留草稿、失败恢复show值、token pending锁编辑保持。COMMIT超时可能已运行，保留token并尝试取消未commit reservation，继续查询真实结果，不把排队接受当保存成功。job处理前和读配置后复核原sourcegen/UI epoch/deadline；PREPARE/COMMIT不延长原动作期限。RESULT查询独立500ms期限，transport失败保留token。

RESET ALL发SYSTEM_FACTORY_RESET/RESULT。Core System仅接受UI/UART REQUEST、无lease、nonzero/current sourcegen/current deliveryepoch/futuredeadline，返回原factory内部worker token/完成状态，不在UI栈写NVS。原freeze/Camera保留预约/身份和UI偏好清除/失败回滚/成功重启算法保持，既有factory测试仍通过。旧正常维护策略未替换为最终启动:80独占模式。

公共纯值从app_wifi/wifi_config.c及public头迁common/network_config.c/.h，规范名称network_config_t/network_config_*；功能API仍app_wifi_config_*，SDK wifi_config_t仍后端私有。common_runtime唯一编译无SDK/RTOS/NVS值逻辑，UI和Wi-Fi共享，不建转发兼容头或复制校验算法。字段长度/default/SSID-password-channel规则/随机拒绝采样/100字节v1存储完全保持。反向标识符还原后源码/头SHA与迁移前一致，证据build/module-wifi-menu-value-move.json、module-ui-wifi-pure-value-proof.log。原测试仅对应标识符/路径替换，未删断言/降低-Werror；展开HEAD foreach，全部54原注册名称仍在当前93个测试中。

test_ui_wifi_menu执行真实UI model/message/kernel/worker，fake queue/timer/router覆盖：创建失败清理重试/原task资源、opening/SSID编辑、COMMIT transport timeout+真实RESULT、PREPARE失败/display恢复/成功、System factory失败、队列16满/无deadline取消/newjob拒绝、queued sourceepoch/绝对deadline、配置外部冲突、读取后过期不prepare、随机密码DEFAULT标记、连接代数关闭。test_core_factory_messages执行真实Core poll，验证token/result、Input来源禁止、deadline/sourcegen/targetepoch stale/no lease、UI/UART入口；Wi-Fi message测试新增max_channel。host93/93通过，fake critical/调度非RTOS/SMP/cache-off/实机时序证据。

最终尺寸：{"default": "0x359850", "stable": "0x358a30", "release": "0x34ce10", "atom-debug": "0x1042a0", "atom-release": "0x101690"}。三LCD <5MiB且Stable80MHz配置正确；LCD ELF实际链接UI菜单/公共config，无旧wifi_menu_ui或旧纯值函数符号。ATOM双模Debug/Release BTDM/BLE/GATTC与Release禁用符号门禁通过。logs build/module-ui-wifi-{host-build,host,default,stable,release,atom-debug,atom-release,symbols,test-preservation}.log；首次机械jobs头误改失败保留default-initial-failure.log，误改jobs头和SDK类型均已修复。所有最终build handles成功终态，boundary/doclinks/diff通过。未提交、推送、烧录或实机验收。

完整目标active：Input service/ATOM与模拟provider真实接入、typed caps/actions/status/preferences查询、UART gateway/bench、UI偏好/菜单quiesce/资源回收、启动:80无认证独占维护、Core组合根/固定订阅冻结、旧接口/最终依赖图/全清单验收均未完成。见[全清单](../development/module-split-checklist.md)。
