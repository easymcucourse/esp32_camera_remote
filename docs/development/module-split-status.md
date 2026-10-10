# main 模块拆分进度

[English](../en/development/module-split-status.md) · **简体中文** · [日本語](../ja/development/module-split-status.md)

> 2026-10-10：下方为有日期的详细台账，旧路径/件数/未完成项按当时范围解释；最新源码与验证以 [当前状态](../development/current-status.md)为准。当前main启动栈24576字节，旧32768表已被取代；Host基线267、四个新构建通过，未烧录。


采集日期：2026-10-06。依据 [拆分计划](../../main-module-split-plan.md)；完整计划仍进行中。

## 当前源码状态

- main 只调用 Core start。独立 Core 编排模式、正常停止、维护切换、启动屏障与失败排空；其公共头仅保留启动，内部服务/重启/factory 事务已收回 private。
- UI/display_surface/board 单向分层，Camera generic backend/内嵌 PTP Sony、Wi-Fi 对象/ESP32 backend/消息桥、Input/provider/SIM、UART gateway/router 已接入生产。正常跨域发现、TCP、参数及 JPEG 走 typed message/lease。
- 独立维护无认证，启动页 HTTP claim 后停止正常 owners、清空/冻结 model 并显示固定 MAINTENANCE。factory/config/preferences/OTA/退出仅 Web，成功后重启；普通 LCD/UART 配置写与旧维护入口已删除。
- 当前启动为 UI/I²C prepare → AP/trigger → 普通 router/Input/bridge/providers/Camera/UART → health/barrier release。UART 创建失败不启动 producer；失败关闭 HTTP 并排空正常 owner。源码流程不证明真实 I²C/AP 安全时序。
- 当前复核 263 项主机全部通过（原 54 保留），新增 [真实Wi-Fi factory契约](../records/module-wifi-backend-contract-20261006.md)及 [Camera／router／UI帧集成](../records/module-frame-bus-20261006.md)。LCD Default/Stable/Release 与 ATOM Debug/Release 五配置均编译通过；LCD 小于 5 MiB，Release 调试禁符号检查通过。五配置基线见 [接口与五配置核对](../records/module-api-build-audit-20261006.md)，最新LCD构建与Sony纯转发函数清理见 [Sony收尾](../records/module-sony-exposure-cleanup-20261006.md)。无本轮烧录或新实机证明。

## 尚未完成

2026-10-06 后续实机烧录发现启动内部 RAM 不足、Router SMP 锁竞争导致相机误超时、Input 新报告时间戳被误判过期及在线菜单 deadline 不足。已修复并重新构建，在线真实 I²C 菜单往返和故障恢复短测通过；最后 30 分钟在线测试在约两分钟时因取景拒绝及会话退出失败，用户另反馈 LCD 显示异常。按用户要求暂停并整理阶段提交，完整硬件验收尚未完成，见[本次修复记录](../records/communication-recovery-test-20261006.md)和[暂停交接](../records/pause-checkpoint-20261006.md)。上述五配置历史基线不是本批全部实现的构建／实机证明。

公共头与[46项消息契约](../design/module-message-contracts.md)已核对；Sony/PTP保留依据与纯转发收尾已补[源码核对](../records/module-sony-exposure-cleanup-20261006.md)；事务/画布/NVS owner已补[源码核对与写入口门禁](../records/module-storage-owner-20261006.md)；各 owner 所有退出/错误路径、编译依赖与运行关联图、[当前操作文档](../records/module-current-guides-20261006.md)与[133项验收范围](../records/module-final-software-audit-20261006.md)已补核对，部分项目仍需更强运行证据。五配置构建和 Release 符号检查已通过；维护 HTTP stop 的 lwIP 旁路已改由 SDK session-close 与 Web 停止门控收口；真实阻塞请求/SMP 停止时延仍待验。实机冒烟/启动安全/稳定性仍待完成。构建/主机测试通过，硬件待验证。

逐项要求见 [验收清单](module-split-checklist.md)。不能从已完成批次推断整个计划已完成。

## 历史批次

以下按迁移当时状态记录，文中的“仍待”“旧路径”等只描述当时，不能代替上述当前状态。正式原始证据保留在 docs/records。

本批主机61/61、三种LCD构建及Release符号检查通过，实际范围见 [Wi-Fi迁移记录](../records/module-wifi-20261005.md)。

Wi-Fi NVS 已收口，host62/62、三种LCD构建与Release符号检查通过，见 [本批记录](../records/module-wifi-store-20261005.md)。

本批恢复出厂协调已迁入app_core，Wi-Fi不再include相机身份/UI偏好；host63/63与三种固件编译通过，见 [本批记录](../records/module-core-factory-20261005.md)。异步Wi-Fi配置worker/token仍待迁移。

配置worker/token/current snapshot已归Wi-Fi后端，配置message已接入，host64/64与三种LCD构建通过；TCP仍缺，见 [本批证据](../records/module-wifi-jobs-20261005.md)。

TCP通道/backend/message worker与lease路径已建立，host67/67和三种LCD构建通过；PTP生产路径仍旧socket，见 [本批证据](../records/module-wifi-tcp-20261005.md)。

新增PTP/IP消息客户端基础，独立fake Console测试不链接app_wifi/lwIP；host68/68与三种构建通过。旧Camera/PTP生产路径尚未替换，阶段4.2仍部分，见 [本批证据](../records/module-ptpip-client-20261005.md)。

标准PTP的operation/data/response已收成同一wire engine，消息client管理新路径的transaction/session；旧生产fd/event路径仍待替换。host69/69、三种构建通过，见[本批证据](../records/module-ptpip-protocol-20261005.md)。

Sony通用data-out编码已收口到共享PTP wire，新消息client发送用实例transaction；原Sony写入回归逻辑/断言保持。host69/69与三构建通过；旧生产fd仍待替换，见[本批证据](../records/module-ptpip-send-20261005.md)。

PTP/IP实例command/event握手已建，原main exchange委托同wire逻辑。host69/69与三种固件通过；event poll/生产fd/事务路径仍未替换，见[本批证据](../records/module-ptpip-init-20261005.md)。

PTP消息实例已支持无buffer readiness poll和通用事件接收，旧/新事件解码共享wire；host69/69/三构建通过，生产fd/select仍待替换，见[本批证据](../records/module-ptpip-events-20261005.md)。

Sony值编码新旧路径共用一份encoder，新client接口复用PTP事务/消息I/O；host69/69与三构建通过。完整Sony backend与Camera生产切换仍未完成，见[本批证据](../records/module-sony-client-20261005.md)。

通用camera_backend契约与一次绑定校验已建，host70/70及三固件编译通过；Sony backend与生产Camera仍缺，见[本批证据](../records/module-camera-backend-20261005.md)。

Sony backend嵌入PTP唯一实例、通用ops/私有factory及同fake Console契约已建立；host71/71和三固件通过。生产Camera/旧fd与sony_camera物理迁入仍缺，见[本批证据](../records/module-sony-backend-20261005.md)。

参数状态机已与Sony dataset parser分离，generic backend快照复制/确认路径和独立fake backend测试建立；host72/72与三构建通过。新read尚无生产调用，Camera整体迁移仍缺，见[证据](../records/module-camera-properties-20261005.md)。

app_camera私有构建单元/参数内核迁入、通用backend session与private Sony factory选择建立；host73/73和三固件通过。新session/read尚未接入production，Camera endpoint/facade/controller余项未完成，见[本批证据](../records/module-camera-session-20261005.md)。

消息DHCP发现/串行通用backend probe/唯一候选选择与clear message已建；link policy归app_camera，queued动作改名避免类型冲突；host74/74与三构建通过。producer仍旧路径，endpoint/生产接入未完成，见[本批证据](../records/module-camera-discovery-20261005.md)。

Camera语义属性/连接/命令/caps发布与UI整体校验/代数/只读租约消费已建；host76/76和三构建通过。新producer尚未调用，旧直接UI仍待删，见[本批证据](../records/module-camera-ui-20261005.md)。

生产identity/actions已迁app_camera，generic owner controls保留安全释放/能力预读/录像回读确认，host77/77与三构建通过。新controls尚未生产接入，Camera endpoint/facade/主循环仍缺，见[本批证据](../records/module-camera-controls-20261005.md)。

原生产JPEG pipeline已切换Camera readonly frame lease→Console→UI decode/render，结果metadata/槽归还/十damage策略已接入；独立JPEG worker移至测试支持，host79/79与三构建通过。完整Camera endpoint/facade、PTP生产切换及余项仍缺，见[本批证据](../records/module-camera-frames-20261005.md)。

generic设置执行/Mode屏障/fresh permit、controls guard与录像执行期pending、backend拒绝语义已补齐；生产内部RAM identity worker迁app_camera。host81/81和三构建通过，新settings/controls尚未生产调用，完整Camera仍待切换，见[本批证据](../records/module-camera-execute-20261006.md)。

Camera私有stream串起generic属性/控制/参数/frame/outputs；双JPEG占用仍可释放，释放共用timeout预算。host82/82和三构建通过，尚未接生产producer，见[证据](../records/module-camera-stream-20261006.md)。

生产Camera inbox独立接收STOP/STATUS/caps/action/settings，STOP完成清理后确认，帧metadata转给owner；配对forget使用内部worker。host83/83和三构建通过，endpoint暂main/旧producer未切换，见[本批证据](../records/module-camera-endpoint-20261006.md)。

生产Camera主循环已实际切换generic session/discovery/stream/Sony backend/PTP typed TCP；旧fd/transaction/Sony初始化/直接Wi-Fi/UI setter已删除，host84/84和三构建通过。生命周期/endpoint仍main、UI menu读取与维护桥/facade待迁，见[证据](../records/module-camera-producer-20261006.md)。

生产Camera runtime/endpoint迁app_camera，Core-only公共生命周期；menu step/System维护策略/网络变化/预约forget使用typed消息。host85/85和三构建通过；legacy调用/独占维护/完整组合根未完，见[本批证据](../records/module-camera-facade-20261006.md)。

Sony 纯解析/校验/编码迁 camera_backend_sony private，旧 fd API 移测试支持，生产 ptpip 无 lwIP 依赖；85/85 与三构建通过，见[证据](../records/module-sony-ptp-cleanup-20261006.md)。

gamepad_input已归app_input；新增纯report仲裁内核，86/86与三构建通过。provider/消息service尚未接入，main暂含private gamepad头；见[本批证据](../records/module-input-reports-20261006.md)。

统一input_provider注册/复制report/断开/注销API、有界队列与private owner已建，host88/88及三构建通过；ATOM尚未接入，service生命周期/typed消息仍缺，见[证据](../records/module-input-provider-20261006.md)。

生产输入状态/电量/SIM已typed INPUT_STATE→UI，89/89及三构建通过；输入动作/导航与完整service/provider未接，见[证据](../records/module-input-ui-state-20261006.md)。

UI偏好NVS/内部worker已归app_ui，UART解析分离，INFO/PAD/RESET typed消息与System恢复出厂等待commit；host90/90、三LCD+ATOM双模Debug/Release通过，见[证据](../records/module-ui-preferences-20261006.md)。旧接口及quiesce/完整输入仍缺。

UI_MENU_ACTION已生产消费，主菜单/MORE导航归UI并回复semantic参数，ATOM不再直接修改导航或换算行号；host91/91与三LCD+ATOM双模构建通过，见[证据](../records/module-ui-menu-20261006.md)。Wi-Fi菜单adapter与完整Input service等仍缺。

Wi-Fi菜单kernel/worker已归UI，配置只发Wi-Fi/System消息，共享network_config纯值迁common且反向SHA证明算法不变；host93/93、三LCD及ATOM双模构建/门禁通过，见[证据](../records/module-ui-wifi-menu-20261006.md)。完整Input/UART/独占维护/Core生命周期仍缺。

Input service已生产接入：ATOM和本地SIM只publish report，统一owner只typed Camera/UI/actions/status/preferences，Core启动/health输入quiesce；host94/94及五构建/符号门禁通过，main不再含input private。独立provider组件/transport生命周期、UART/独占维护/Core全生命周期仍缺，见[证据](../records/module-input-service-20261006.md)。

物理ATOM/SIM已独立component/transport，Core-only lifecycle及device/结果归还、typed UART原始命令/readonly sequence lease/I2C诊断已接；host99/99与五构建/唯一sourcegraph/Release无SIM gates通过。完整UART/独占维护/Core UI/router停止仍缺，见[证据](../records/module-input-providers-20261006.md)。

UART Wi-Fi 编码器已只使用 Wi-Fi/System 消息，COMMIT 超时保留事务查询、换代清除旧密码；host100/100 与三 LCD 构建通过。编码器暂仍 main，完整 gateway 与维护/Core 编排待完成，见[证据](../records/module-uart-wifi-20261006.md)。

UART UI 偏好也已只用消息；info 保留异步 token，UI 完成结果在 UART 拥塞时保留重试；UART 统一 inbox 分派 UI/SIM 格式化，不再由 SIM poll 丢弃其他消息。完整 gateway 与旧兼容入口清理仍待完成，见[证据](../records/module-uart-preferences-20261006.md)。

UART 相机控制/fault/status/extra 已 typed；status 与九项 extra 都先发请求再等待，保留绝对期限，失败不打印虚构 OK 快照。s 仅 UART admission ACK，Core STOP 仍物理排空；main camera_console 无 Camera/UI 私有函数。维护/bench/Core mode/独立 gateway 生命周期仍缺，见[证据](../records/module-uart-camera-20261006.md)。

显示基准执行已归 UI、UART 仅 typed 请求/结果，Camera 独占预约等待真实排空且超时需归还；Debug fault/bench 裁剪，System mode 接 Core 过渡维护策略实时快照。host108/108 与三 LCD 构建/Release 符号和源码门禁通过，见[证据](../records/module-ui-bench-20261006.md)。完整独立 gateway、Core 生命周期和不可逆独占维护仍缺。

UART 行读取/正常编码器/help/status 已整体归 app_console，Core-only independent start/quiesce，UART失败不停止router/Camera；维护串口/探针及旧手柄维护API已删。host111/111，见[证据](../records/module-uart-gateway-20261006.md)。Core完整组合根、UI/jobs停止、启动独占维护及兼容API清理仍缺。

UI偏好worker已支持Core quiesce：关闭admission、取消排队写、等commit/同步caller通知完成才释放，超时保留；原阻塞策略/task/队列/NVS保持。111/111主机通过，见[证据](../records/module-ui-preferences-stop-20261006.md)。其他UI workers/endpoint/显示锁及完整独占维护仍未完成。

UI Wi-Fi菜单worker已Core quiesce：关闭admission、取消普通RPC、known staged token取消在拥塞时保留重试、双endpoint退休不发送旧token，任务退出后删除队列。111/111主机通过，见[证据](../records/module-ui-menu-stop-20261006.md)。菜单停止不证明Wi-Fi/System jobs排空，UI endpoint/renderer与完整独占维护仍缺。

UI消息worker已Core quiesce：排队JPEG不解码/归还lease/保留frame completion直到可发送或退休，超时保留endpoint/task，start失败清理及冻结订阅重启有回归。host112/112，见[证据](../records/module-ui-endpoint-stop-20261006.md)。refresh/renderer与固定维护锁、Core全局停止/独占维护仍缺。

连接页refresh与renderer已不可逆关闭，等绘制/通知归还后释放JPEG资源，新增现有画布固定MAINTENANCE及失败重试/幂等。host113/113及三LCD构建通过，见[证据](../records/module-ui-render-stop-20261006.md)。最终Core尚未引用固定画面API，模型兼容setter及全局独占维护仍缺；不代表S3.5整体完成。

Core已加入factory admission关闭/queued取消/活动事务等待，以及正常配置worker→Wi-Fi消息bridge共享budget停止；保留AP供后续独立维护。host115/115、三LCD构建通过，见[证据](../records/module-core-network-stop-20261006.md)。System/router全局停止、完整组合根和独占Web维护仍缺。

Camera endpoint stop已有界并等待task/结果queue归还，Core退出链新增System退休→router waiters/leases/housekeeping task确认退出；timeout保持闭锁且拒router restart，冻结Camera订阅重建有回归。host115/115与三LCD构建通过，见[证据](../records/module-core-router-stop-20261006.md)。最终启动组合根/不可逆维护激活尚未绑定，旧compat仍需清理。

main entry已仅调用Core，startup/NVS/Wi-Fi对象创建归Core，正常启动固定订阅freeze；UART未ready则推迟。host121/121与三LCD构建成功，见[证据](../records/module-core-start-20261006.md)。旧maint服务仍main，过渡link contract/WHOLE_ARCHIVE必须随app_maintenance迁移删除；不是S1.6整体完成。

HTTP监听已通过后端绑定SoftAP接口及具体地址，控制UDP仅回环，错误拒绝ANY回退；123/123与三LCD构建/对象路由检查通过，见[证据](../records/module-http-scope-20261006.md)。独立app_maintenance/启动触发/无认证及不可逆模式尚未完成；SDK HTTP→Wi-Fi backend额外链接边须列入最终图。

独立app_maintenance已接实际OTA/header/单上传gate与注入系统ops，OTA无Camera/UI/Console依赖；boot health归Core并用app_wifi状态，147/147、三LCD构建与对象/link检查通过，见[证据](../records/module-maintenance-ota-20261006.md)。完整Web/trigger/auth删除与Core不可逆切换仍缺，旧mode接入adapter及WHOLE_ARCHIVE待删除。

实际Web/页面/JSON/Wi-Fi patch已归app_maintenance，生产认证删除，settings通过Core回调写schema1共享存储并重启；退出也重启、Wi-Fi staged token跨generation可查。167/167、LCD3+ATOM2及独立对象/link检查通过，见[证据](../records/module-maintenance-web-20261006.md)。startup trigger/不可逆mode/factory/fixed UI仍待接，main旧controller/compat/WHOLE_ARCHIVE暂留。

Core 原子模式与 UI 的 ENTER_NORMAL 许可已接入生产帧/设置页/Debug 基准，新增配置 worker 停止后的独立重启接口，保留 AP 和旧 token。169/169、三 LCD 构建及生产符号验证通过，见[证据](../records/module-core-mode-20261006.md)。维护 claim/activate 尚未生产绑定，任意 HTTP 首请求、全局停止/固定画面及旧 mode 清理仍待完成。

独立维护新增 init/open/close/activate/status/stop lifecycle，统一 HTTP dispatch 与404/405覆盖任意首请求，获批并激活后302/重载才执行完整Web。179/179、三LCD构建与实际对象隔离检查通过，见[证据](../records/module-maintenance-trigger-20261006.md)。Core尚未调用该lifecycle，旧controller兼容gate暂留，生产启动触发与全模块独占切换仍待接入。

Core真实绑定独占lifecycle，停止全部正常owners/router后发布固定画面、重启唯一config worker、提交维护gate/Coremode；正常UI许可前物理关闭HTTP。旧mode/adapter/Main兼容头/WHOLE_ARCHIVE删除，main只entry。204/204、三LCD构建/生产调用链验证通过，见[证据](../records/module-core-maintenance-20261006.md)。A35最终启动顺序/partial-init cleanup/factory Web/旧UI菜单model清理及全量审计仍缺；前段记录是各批当时状态。

最新消息路径/旧头核对与真实UART Input编码器覆盖见 [审计记录](../records/module-route-header-audit-20261006.md)。原有待实施 S5.1/S6.2 已由实际源/链接证据更新；其余硬件与全项审计仍未结束。

Sony七个client纯转发入口已合并到backend/encoder，完整260回归及最新三LCD构建通过；当前使用清单40global/37ELF/3fixture-only。仍缺合并后的真实相机smoke，见 [控制层合并记录](../records/module-sony-control-merge-20261006.md)。

未引用PTP声明清理及当前文档精确路径分类见 [本批记录](../records/module-unused-declarations-20261006.md)。三LCD构建和260回归仍通过，生成镜像hash不同，因此没有bit-identical声明；设备信息与新固件硬件验收仍待确认。
