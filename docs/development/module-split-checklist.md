# 完整拆分计划验收清单

[English](../en/development/module-split-checklist.md) · **简体中文** · [日本語](../ja/development/module-split-checklist.md)

> 2026-10-10：下方为有日期的详细台账，旧路径/件数/未完成项按当时范围解释；最新源码与验证以 [当前状态](../development/current-status.md)为准。当前main启动栈24576字节，旧32768表已被取代；Host基线267、四个新构建通过，未烧录。


目标：完成完整计划并编译通过。以下从原计划逐项提取，不能以阶段性的绿色构建替代整体完成。采集：2026-10-05。

状态：已证实 / 部分 / 待实施 / 待验证。证据必须与要求范围相同；实际进度见 [拆分进度](module-split-status.md)。

| 编号 | 原计划要求 | 当前状态 | 证据 / 下一步 |
| --- | --- | --- | --- |
| S0.1 | 记录当前 54 项主机测试、LCD debug/release 构建结果和应用大小。 | 已证实 | docs/records/module-split-20261005.md：原测试/三个构建基线与创建参数/原头 include 清单 |
| S0.2 | 保存关键任务的核心、优先级、栈大小和启动顺序清单。 | 已证实 | docs/records/module-split-20261005.md：原测试/三个构建基线与创建参数/原头 include 清单 |
| S0.3 | 记录 `main` 对外头文件及调用方，禁止在拆分期间无意扩展接口。 | 已证实 | docs/records/module-split-20261005.md：原测试/三个构建基线与创建参数/原头 include 清单 |
| S1.1 | 创建 `display_surface` component，实现画布 acquire、refresh、cancel、状态和恢复接口。 | 已证实 | app_ui/display_surface/board_7b 源码；host display_surface/ui_model；私有头扫描 |
| S1.2 | 将现有 `board_lcd` 帧缓冲和面板操作收口为 `board_7b` 私有后端；移除应用可见的 back-buffer/publish 接口。 | 已证实 | app_ui/display_surface/board_7b 源码；host display_surface/ui_model；私有头扫描 |
| S1.3 | 创建 `app_ui` 的 model/renderer，将 `board_7b_set_*()`、字体和业务页面绘制职责迁出硬件后端。 | 已证实 | app_ui/display_surface/board_7b 源码；host display_surface/ui_model；私有头扫描 |
| S1.4 | 将现有应用调用方全部改为依赖 `app_ui`；只有 `app_ui` 依赖 `display_surface`，并且不再公开 `board_7b.h`/`board_lcd.h`。 | 已证实 | app_ui/display_surface/board_7b 源码；host display_surface/ui_model；私有头扫描 |
| S1.5 | 创建独立 `app_core` component，将 health、重启和跨域流程编排迁入其中。 | 已证实 | 独立Core组件含组合根/模式/health/重启/失败排空与正常停止；public只start，main仅依赖Core。source、core_start/mode/maintenance/shutdown fixtures与实际archive验证。创建/迁移要求已证；真实生命周期见V18/A31，不混入本项。 |
| S1.6 | 将 `main/app_main.c` 缩减为调用 `app_core_start()`；`main/CMakeLists.txt` 只注册该入口文件。 | 已证实 | main只app_main.c与app_core依赖；三LCD实际compile graph/构建及源码门禁，module-core-maintenance-20261006 |
| S1.7 | 创建 `app_console` message router、endpoint、request/reply、订阅和 lease API，并先于正常功能 endpoint 启动。 | 已证实 | app_console.h/app_message.h/router 源码与 fake endpoint 独立测试；先于 ATOM/Wi-Fi/Camera 启动 |
| S1.8 | 定义版本化 `app_message.h`，包含 Camera、Wi-Fi、Input、UI 和 System message contract。 | 已证实 | app_console.h/app_message.h/router 源码与 fake endpoint 独立测试；先于 ATOM/Wi-Fi/Camera 启动 |
| S1.9 | 创建各功能 component 的生命周期和 message endpoint。 | 已证实 | Core/System、Console/UART、Camera、UI、Input/provider、Wi-Fi bridge生命周期与endpoint均生产绑定；公共版本与停止/失败保留契约已补齐。实际graph/archive symbols及255host；见module-api-build-audit-20261006。实机切换另列V18/A31。 |
| S1.10 | 在每个 message contract 中注明线程安全、deadline、generation、payload/lease 所有权和错误语义。 | 已证实 | 公共头共享envelope规则+module-message-contracts逐46编号owner/payload/代际/lease/完成错误差异；Camera outbound attempt代与source endpoint寿命分开。enum/table46无漏项；逐域publisher/consumer源核对见module-message-contracts-20261006。文档要求完成不替代运行/硬件验收。 |
| S2.1 | 创建 `app_wifi` 接口，将通用配置类型、校验、客户端快照、异步 token 和 TCP channel 语义迁入该 component。 | 已证实 | app_wifi opaque object/config/snapshot/token/channel契约，wifi_esp32实现与两lane bridge已生产使用；Camera discovery/PTP仅typed network。module-wifi-tcp/camera-discovery/ptpip-client及最终实际图。 |
| S2.2 | 将 AP 驱动、NVS 和配置应用迁入 `wifi_esp32` 实现。 | 已证实 | wifi_esp32 AP/NVS/config jobs源；host64/64与三种构建；module-wifi-jobs记录 |
| S2.3 | 创建 `app_wifi_messages` endpoint，把发现/RSSI/TCP/config message 转发到 `app_wifi`，并移除 Wi-Fi 实现对相机和 UI 的包含。 | 已证实 | app_wifi_messages discovery/RSSI/TCP/config endpoint生产绑定；API版本/两lane优先队列/lease，Wi-Fi源码无Camera/UI依赖；边界与实际archive符号门禁通过。 |
| S2.4 | 将 `gamepad_input` 和来源仲裁迁入 `app_input`，只发布动作/状态 message。 | 已证实 | 真实input_service/owner/report registry+gamepad kernels归app_input，仅typed Camera/UI请求。input_reports/provider/owner/service/release及新增input_equivalence通过；模块边界/真实符号图通过。见module-input-contract-20261006；不代表实机动作效果 |
| S2.5 | 将 `atom_link` 迁入 `app_input_atom`，只通过 provider API 上报真实手柄 report。 | 已证实 | app_input_atom物理device/协议/monitor独占、Core life/stop、raw provider API；真实源码fake I2C测试/构建graph，module-input-providers-20261006 |
| S2.6 | 将 UART 手柄模拟迁入可选的 `app_input_sim`，串口命令只生成统一 report。 | 已证实 | Debug app_input_sim独立protocol/player/report、UART仅typed raw encoder；readonly lease/4jobs/8结果、Release无源/符号/库；module-input-providers-20261006 |
| S2.7 | 将 Wi-Fi 菜单和 UI 偏好迁入 `app_ui`，通过 message 请求 Wi-Fi/Camera，不直接依赖对应门面。 | 已证实 | UI菜单/偏好归app_ui，typed路由不依赖Wi-Fi/Camera门面；按S3.6最终Web-only要求已删除旧热点editor/prefs writer，不保留被新要求取代的正常编辑器。module-ui-private/preferences-readonly/wifi-readonly |
| S2.8 | 同步主机测试源文件路径，但不修改测试逻辑或减少测试项。 | 已证实 | 原54测试保留；搬迁协议支持归tests/support/legacy且保持原断言，新增production source fixture。最终255/255，未降低告警；各移动清单见module-sony-ptp-cleanup。 |
| S3.1 | 将端口 80 trigger、完整 Web、配置、OTA 和网页资源迁入独立 `app_maintenance`。 | 部分 | 独立Web/OTA/config/resources/trigger/factory已Core生产绑定，最终提前trigger与启动屏障实现；当前261host/三LCD通过。真实启动HTTP与完整维护切换另待硬件。 |
| S3.2 | 删除 PIN/auth/token/session 逻辑，并在 Web/文档中明确无认证策略。 | 已证实 | 生产PIN/auth/login/session/cookie及路由删除，README/当前user-guide/design明确无认证，历史记录和test-only maint_auth有范围说明；Web info authentication:none。真实客户端可达性另列V21/V22。 |
| S3.3 | 用 `app_maintenance_system_ops_t` 向 `app_core` 请求独占模式和重启，不包含 `app_core` 私有头。 | 已证实 | Core注入exclusive/reboot、真实ELF生产调用、独立维护对象无Core/功能引用，旧compat删除；module-core-maintenance-20261006 |
| S3.4 | 在 `app_core` 建立 STARTUP/NORMAL/ACTIVATING/MAINTENANCE 状态机和不可逆维护切换。 | 部分 | 已生产绑定真实mode/全部normal stop/fixed/config restart/activate，逐步失败仅restart主机证明；真实HTTP join/SMP/实机完整切换待验 |
| S3.5 | 在 `app_ui` 实现只显示 `MAINTENANCE` 的显示锁，拒绝其他 frame/model/benchmark。 | 部分 | Core已绑定固定画面；model永久清空/冻结、renderer排空、正常setters拒绝，host模型/像素验证。module-ui-clear-20261006；真实LCD/SMP待验 |
| S3.6 | 将恢复出厂、配置写入、OTA 和退出全部改为 Web 操作，并在完成后重启。 | 部分 | Web factory/config/settings/OTA/exit及成功后重启已接；普通UART/LCD/WiFi/prefs写路径删除，host回归。module-web-factory/preferences-readonly/wifi-readonly；真实保存/重启待验 |
| S3.7 | 删除手柄、UART、LCD 菜单维护入口以及维护对相机/输入/Console 的调用。 | 部分 | 旧controller、UART/手柄/LCD维护入口及旧menu/text已删；补删UI无生产调用的raw vendor property/status setter与9code表，原体只留fixtures，boundary禁止回归。module-ui-vendor-cleanup；其余dead API与最终实机仍在审计 |
| S3.8 | 保持现有配置 token、network generation、上传互斥和 OTA 失败回滚语义。 | 部分 | Web保留prepare/回执commit/cancel及generation跨换代token结果；OTA单gate/回滚已有回归，整体实机竞争待验证 |
| S4.1 | 创建 `camera_backend` 通用接口，只定义生命周期、能力、属性、取景和语义动作。 | 已证实 | 纯C语义契约/统一结果/版本能力/const ops/一次绑定/生命周期与buffer规则，真实camera_backend独立fake ops成功/错误测试及Sony具体ops生产绑定；具体实现见S4.4。 |
| S4.2 | 将 `ptpip` 整理为 `ptpip_client_t` 基类，用 Wi-Fi channel request/reply message 替代裸 socket fd，合并 session/transaction/deadline/cancel 状态并移除 lwIP/app_wifi 依赖。 | 已证实 | 生产唯一client/session/transaction/deadline/cancel与typed TCP；旧fd移tests支持，CMake无lwIP，85/85三ELF；module-sony-ptp-cleanup-20261006 |
| S4.3 | 将现有 `sony_camera` 的属性解析、取景校验、命令编码和实际使用枚举迁入 `camera_backend_sony`。 | 已证实 | 4源/5头迁Sony backend private，原内容哈希一致，原fixtures与生产ELF核对；module-sony-ptp-cleanup-20261006 |
| S4.4 | 在 `sony_camera_backend_t` 中内嵌 `ptpip_client_t`，实现 `camera_backend_ops_t`，禁止复制 PTP/IP 基础逻辑。 | 已证实 | Sony backend嵌唯一PTP实例，完整ops/private factory真实生产使用；backend/protocol/producer回归 |
| S4.5 | 删除 `camera_transport`、`camera_device`、两层 adapter、裸 fd Sony API 和无语义 wrapper。 | 部分 | 生产fd Sony/PTP API移tests支持；sony_client_controls七个纯转发已合并到backend直接encoder绑定；47static helper有源内引用，4未引用声明已清理；删除后真实smoke尚待完成 |
| S4.6 | 创建独立 `app_camera`，迁移身份、链接、设置状态机、动作队列和取景流水线。 | 已证实 | identity/link/kernels/actions/frame/runtime/endpoint独立app_camera并真实生产调用，module-camera-facade-20261006 |
| S4.7 | 按“纯状态逻辑 → backend 控制 → 发现 → 会话编排 → 主循环”的顺序拆分并迁移 `camera_controller.c`。 | 已证实 | Camera职责拆成runtime/discovery/session/properties/controls/actions/frames/stream/endpoint/identity，旧controller生产文件已删；fake generic producer可独立编译。module-sony-contract-audit。 |
| S4.8 | 每次只移动一个职责，保留临时兼容入口并执行完整回归；兼容入口在全部调用方迁移后删除。 | 已证实 | 正式批次逐职责记录/回归，生产兼容入口已随调用方迁移删除；legacy只用于原断言。最新255及五配置，module-api-build-audit。 |
| S4.9 | `app_camera.h` 只保留给 `app_core` 的生命周期/quiesce；运行时动作、设置、状态和诊断全部定义为 Camera message。 | 已证实 | app_camera public仅Core生命周期/版本，旧reservation/forget/quiesce-release接口已删，生产runtime请求经typed endpoint；public/private扫描和实际archive符号边检查通过，module-ui-private/module-symbol-owner |
| S4.10 | `app_camera` 发布 JPEG frame lease event，由 Console 转发给 UI；取景解码、画布获取、叠加和刷新全部由 `app_ui` 完成。 | 已证实 | app_camera frame lease→Console→app_ui decoder/surface/overlay；生产ELF及frame/UI/producer tests |
| S4.11 | 将 UI、输入和 UART 的相机访问迁移为 message；仅 `app_core` 直接调用 `app_camera.h`，维护应用不包含该头。 | 已证实 | UI/Input/UART只Camera messages，维护无Camera/Console依赖，旧Input/UART/维护compat删。source scanner与三archive symbol边无业务直接Camera调用；Core-only life实测编译 |
| S4.12 | 将 Sony backend factory 设为 `app_camera` 私有构建依赖，从 `app_core` 和其他 component 的依赖列表移除。 | 已证实 | factory仅app_camera private binding；Core无backend对象/依赖，main已无PTP/Sony/backend/privateCamera依赖 |
| S4.13 | 生成调用清单后删除无生产调用、无需求、无样本依赖的 PTP/Sony 代码，并记录每个删除项及替代路径。 | 部分 | 16文件移动清单/哈希与control合并清单；最终补移test-only exposure纯转发（原体相同/3fixture），39global/47static/55constant依据核对，三LCD与262host通过。module-sony-exposure-cleanup；真实相机smoke仍待验 |
| S5.1 | 将剩余客户端发现/RSSI、TCP channel、设置/菜单动作、JPEG frame/状态回调改为 typed message，删除直接调用路径。 | 已证实 | 当前discovery/RSSI→Wi-Fi endpoint，PTP→typed TCP，UI/Input/UART→Camera/UI requests，JPEG/STATE/PROPERTIES→Console events。逐路径源、实际archive边和独立fake endpoints验证，module-route-header-audit。 |
| S5.2 | 将 UART 行读取、命令注册、公共帮助和状态聚合迁入 `app_console` gateway。 | 已证实 | app_console_uart与正常encoder整体迁入Console，Core-start/quiesce、fake gateway/生命周期及五构建；module-uart-gateway-20261006 |
| S5.3 | 将相机、Wi-Fi、输入、UI、模拟、故障注入和显示基准命令改为 message encoder；删除维护串口命令与探针。 | 已证实 | 普通控制/状态/模拟/fault/bench只消息；maint串口/probe源码与dead维护手柄接口删除；Debug/Release gateway回归 |
| S5.4 | 删除 `app_diagnostics` component；显示基准实现保留在 `app_ui`，UART 只发送 request 并读取 reply。 | 已证实 | 未创建 app_diagnostics；基准归 app_ui，UART 只消息请求/查询/事件；host与三构建通过 |
| S5.5 | 各命令处理器只包含 message contract，不包含功能门面或私有头。 | 已证实 | 命令仅message/纯值/common/SDK；私有UART头不含功能API，Console无功能component依赖，boundary扫描通过 |
| S5.6 | 用 Kconfig 控制模拟、故障注入和基准命令是否编译，不移除生产 help/status/控制命令。 | 已证实 | Debug SIM/fault/bench 条件编译，Release source/ELF 无对应实现；生产 help/status/control 保留 |
| S5.7 | 为每个 component 使用 `REQUIRES app_console` 表达 message 依赖，并禁止跨功能直接 include。 | 已证实 | 正常功能依赖Console contract，全部显式public/private CMake边已核对；桥接器PRIVATE app_console是内部实现依赖，未虚增public依赖。三实际图91边与archive direct边通过。 |
| S5.8 | CI 增加直接调用旁路、lease 泄漏、私有头越界和依赖环检查。 | 已证实 | CI源边界/私有头门禁、router lease回归、ASAN/UBSAN/leak检测、实际component graph及archive direct-symbol门禁已接入；相关正反例本机通过。ci.yml与ci_build.py注册核对；远程CI未跑，规则不覆盖任意动态指针，见module-final-software-audit。 |
| S6.1 | 删除已无调用方的 `camera_pair.h` 兼容 API。 | 已证实 | 生产camera_pair.h已删除，Core仅public Camera lifecycle，身份清除仅维护系统ops。boundary与真实符号图通过。 |
| S6.2 | 删除旧路径转发头和重复命名。 | 已证实 | main/components/common全部headers去注释/include guards后无纯转发头、无重复basename；旧compat/controller/pair/transport/device生产路径已删，legacy仅测试支持。module-route-header-audit。 |
| S6.3 | 更新架构、开发、构建和测试文档。 | 部分 | 当前架构/依赖资源图、46message、README/user/开发文档已更新；旧硬件记录注明历史范围。9旧UART脚本移legacy保留哈希，当前bench/偏好只读/恢复断言及串口日志说明修正；11语法检查+5host通过，module-current-scripts。当前使用手册补查并修正旧u/手柄电量/循环边界/52测试/cJSON依赖，61精确路径58存在+3明确历史/未来。module-current-guides；真实脚本回放及其他完整效果仍待验。 |
| S6.4 | 使用 `git grep` 或等价检查确认无旧路径和私有头跨模块引用。 | 部分 | 生产源码private/旧头/退休入口由boundary及三实际compile/archive图检查通过；测试legacy有意保留旧协议支持，当前guide/tool/design精确61路径已逐分类，58存在+3明确未来/历史，module-current-guides；保留历史名称不声称当前生产存在。源门禁通过不替代实机。 |
| V1 | 画布重复 acquire、超时、cancel 后重取以及 refresh 后旧指针/旧 generation 失效； | 主机已验证 | test_display_surface真实surface+fake backend覆盖重复acquire、两处mutex超时、cancel重取、copy/stale lease不可提交/释放他人、refresh/fail/recover旧generation失效。硬件buffer同步另待验 |
| V2 | JPEG 解码失败不刷新半帧，刷新失败后禁止写入，恢复后只能取得新画布； | 部分 | surface真实host覆盖失败禁写/recover新generation；新增Debug/Release真实ui_jpeg_renderer+库/surface stub矩阵，部分fast/ROM decode/header/alloc失败不publish且cancel/reset/nextgood通过。真实decoder/硬件同步仍待验，module-jpeg-renderer |
| V3 | 双 buffer 扫描/绘制所有权不重叠，连续 LIVE/SETTINGS 切换不增加全屏拷贝； | 部分证实 | [renderer连续切换](../records/module-ui-switch-20261006.md)：真实renderer对fake双buffer连续32帧旧front整帧校验不变、workspace/decoder复用；源码直接写back并原地缩图，无新增全屏复制。真实RGB扫描/SMP待验 |
| V4 | `app_camera` 发布 JPEG message 后，在成功、坏帧、丢帧、超时、停止和显示失败路径中都准确回收 frame lease； | 部分 | host验证readonly/lastref/乱序result/坏帧/发送失败/lease池耗尽/fatal/stop等待；原生产已调用；硬件与最终维护quiesce待验证 |
| V5 | 真实 JPEG 与显示基准经过同一个 `app_ui` renderer，基准命令不直接取得画布； | 部分 | 源码bench与正常JPEG共用app_ui_show_jpeg；Debug直接真实test_jpeg生成失败归还canvas、不publish，并成功结果经同一decode/overlay/publish入口。Console无canvas API引用；真实库/性能待验 |
| V6 | 除 `app_ui` 和显示后端外，源码中不存在 `display_surface.h`、`board_7b.h` 或 `board_lcd.h` include； | 已证实 | check_module_boundaries实际生产source扫描仅UI/surface/backend许可canvas/hardware include，20nodes91edges及archive符号边无其他应用surface调用 |
| V7 | 同一组手柄 report 分别由 ATOM provider 和 UART 模拟 provider 上报时，产生完全相同的 `pad_action` 序列； | 主机已验证 | 真实provider registry→input_owner→input_reports→gamepad replay同一normalized timeline，ATOM/SIM共77动作type/value/generation逐项相等，含阈值/肩键/菜单/gap/offline。test_input_equivalence；物理协议端到端仍待实机 |
| V8 | ATOM 断开、gap、provider 重启、UART 模拟停止和来源切换都先产生完整释放； | 部分 | test_input_reports覆盖gap/切换release retry与held baseline；test_input_owner覆盖unregister/new handle/overflow/stale backlog，provider/SIM测试覆盖stop。253host通过；实际设备断线及串口/SMP时序待验 |
| V9 | 旧 source_epoch、重复 report_id 和来源切换后迟到的 report 被拒绝； | 主机已验证 | test_input_reports断言重复ID/旧epoch拒绝、ID倒退隔离、epoch advance恢复；owner断言非selected迟到报告不抢来源/旧handle无效。真实纯kernel和registry，不是协议序号模拟断言 |
| V10 | 关闭调试 Kconfig 的生产固件不链接 `app_input_sim` 及其串口命令； | 已证实 | 最终Release app_input_sim注册为空组件（IDF early requirements需要），无实际编译源/入口符号；LCD23、ATOM5禁符号全部不存在。不能声称metadata完全无SIM组件。 |
| V11 | 使用 fake 门面回归 `app_console` 的相机、Wi-Fi、输入、UI、模拟和基准命令，并确认不存在维护命令； | 主机已验证 | 实际UART Camera/Wi-Fi/UI/bench已有fake endpoints；新增真实I2C/SIM encoder+实际lease/parser覆盖选择拒绝/ENABLE失败/发送失败/槽耗尽/过期事件。gateway两配置拒绝maint/probe；最终260/260，module-route-header-audit。 |
| V12 | Console 停止、UART 读取失败和未知命令不改变业务任务生命周期或内部状态； | 主机已验证 | actual debug_console UART read失败/unknown command退休自身owner，driver失败保留重试；gateway quiesce仅UART endpoint，router独立存在。生产source/依赖无其他业务生命周期调用，console_lifecycle/gateway两配置通过。 |
| V13 | 验证 request/reply correlation、deadline、generation、迟到 reply、endpoint 重启和队列满处理； | 主机已验证 | 真实app_message_router fixture覆盖correlation/type/generation mismatch、deadline/late reply、epoch换代、queue满、cancel保留consumer lease及quiesce。最终255通过，真实SMP时序另验。 |
| V14 | 验证 control/bulk 双队列下 STOP/RELEASE/NETWORK_CHANGED 不被 JPEG 或 TCP bulk 阻塞； | 部分 | router与TCP worker fake回归已覆盖lease/close优先/排空；真实Camera slots→router→UI handler集成已验证control优先；Core STOP编排/实机待验证 |
| V15 | 验证 JPEG/TCP lease 零拷贝转发、引用计数、多订阅释放和 router quiesce 回收； | 部分 | router/TCP与生产JPEG新槽测试覆盖零拷贝/ref/partial fanout/最后归还；真实Camera→router→UI多订阅/queue-full/partial fanout/quiesce集成通过；Core编排/实机仍待验证 |
| V16 | 依赖扫描确认正常功能 component 只通过 `app_console.h`/`app_message.h` 跨域通信，Console 不依赖功能 component； | 已证实 | 正常跨域通过Console contract；生产private/include扫描、三真实component graph与archive direct edges通过，Console无功能component依赖。Core组合根及后台factory注入是计划规定例外。 |
| V17 | 在 STARTUP 首次访问 SoftAP:80 会进入维护；进入 NORMAL 后端口 80 trigger 关闭且无法原地进入维护； | 部分 | 独立HTTP首请求/拒绝/关闭/不可重开主机证据；Core真实mode及物理关闭绑定、实机未完成 |
| V18 | 维护切换确认输入完整释放、相机/JPEG 排空、Console 停止、正常 UI token 取消后才启用完整 Web； | 部分 | 真实Core/shutdown测试trace证明全owner stop→fixed→config→activate及每步失败只RESTART；owner级Input/Camera/UI/router各有实际源码fixture，但Core下层stub，不证明全部真实任务SMP组合。module-owner-evidence。 |
| V19 | 维护期间 LCD 像素输出只包含固定 `MAINTENANCE` 画面，JPEG、overlay、基准和状态提交均被拒绝； | 部分 | 实际render_stop/model/endpoint证明fixed画面与冻结/普通frame/fault/setter拒绝；字体stub仅1白像素/MAINTENANCE字符串，真实48px字形/LCD像素待设备。module-owner-evidence。 |
| V20 | `app_maintenance` 独立测试目标只链接 `app_wifi` 和 HTTP/OTA/JSON stub，不链接相机、输入、UI 或 Console； | 已证实 | 实际独立Web+trigger+maint_wifi+真实cJSON及OTA targets无Camera/Input/UI/Console/Core实现链接；factory只fake系统ops回调。source/CMake及53维护cases验证，不要求真实HTTP/NVS硬件在host链接。 |
| V21 | Web 无 PIN/token/cookie/auth 检查，连接 SoftAP 的客户端可直接完成配置、OTA、恢复出厂和重启； | 部分 | 实际Web源/路由/独立fixture无login/auth/PIN/session/cookie，直接settings/config/factory/reboot/OTA handlers回归。真实热点客户端/浏览器尚未验证，当前历史LCD/ATOM串口未出现。 |
| V22 | 端口 80 只绑定 SoftAP netif，STA/其他 netif 不可访问； | 部分 | 实际SDK HTTP全部TU注入wifi_esp32 AP bind adapter并链接；IPv4/IPv6 scope tests检查AP地址+SO_BINDTODEVICE和fail-closed，UDP仅loopback。真实STA/其他netif访问隔离待设备。 |
| V23 | 使用 fake Wi-Fi message endpoint 验证 UI、相机发现和 PTP/IP channel，不链接 `app_wifi`/lwIP； | 部分 | 当前Camera discovery与PTP client/protocol、Sony共享fake Console/Wi-Fi fixture目标无app_wifi/lwIP，真实源独立测试通过。最终Web-only要求删除UI Wi-Fi编辑器，当前UI信息menu已独立验证；历史UI fake Wi-Fi编辑器仅test-only保留，不冒充当前实现。见module-final-software-audit。 |
| V24 | 使用 fake `app_wifi` 回归 message bridge 与维护 Web 的配置 prepare/commit/cancel； | 部分 | 当前bridge用真实facade+fake driver验证STATUS/GET/发现及退休PREPARE/COMMIT/CANCEL/RESULT均NOT_SUPPORTED；独立Web以fake app_wifi验prepare/ACK/commit/cancel失败回滚。9相关host通过。原验证条目bridge writer被最终S3.6 Web-only要求取代，见module-final-software-audit；真实维护保存仍待验。 |
| V25 | 网络 generation 变化后旧 PTP/IP channel 立即失效，并且不会被新会话复用； | 主机已验证 | 真实PTP network_changed立即取消旧通道，open期间换代不保存旧token；bridge旧generation/owner拒绝、late reply归还，facade old channel STALE。ptpip_client/wifi_channel_messages/app_wifi_channels；实际网络时延/SMP另验。 |
| V26 | 检查除 `wifi_esp32` 外不存在 `esp_wifi.h`、`esp_netif.h`、`lwip/sockets.h` 或裸 socket fd； | 已证实 | 当前components/main生产源扫描esp_wifi/esp_netif/lwip sockets仅wifi_esp32；维护直接lwIP依赖与shutdown已删，PTP/Sony/正常应用无裸socket API。维护SDK session标识只传SDK关闭，不作为app_wifi fd。module-http-stop。 |
| V27 | 使用 fake `camera_backend` 回归 `app_camera` 的重试、停止、动作、属性和取景编排； | 部分 | 真实runtime+fake backend测试retry/cleanup/stop/actions/property/frame/配对/网络变更；完整final行为与附加交互待验收 |
| V28 | 使用 fake Camera endpoint 分别构建 UI、输入和 UART gateway 测试，不链接 `app_camera`/backend/PTP/Sony component； | 已证实 | 十个host实际link.txt包含UI/Input/UART/discovery独立目标，无Camera/backend/PTP/Sony/app_wifi实现库；fixtures编译真实被测源并fake消息端点，相关21/21通过。module-route-header-audit。 |
| V29 | 校验 FRAME/STATE 订阅、endpoint 停止、message 释放和超时回收，不发生重复释放或槽泄漏； | 部分 | actual router lease ref/fanout/expiry/late reply/endpoint epoch/stop/quiesce，加Camera slots与UI result排队/关闭归还矩阵；主机各owner无泄漏，真实Camera frame slots+router+UI handler的协作式host集成通过（module-frame-bus）；真实并发/SMP与state完整集成尚未证明。 |
| V30 | 分别对 `wifi_esp32`、`ptpip_client_t` 基类和 `camera_backend_sony` 执行契约测试； | 部分 | 新增真实wifi_esp32 factory+facade（SDK/jobs/TCP/storage边界fake）验证九partial init/lock/event/stop保留重试；PTP/Sony共享fake Console已有契约。完整263通过，module-wifi-backend-contract；真实radio/DHCP/Flash/启动与SDKcleanup失败另待硬件，不冒称full SDK验证 |
| V31 | 用 fake Console/Wi-Fi endpoint 独立测试 PTP/IP 基类，再用同一 fixture 测试 Sony backend 扩展，确认没有第二套 transaction/session； | 已验证 | PTP与Sony backend在同fake Console fixture使用同一真实PTP client/wire/lease；初始化2..10→vendor/标准11..17及健康CloseSession共享事务；无app_wifi/lwIP/legacy fd链接 |
| V32 | 检查除 `app_camera`、Sony backend、PTP/IP 基类和契约测试外不存在 `camera_backend.h`、`ptpip_*`、`ptp_*` 或 Sony 私有 include； | 已证实 | backend/PTP/Sony header include仅Camera私有/generic backend自身/Sony/PTP内部；通用boundary basename规则已拒绝其他功能component越界，生产扫描通过。tests独立fixture明确例外。 |
| V33 | 删除清单中的每个 PTP/Sony 符号均无生产调用、需求引用和 fixture 依赖； | 部分 | 当前39生产global/37ELF+2parser fixture、47static helpers/55constant依据；7client wrapper合并及exposure wrapper移test-only，原assert/字节fixture保留，4无引用常量已删。移出生产有测试依据者未毁掉fixture。真实协议删除/合并smoke仍待验，见module-sony-exposure-cleanup。 |
| V34 | 停止取景并排空解码任务； | 部分 | 真实Camera producer停止等待frame slots/leases/backendcleanup；UI endpoint joins/result flush与renderer user drain各有fixture。Core组合顺序host通过，真正在线取景/decoder/设备排空仍待实测。 |
| V35 | `app_core` 独占切换取得/释放相机系统租约，维护应用不调用租约 API； | 部分 | Core mode独占claim→关闭Camera admission→physical stop/drain→retire endpoint；维护无Camera lease/API。计划最终不可逆MAINT要求不恢复正常，因此旧可逆系统租约release已删除；原条目“取得/释放”与最终不可逆流程的语义差异需验收时明确，不声称原可逆lease行为仍存在。 |
| V36 | Wi-Fi 仅显示配置与需要重启的配置； | 部分 | 真实UI menu对Input/UART的Wi-Fi CONFIRM/STEP/BACK/RELEASE均HANDLED且不开editor；普通配置写拒绝、Web ACK→commit→完成后重启已有host。旧wifi_menu_current实际legacy，不作当前实现证据。见module-remaining-ui-input-20261006；真实AP保存/重启待验 |
| V37 | factory wifi / factory all 的成功、失败和超时； | 主机已验证 | 真实Core factory wifi/all成功、freeze/read/write/identity/preferences/rollback失败与freeze timeout，加实际Web confirm/ACK丢失/reboot reservation等通过。NVS/flash stub，实机保存/复位另验。 |
| V38 | ATOM 断线、gap、来源切换和所有动作释放； | 部分 | 真实owner/provider/reports/gamepad等价fixture增加gap/offline先RELEASE_ALL与MF_CANCEL、gap窗口无新动作断言；reports/owner/service另验切换与release/MF失败重试。全部262通过，见module-remaining-ui-input-20261006。fake下游不能证明真实Sony动作释放或实体ATOM断线时序 |
| V39 | OTA 上传期间关闭竞争及 health 内部栈重启； | 部分 | 实际OTA失败/重入/shutdown/丢ACK回归与Core内部health归属；真实关闭竞争/flash/硬件重启仍待验 |
| V40 | LIVE/SETTINGS 切换、首帧、坏帧和显示恢复； | 部分证实 | [renderer故障矩阵](../records/module-jpeg-renderer-20261006.md)与[连续切换](../records/module-ui-switch-20261006.md)覆盖主机首帧、32次切换、坏帧后good及publish失败恢复；SDK codec/真实显示与相机待验 |
| V41 | 应用镜像小于现有 5 MiB 门禁。 | 已证实 | 最终Default0x358780/Stable0x357970/Release0x34c770均小于5MiB；本轮五配置构建终态0，module-api-build-audit。 |
| A1 | `main/` 只保留 `app_main.c` 应用入口，组合根位于 `app_core`，业务代码位于职责明确的 components。 | 已证实 | main只app_main.c应用源与Core依赖；组合根/业务components当前源码与三实际compile graph一致，boundary通过。 |
| A2 | component 依赖图无环，私有头不被跨模块包含。 | 部分 | LCD三实际图20项目/绑定节点91显式边无环，含post-project HTTPedge；private头/source边界通过。module-dependency-graph；全部动态旁路及private审计仍待完成 |
| A3 | `app_ui` 是唯一应用层显示入口；相机、维护、`app_console` 和 `app_core` 均不得直接依赖 `display_surface`。 | 已证实 | check_module_boundaries.py；main/其他应用不存在 board/display_surface include |
| A4 | 除 `app_ui`、显示后端及后端测试外不得包含 `display_surface.h`、`board_7b.h` 或 `board_lcd.h`。 | 已证实 | check_module_boundaries.py；main/其他应用不存在 board/display_surface include |
| A5 | `display_surface` 统一维护画布 buffer、写租约、刷新代数和恢复状态；不存在绕过抽象层直接取得或发布帧缓冲的路径。 | 部分 | 写租约/代数/恢复和私有后端已建立；完整生产消息与恢复矩阵待集成回归 |
| A6 | 刷新接口在成功、失败和取消路径都能释放所有权，不发布部分 JPEG 或未完成 UI 帧。 | 部分 | 真实surface消费refresh/cancel lease，新增实际ui_jpeg_renderer控制流验证坏header/fast/ROM部分失败不publish，后续good与publish-fail/recovery。实际codec/RGB同步及SMP待硬件，module-jpeg-renderer |
| A7 | JPEG 解码画布和显示基准均由 `app_ui` 管理，并复用同一 renderer 与刷新路径。 | 部分 | 源码/host证明共用 app_ui JPEG renderer/画布；三构建通过，实机性能/稳定性待验证 |
| A8 | 真实手柄上报和 UART 模拟都只能通过 `app_input` provider API 进入系统，不存在直接修改输入状态或直接调用业务动作的旁路。 | 已证实 | 物理ATOM与独立SIM只publish copied report，UART raw message交SIM endpoint；source边界与3实际符号图无业务门面引用。input_provider/owner/service/provider tests通过；Core编排已接 |
| A9 | `app_input` 统一完成来源仲裁、去重、按键边沿、扳机迟滞、重复和安全释放；具体 provider 不包含业务按键映射。 | 已证实 | 状态/仲裁/去重/迟滞/边沿/repeat/release仅app_input私有kernel/sole owner；provider public头与源码无Camera/UI业务实现依赖。真实kernel/registry/service测试与边界门禁通过 |
| A10 | ATOM 与 UART 模拟对等 report 的动作结果一致，来源切换不会遗留按下状态。 | 部分 | 新增test_input_equivalence从两种source provider API生成77字段完全一致动作；reports/owner证明switch必须release且新held不press。端到端物理按键/串口时序尚未验证 |
| A11 | `app_diagnostics` component 已删除；正常应用的模拟、基准、故障注入和状态命令统一归入 `app_console`，维护探针/串口命令已删除。 | 已证实 | 无app_diagnostics生产组件；UART SIM/fault/bench encoder归Console，bench实现UI，维护UART/probe入口删除。源码门禁及最终Release禁止符号通过。 |
| A12 | `app_console` 是正常应用核心消息总线；UART gateway 与功能 endpoints 都使用同一 typed message contract。 | 已证实 | 正常UART和功能endpoint共用app_message.h与Console router/request/event/lease；独立fake encoder/endpoint/actual router回归，生产graph/direct symbols支持。 |
| A13 | 依赖方向为 `功能 component → app_console message API`，Console router 不依赖任何功能 component。 | 已证实 | Console仅common/SDK基础依赖，functional components依赖Console；三真实compile图与archive direct symbols通过。I2C纯formatter已从ATOM移common且补齐ATOM构建源。 |
| A14 | 客户端发现/RSSI、TCP channel、设置/菜单动作和 JPEG frame/状态订阅不存在直接函数调用旁路。 | 已证实 | 四类指定交互逐条source追踪为消息路径；actual component/archive边无跨域业务门面，间接ops属Core composition/backend内部合法边界。module-route-header-audit，硬件时序不在此源码要求内。 |
| A15 | JPEG/TCP payload 通过 lease 零拷贝转发，控制消息具有独立队列和更高处理优先级。 | 部分 | 真实frame→router→UI与TCP lane借用lease/pointer测试和双队列源证明零拷贝、control优先；Core/实机调度压力仍待验，见module-frame-bus |
| A16 | `app_wifi` 与 `camera_backend` 分别形成网络和相机后端边界，调用方不依赖具体实现。 | 已证实 | Wi-Fi opaque facade及Camera generic version/cap/const ops隔离具体backend；Core只选择网络factory，Camera私有选择Sony。fake facade/backend可独立编译并回归。 |
| A17 | `app_wifi` 是网络底层接口；正常应用通过 `app_wifi_messages` 访问，只有独立维护应用直接调用它。 | 已证实 | 普通UI/Input/Camera/UART只使用Wi-Fi messages；bridge消费底层app_wifi，独立Web直接调用。Core仅factory/lifecycle/boot/stopped-store系统编排例外，生产source和archive边检查通过。 |
| A18 | `ptpip` 只使用 Wi-Fi channel message，维护 Web 直接使用隔离的 `app_wifi` API，两者均不存在裸 socket/lwIP 旁路。 | 已证实 | PTP只typed Wi-Fi channel；维护仅app_wifi/SDK HTTP API，HTTP stop移除lwIP/raw shutdown，SDK queued session-close与atomic admission gate。boundary/三实际图/真实Web53cases通过；网络隔离/时延另列硬件。module-http-stop。 |
| A19 | 替换 `wifi_esp32` 时无需修改 `ptpip`、Web、UI 或相机模块。 | 已证实 | wifi_esp32实现由Core boot选择，app_wifi driver ops隔离；PTP/Web/UI/Camera均无具体factory/implementation头或符号，fake driver/channel/bridge tests通过。第二真实实现硬件另列A24。 |
| A20 | `app_camera.h` 仅供 `app_core` 生命周期编排；UI、输入和 UART 使用 Camera message，维护应用不连接 Camera endpoint。 | 已证实 | Camera public仅Core生命周期/query；实际组件符号允许Core到public life、禁止非Core调用，源码private include检查通过；维护对象仅Wi-Fi/系统ops无Camera endpoint |
| A21 | 配对、连接、动作、设置、能力、状态、JPEG frame 和诊断均由 Camera endpoint 的 request/event contract 提供。 | 已证实 | Camera endpoint提供start/pair/stop/action/menu/caps/status/debug/display，outputs提供STATE/PROPERTIES/COMMAND_STATUS与frame lease；public API仅Core lifecycle。endpoint/producer/outputs/frame与fake功能端点回归。 |
| A22 | `camera_backend`、PTP/IP 基类和 Sony backend 仅为 `app_camera` 私有依赖；`app_core` 不创建、不保存也不调用这些对象。 | 已证实 | app_camera CMake PRIV_REQUIRES camera_backend/Sony，private binding唯一factory；Sony private依赖ptpip。Core CMake和source无backend/PTP对象，真实graph public/private门禁通过 |
| A23 | `app_camera` 可在 fake backend 下完整构建和测试，不链接 PTP/IP 或 Sony backend。 | 已证实 | test_camera_producer/debug直接编译runtime/stream/session/controls等并提供fake generic backend，无Sony/PTP链接；配对/retry/control/properties/frame/stop/partial init覆盖。当前相关9条通过 |
| A24 | `wifi_esp32` 和 Sony backend 可分别替换；新的 PTP/IP 厂商 backend 复用同一个 `ptpip_client_t` 基类。 | 部分 | generic factory+ops及fake backend验证Camera可脱离Sony；Sony只内嵌同PTP client，wifi_esp32由隔离facade注册。尚无第二厂商真实backend/第二真实网络实现验收，不扩大替换证明 |
| A25 | Sony backend 组合内嵌 PTP/IP 基类，连接、session、transaction、deadline 和取消状态均只有一份。 | 已证实 | sony_backend_t只内嵌一个ptpip_client_t；session/next_transaction/deadline/nested scopes/cancel/network/channel tokens都归此实例。真实Sony+PTP同fixture交易2..17/CloseSession共享且failure cleanup retained，无生产旧fd全局owner |
| A26 | 独立 `sony_camera`、`camera_transport*`、`camera_device*` 和纯转发 wrapper 已删除，保留代码均有调用方或测试依据。 | 部分 | 独立Sony注册与生产fdAPI已移除；client控制转发层已合并；test-only exposure纯转发已移出；依据清单当前39global/47static/55constant与生产ops路径，262host与三LCD/archive通过，module-sony-exposure-cleanup；真实smoke待完成 |
| A27 | `camera_controller.c` 被拆为服务、运行时、发现、会话、属性和控制模块。 | 已证实 | 当前Camera独立runtime/discovery/session/properties/controls/actions/stream/frames/endpoint/identity等源编译，旧camera_controller单体生产路径已删；private API/compile_commands支持，非仅目录命名 |
| A28 | 相机 transport channel/事务、显示缓冲、Wi-Fi/NVS 写入分别保持单一所有者。 | 部分 | 实际session/PTP/lane/active canvas/storage mutex及namespace phase owner已逐源核对；LCD13直接NVS写只在3primitive，新门禁正反例/full262通过，module-storage-owner。真实SMP/Flash及完整维护交接时序待硬件。 |
| A29 | `app_maintenance` 可脱离相机、输入、UI 和 Console 独立构建，只通过 `app_wifi` 与注入的系统回调运行。 | 已证实 | maintenance CMake无Camera/Input/UI/Console/Core依赖；Web/OTA/trigger独立host通过，仅Wi-Fi/系统ops及SDK stub，实际archive symbols无正常功能引用。 |
| A30 | 维护只能在启动界面访问 SoftAP:80 触发；进入正常模式后 trigger 关闭，进入维护后只能通过 Web 操作并以重启退出。 | 部分 | Core/HTTP真实源与fake启动claim/boot barrier/NORMAL close/reject/不可重开证明不可逆Web-only切换；热点实机尚缺，module-owner-evidence。 |
| A31 | 维护期间相机、输入、Console 和正常 UI 全部停止，LCD 只显示 `MAINTENANCE`。 | 部分 | 真实Core顺序与逐owner停止/固定模型验证，但下层独立fixtures非SMP整体；fixed字体是stub，真实LCD与所有tasks停止需实机验证。 |
| A32 | 维护 Web 明确无认证，不存在 PIN/token/session/cookie；任意热点客户端拥有全部维护权限。 | 已证实 | 生产auth/PIN/login/session/cookie源/路由删除；Web info authentication:none、页面权限说明、handler无认证检查，实际独立Web全操作主机回归。热点可达性/隔离另列V21/V22。 |
| A33 | 所有现有主机测试保留且通过；不得通过删除测试或降低告警级别完成迁移。 | 已证实 | 原54保留，当前261/261全回归；新增真实UART Input编码器覆盖，无减少原assert/告警。legacy不入固件。module-http-stop。 |
| A34 | LCD debug/release 固件均构建通过，应用大小满足门禁。 | 已证实 | 最新LCD D3586c0/S3578a0/R34c690构建终态0且<5MiB；ATOM D1043f0/R1017f0上一批终态0且源未改。Release禁符号验证见两批记录。 |
| A35 | 启动顺序调整为基础 UI/`app_wifi` → 启动界面与 :80 trigger → 正常输入/Console/相机；ATOM 与 Wi-Fi 的硬件安全时序必须在实现阶段重新验证，health 任务仍使用内部 RAM 栈。 | 部分 | 实际UI→I2C prepare→AP/trigger→普通router/Input/bridge/providers/Camera，boot barrier及241host证明；health内部栈保持。module-startup-order/startup-failure；I2C/AP实机安全时序待验 |
| A36 | PTP/I²C 协议保持不变；维护 HTTP 移除认证并改为启动页触发，配置存储若变化必须使用版本化迁移。 | 部分 | HEAD对照14文件13一致/1同布局typedef，Wi-Fi codec命名映射后相同；UI prefs缺schema读取legacy1、unknown保留/load拒绝。当前26协议/存储host通过；HTTP/Core无认证启动触发源码及host已证。新Sony/I²C与启动实机待验，module-protocol-storage。 |
| A37 | 编译期依赖图和运行时功能模块关联图与最终代码一致，不存在图外的跨模块调用旁路。 | 部分 | 实际compile graph由IDF metadata+compile_commands+target link property生成，runtime图按源码review，三配置及10 verifiercases通过。module-symbol-owner 已核对 Default/Stable 41、Release 37 条直接符号边；间接factory/visitor/cancel/lease/display/Core注入已逐绑定核对并补运行图反向边，见module-indirect-callbacks-20261006；SDK任意动态路径与真实SMP待验 |
| A38 | 架构文档、项目结构和测试路径与最终代码一致。 | 部分 | 当前架构/资源/依赖/46message与testing262、README/user同步；current工具旧维护/偏好脚本归档，bench/NORMAL和恢复日志对齐，11语法+5host证明对应范围。全文件例子语义及实机回放尚待收尾，历史记录不作当前指南。 |
