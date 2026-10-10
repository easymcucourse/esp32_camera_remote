# 当前正常应用消息契约

[English](../en/design/module-message-contracts.md) · **简体中文** · [日本語](../ja/design/module-message-contracts.md)

> 2026-10-10：下方为有日期的详细台账，旧路径/件数/未完成项按当时范围解释；最新源码与验证以 [当前状态](../development/current-status.md)为准。当前main启动栈24576字节，旧32768表已被取代；Host基线267、四个新构建通过，未烧录。


采集：2026-10-06。依据当前 `app_message.h`、`app_console.h` 和实际 endpoint/producer 源码。编号前缀统一为 APP_MESSAGE_；保留编号不代表功能仍支持。维护 Web 不使用此总线。

## 每个操作共用的规则

接口供任务调用，不供 ISR 调用。router 保护队列、相关ID及引用计数；业务在下表 owner 上执行，不在 router 锁内回调。并发发送可失败，调用者不能假定单次入队必成功。普通 payload 按值复制，不能携带内部对象指针。

R 表示 REQUEST/reply：调用 `app_console_request*` 预留 correlation，设置非零 generation 及 esp_timer 绝对 deadline_us；reply 继承 type/correlation/generation/deadline，调用者释放 reply。请求入队与 `app_console_reply` 在原 deadline 内等待 router mutex，避免 SMP 短暂锁竞争被误报为通信超时；队列容量不足仍立即失败。普通 `app_console_send` 保持零等待。运输返回 ESP_OK 后仍须检查 reply.result。timeout 结束等待，不撤销已执行动作或未归还 lease。非R的定向控制和E（EVENT）可使用 deadline0；有正deadline时 router 会丢弃过期消息。

普通R及定向控制的 generation 通常使用源 endpoint lifetime；Camera 向 Wi-Fi/System 发出的发现、PTP和会话请求采用attempt/backend实例代际，router另行保存请求源与目标endpoint寿命。router 自行盖 destination endpoint_epoch。并非所有 handler 都额外校验源代际、来源权限或字段范围；表中的合法生产者描述当前调用路径，不是热点认证或安全边界。Wi-Fi网络代际、Camera safety generation、report source_epoch/report_id、channel token 与 endpoint lifetime 是不同标识，不能混用。

表中“值”表示不需要lease；BULK必须携带lease。readonly lease不能改写，writable lease仅供指定接收者写入。send 无论成功失败都消耗传入引用；receive/reply 返回的引用由调用者释放。buffer/context须存活到最后归还回调，不能因超时、停止入队或partial fanout错误而提前复用。endpoint stop释放queued消息；delivered引用不能撤销。control/bulk独立排队，receive优先control。

运输错误与业务错误分开：队列/锁拥塞或deadline通常为TIMEOUT，容量/分配为NO_MEM，生命周期/过期ownership为INVALID_STATE，参数为INVALID_ARG，不支持为NOT_SUPPORTED。各handler的具体分支以源码为准，例如Input统一将不合法envelope（含过期deadline）返回INVALID_ARG；不能把错误码概括为事务回滚。下表补充完成点与特殊错误。

## Wi-Fi owner

桥接endpoint消费普通查询，两个独立TCP lane owner执行阻塞I/O；endpoint自身不等待socket。普通查询只有router的R deadline，不承诺执行期间可取消。对应源码 `app_wifi_messages.c`、`wifi_channel_messages.c`。

| 编号 | 路径 / owner | payload、代际、所有权及完成语义 |
| --- | --- | --- |
| CAMERA_DISCOVER | Camera/UART R→Wi-Fi endpoint | 值；Camera使用发现attempt代际，UART使用endpoint寿命；reply.discovery复制最多4个client；不是创建Camera会话。 |
| WIFI_RSSI | Camera/UI R→Wi-Fi；Wi-Fi E→UI | 值peer；R查指定MAC，未找到RSSI=-127；E每约2s发布，generation为网络代际，失败允许下次刷新覆盖。 |
| WIFI_CHANNEL_OPEN | PTP R→Wi-Fi TCP lane | 值channel.address/port，generation为PTP实例generation（lane的owner_generation）；channel.generation0允许捕获当前网络代，否则必须匹配。reply返回token与网络代际；晚/拒绝OPEN reply会关闭新channel，无孤立owner。 |
| WIFI_CHANNEL_SEND | PTP R+BULK→TCP lane | 借用lease数据、channel.length；源+源代际+token+网络代际确定owner。reply移交同一lease，并给partial length和channel.status；lease归还前buffer不能复用。 |
| WIFI_CHANNEL_RECEIVE | PTP R+BULK→TCP lane；或R poll | 写lease接收，不能超过capacity；poll=true时length0且无lease，只报告readable不消费字节。reply ownership同SEND。 |
| WIFI_CHANNEL_CLOSE | PTP R→TCP lane control inbox | 值；token0可用opening_correlation取消未知token的OPEN。先cancel活动I/O、排queued jobs、关闭channel才ACK；保存8条closed记录供同owner重试。cancel动作可能先于ACK timeout发生。 |
| WIFI_CONFIG_GET | UI/UART R→Wi-Fi endpoint | 值config；复制当前配置，不写NVS。 |
| WIFI_CONFIG_PREPARE | 退休正常编号 | endpoint返回NOT_SUPPORTED；维护通过app_wifi异步prepare。 |
| WIFI_CONFIG_COMMIT | 退休正常编号 | endpoint返回NOT_SUPPORTED；不在普通总线提交配置。 |
| WIFI_CONFIG_CANCEL | 退休正常编号 | endpoint返回NOT_SUPPORTED。 |
| WIFI_CONFIG_RESULT | 退休正常编号 | endpoint返回NOT_SUPPORTED；不能依旧编号等待普通配置完成E。 |
| WIFI_SELECT_CAMERA | Camera R→Wi-Fi endpoint | 值peer.selected/mac；更新RSSI跟踪目标，不保存相机身份。使用Camera发现/会话generation及正deadline；不是E。 |
| WIFI_NETWORK_CHANGED | Wi-Fi E→Camera/UI | 值network.generation与envelope generation相同；无lease、control。发布失败重试同代；Camera更新取消条件，producer后续将新代通知backend。不是同步排空ACK。 |
| WIFI_STATUS | R→Wi-Fi；Wi-Fi E→UI | 值network含配置/address/online/max_channel/generation；E generation为网络代际，每约200ms刷新。失败下次快照覆盖。 |

## Camera owner

Camera endpoint与实际producer分离；endpoint接受动作/设置不等于PTP已完成。producer唯一拥有backend/PTP会话；frame结果元数据经独立队列送回producer。源码 `camera_endpoint.c`、`camera_runtime.c`、`camera_outputs.c`、`camera_frames.c`。

| 编号 | 路径 / owner | payload、代际、所有权及完成语义 |
| --- | --- | --- |
| CAMERA_START | UART/Core控制或R→Camera endpoint | 值command.flag：false预览/true配对诊断。active或有pending stop时拒绝；成功表示producer已启动，不等于网络已连接。 |
| CAMERA_STOP | UART/Core控制或R→Camera endpoint | 值；UART R flag=true仅ACK停止请求已接受，其他源不可使用。普通R等对应producer lifetime退出才ACK；deadline过期退休待回复条目，不恢复相机。 |
| CAMERA_FORGET | 退休正常编号 | NOT_SUPPORTED；身份删除只在维护存储流程。 |
| CAMERA_DISPLAY_SESSION | Debug UI R→Camera endpoint | 值index1取得/index0释放、token/flag恢复；UI endpoint代际。取得等待真实owner停止；timeout不撤销reservation，UI仍须以同token释放。Release不支持。 |
| CAMERA_ACTION | Input/UART R→Camera endpoint | 值pad_action，其中generation为独立Camera safety generation；reply.capabilities包含最新安全状态。ESP_OK是动作接受，命令物理完成另由状态发布；无有效caps的拒绝不能证明安全release完成。 |
| CAMERA_SETTING_ADJUST | Input/UART R→Camera endpoint | 值command.index为语义property、direction±1、token可校验Camera safety generation。会话/closing失败STATE；成功为desired目标入队，后续状态/回读判定实际结果。 |
| CAMERA_MENU_ACTION | Input/UART R→Camera endpoint | 与SETTING_ADJUST同处理；不携vendor property code。 |
| CAMERA_FRAME | Camera E+BULK→UI及其他订阅者 | readonly JPEG lease；generation为frame/producer代际，command.token唯一帧序号，duration_ms读取耗时。send partial fanout失败仍等最后引用归还，提交dropped token；并非所有消费者都已收到。 |
| UI_FRAME_RESULT | UI定向control→Camera endpoint/producer | 无lease，generation沿用原frame，command.token/result对应原帧；不是UI lifetime。TIMEOUT缓存最多2条metadata；router/Camera停止后丢弃metadata，JPEG由调用者照常释放。 |
| CAMERA_STATE | Camera E→UI | 值camera字符串/stage/busy/session/stopped；generation为producer代际。过时代UI拒绝；发布失败后后续阶段/重试刷新，不承诺可靠事件日志。 |
| CAMERA_CAPABILITIES | R→Camera；UI亦支持Camera E | 值capabilities，R按源寿命、E契约按producer代际；capabilities.generation独立安全代。当前producer将caps包含在PROPERTIES lease中，无单独caps E发布调用。状态读取不控制生命周期。 |
| CAMERA_PROPERTIES | Camera E+BULK→UI | readonly app_camera_view_t大小严格相等；语义property逐项index与status校验。producer构造独立快照，最后引用free；不能持有原backend属性指针。 |
| CAMERA_COMMAND_STATUS | Camera E→UI | 值command.index为control、value0..5；generation为producer代际。属性/动作接受后的显示状态，失败send不代表动作撤销。 |
| CAMERA_STATUS | UART/Input R→Camera endpoint | 值camera debug快照；源endpoint generation，无lease。读取瞬间状态，不保证整个reply等待窗口不变化。 |

## UI owner

UI endpoint消费模型/菜单/frame；renderer内获取surface并提交。维护admission关闭后拒绝普通消息，frame仍发送drop metadata并释放lease。源码 `ui_message_endpoint.c`、`ui_menu_messages.c`、`ui_preferences.c`、`ui_frames.c`、`ui_bench.c`。

| 编号 | 路径 / owner | payload、代际、所有权及完成语义 |
| --- | --- | --- |
| UI_MENU_ACTION | Input/UART R或安全control→UI endpoint | 值pad_action；源endpoint寿命与目标epoch需匹配。Input菜单动作采用500ms deadline，让UI sole consumer先完成当前JPEG；周期查询与Camera动作仍100ms。flags0/deadline0 RELEASE_ALL可安全取消。reply.menu将STEP解析成语义property，Input随后发Camera请求；Wi-Fi行只显示信息。 |
| UI_STATE | 未使用保留编号 | 当前没有producer/subscription/handler，发至UI返回NOT_SUPPORTED；不能把它当可用状态E。 |
| UI_STATUS | Input/UART R→UI endpoint | 值ui快照；源寿命，无lease，不保证后续仍处同一页面。 |
| UI_PREFERENCES | Input/UART/System/ATOM R→UI endpoint | GET-only、无lease、flag=false、正deadline/非零源寿命；reply.command.value=info、direction=pad。未ready STATE；其他旧写/reset编号NOT_SUPPORTED。 |
| UI_PROPERTY_STATUS | UART R→UI endpoint | 值command.index ASPECT..WB_GM；检查UART generation和UI epoch；reply.property为显示中的actual/target/status，不读取Sony backend。 |
| DISPLAY_BENCH | Debug UART R→UI；UI完成E→UART | 无lease；START token非零、duration30000；ESP_OK仅worker创建。STATUS同UART寿命/token查已完成benchmark，busy NOT_FINISHED、未知NOT_FOUND。完成E generation为UI寿命，拥塞重试，UART退休/取消时终止重试。Release不支持。 |
| DISPLAY_FAULT | Debug UART R→UI endpoint | 值command.value故障种类；返回renderer故障注入结果，非新的显示buffer owner。Release不支持。 |

## Input / provider owners

Input owner统一安全释放/来源仲裁；provider私有owner只操作raw report/协议/player。report经provider API进入Input，不经这张表发布业务动作。源码 `input_service.c`、`atom_link.c`、`input_sim.c`。

| 编号 | 路径 / owner | payload、代际、所有权及完成语义 |
| --- | --- | --- |
| INPUT_STATE | Input E→UI | 值input；generation为Input寿命，source_epoch/report_id独立描述report。UI完整验证范围/组合；周期发布失败允许后续覆盖。 |
| INPUT_STATUS | UI/UART R→Input owner | 值input；源generation/目标epoch/正deadline校验，错误envelope返回INVALID_ARG。 |
| INPUT_SELECT | UART/System R→Input owner | 值command.index ATOM0/SIM1；选择接受不代表安全release已完成，新源需待旧源释放才rearm。Release SIM返回NOT_SUPPORTED。 |
| INPUT_ATOM_COMMAND | Input定向control或UART R→ATOM owner | PAD_KIND仅Input，value0..1、源Input寿命及正deadline；UART操作LOG_MODE/STATS(flag reset)/LOG_READ用UART寿命与ATOM epoch。值i2c_stats/i2c frame复制，无borrowed monitor/lease。 |
| INPUT_SIM_COMMAND | Debug Input control/UART R/UI STATUS R→SIM owner；完成E→UART | PAD_KIND只Input；其他修改只UART，UI只能STATUS。SEQUENCE R+BULK精确readonly pad_sequence_t，复制到4job player，token非零；8完成记录拥塞重试、UART退休丢弃。完成E使用SIM endpoint generation及command.token/duration_ms/cancelled，不保留JPEG或Camera对象。 |

## System owner

System endpoint由Core health轮询，模式改变不可逆。来源合法性和业务验证按 `app_core_messages.c`、`app_core_camera.c`；不是独立认证机制。

| 编号 | 路径 / owner | payload、代际、所有权及完成语义 |
| --- | --- | --- |
| SYSTEM_STATUS | UART/UI R→Core poll | 值system.mode/uptime/free RAM/restart_pending，无lease；激活/维护/重启阶段仍允许状态读取（endpoint存在时）。 |
| SYSTEM_RESTART | UART R→Core poll | 值command.duration_ms，最小500、最大10000；prepare/commit成功后重启已计划，不因ACK丢失取消。激活/维护/重启模式STATE。 |
| SYSTEM_FACTORY_RESET | 退休正常编号 | NORMAL/STARTUP NOT_SUPPORTED；其他mode可能先返回STATE。factory仅维护Web。 |
| SYSTEM_FACTORY_RESULT | 退休正常编号 | 同上，无普通factory完成事件。 |
| SYSTEM_CAMERA_SESSION | Camera R或退出control→Core poll | 值command.flag进入/离开正常会话policy；正deadline/Camera attempt-session generation，无lease。进入只STARTUP/NORMAL；退出可在停止时通过。不是相机buffer/系统租约，也不能切换维护。 |
| SYSTEM_ENTER_NORMAL | UI R→Core poll | 值command.index LIVE/SETTINGS/Debug TEST；UI lifetime及System epoch校验。成功关闭trigger并进入不可逆NORMAL，UI按自身epoch缓存；失败不得继续显示普通页面。 |

## 验证范围

所有编号均列入此表，包括退休/未使用项；源码核对不是自动证明每条路径可靠。当前host261全回归含真实Wi-Fi换代发布失败重试、Camera/UI/Input/provider/System handler及lease/router测试；Camera→router→UI帧集成使用协作式fake RTOS。真实HTTP/PTP/SMP/cache-off、动作效果与停止时限仍待实机。见[网络事件修复](../records/module-network-event-20261006.md)、[帧集成](../records/module-frame-bus-20261006.md)、[资源归属](module-resource-ownership.md)。
