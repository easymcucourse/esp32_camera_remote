# 测试与持续集成设计

本文汇总项目的测试分层、现有测试、计划新增的测试、实机与故障注入测试、长时间稳定性测试以及 CI 方案。各模块的具体测试用例写在对应设计文档中，本文只做汇总和约定。

> 当前263项CTest通过（2026-10-06，module-wifi-backend-contract批次），原54保留；真实注册以tests/host/CMakeLists.txt和ctest清单为准。CI已接入但远端Actions尚未执行。以下日期段落只代表当时实现/构建/硬件状态。

## 1. 测试分层

| 层级 | 运行环境 | 目的 | 现状 |
| --- | --- | --- | --- |
| 主机单元测试 | PC，gcc / clang、Python 3 | 解析、编码、状态机、脚本工具 | 当前263项（原54保留；最新记录见module-wifi-backend-contract） |
| 主机工具验证 | PC | 字体渲染、界面截图、抓包样本解析 | `font_preview_host` |
| 编译检查 | CI，ESP-IDF 5.5.1 | 两个工程开发 / 关闭模拟构建 | 四项矩阵已配置，远端待验证 |
| 实机功能测试 | 开发板 + 相机 + 手柄 | 按需求文档的验收测试逐项确认 | 手工 |
| 故障注入测试 | 实机 | 断电、断网、异常数据、资源耗尽 | 已测离线 LCD 恢复与 ATOM I²C CRC / 丢响应 / 延时，其余待验收 |
| 稳定性测试 | 实机，≥ 30 分钟 | 帧率、延迟、内存、重连次数 | 未系统进行 |

## 2. 主机单元测试

### 约定

- 纯 C11，只用 `assert` 和标准库，不引入测试框架；与现有 `m5_atom_matrix/tests` 风格一致。
- 新增纯逻辑模块尽量不依赖 FreeRTOS / lwIP；时间由调用方传入。现有 transport、NVS 和发送适配回归使用 `network_stubs`、`identity_stubs`、`tx_stubs`、`display_stubs` 与日志替身，不等同于硬件驱动测试。
- 编译选项：`-std=c11 -Wall -Wextra -Werror`；CI 中另加 `-fsanitize=address,undefined`。
- CTest条目可以是一个C可执行用例场景或Python测试容器，不能将条目数等同于程序/内部断言数。返回0表示该条目通过。

### 当前注册测试

统一执行命令见 [编译与烧录](build-and-flash.md#主机测试)。下表是原54基线名称，部分退休实现已归tests/support/legacy且不参与固件；旧auth/menu/factory/fd fixtures证明历史回归，不代表当前生产接口。新增当前覆盖见后表。

| 测试 | 数量 | 覆盖 |
| --- | ---: | --- |
| `ota_health`、`maint_notice`、`ble_advertisement` | 3 | OTA 六十秒边界 / AP 缺失 / 堆失败、维护提示到期 / 回绕、BLE 广播截断 / 原子拒绝 / 扫描响应合并 |
| `image_stride` | 1 | RGB565 原地行距展开、重叠与不重叠搬移、零尺寸 / 容量 / 溢出拒绝及前后守卫 |
| `atom_fault` | 1 | 故障计数有限递减、drop 优先 / 保留 CRC 次数、持续延时及关闭 |
| `i2c_monitor` | 1 | 分类、变化过滤、失败保留、缓冲 / 溢出及统计重置 |
| `pad_cmd`、`pad_player` | 2 | 手柄别名 / 组合 / 序列、范围及原子拒绝；动作绝对截止、队列、取消释放、计时回绕 / 迟唤醒 |
| `maint_auth` | 1 | PIN 拒绝采样、5 次失败 / 60 秒锁定、回绕、单会话 / token 失效及解析 |
| `maint_confirm` | 1 | 三秒双确认、边界到期、导航 / gap 代数取消、计时回绕 |
| `restart_schedule` | 1 | 预留 / 取消、响应后提交、重复拒绝、截止时间与计时回绕 |
| `ota_header` | 1 | 芯片 / 项目 / 大小 / 描述 / SHA 标志 / 段边界及编译时间比较；不替代完整镜像校验 |
| `maint_json`、`maint_wifi` | 2 | 平面 JSON / 嵌入 NUL / 深度限制、部分热点字段 / 严格整数信道 / 原子拒绝 |
| `doc_links` | 1 | 文件 / 标题 / 编码 / 围栏与非零失败退出 |
| `debug_line`、`uart_script` | 2 | CRLF / 退格 / 超长及非法行丢弃、请求号溢出；新鲜 ACK、异步 token、跨端等待、分段日志 |
| `sony_liveview`、`board_lcd`、`liveview_pipeline` | 3 | JPEG 段 / 扫描标记边界、单帧丢弃 / 连续损坏、LCD 缓冲所有权 / 旧回调 / 限次恢复、致命错误排空 |
| `factory_reset`、`wifi_menu`、`wifi_config`、`wifi_apply`、`debug_args` | 5 | 维护占用与重置失败 / 回滚、热点草稿 / 校验 / 编解码 / 应用、行参数解析 |
| `atom_slave_tx`、`atom_protocol`、`ds4_events`、`ds4_report`、`matrix_model` | 5 | 发送缓冲替换、CRC / 重同步 / 主机状态、事件缓存 / HID、灯阵模型 |
| `gamepad_input`、`camera_actions`、`setting_control`、`camera_menu` | 4 | 输入 / 释放、动作代数、目标合并、七项参数控制 |
| `camera_parsers`、`focus_input`、`focus_caps`、`sony_descriptors`、`sony_write`、`camera_settings` | 6 | 数据集、旧输入模块、能力 / 描述解析、写入线格式、扩展参数格式 |
| `props_193841`、`props_195145`、`props_200352`、`props_200950` | 4 | 同一个 `test_property_fixtures` 可执行文件处理四份裁剪快照 |
| `camera_link`、`ptp_session`、`ptpip_transport`、`camera_identity` | 4 | 发现 / 退避、事务 / 传输取消、身份记录 |

`focus_input` 是旧模块回归，当前 LCD 固件改用 `gamepad_input`；单元测试仍保留。全部测试通过不证明实机时延、相机效果、真实 Flash / FIFO 或灯阵视觉效果。

### 当前拆分覆盖与限制

| 领域 | 真实被测实现/边界 | 证据限制 |
| --- | --- | --- |
| Console | router/request/reply/cancel/deadline/lease/endpoint、UART reader/gateway/typed encoders | fake scheduler不证明SMP，encoder fake端点不证明业务效果 |
| Input | provider registry/owner/reports/gamepad/service、物理与SIM独立provider | equivalence从public report API比较77动作，不包含实际两端协议/无线输入 |
| Wi-Fi | facade/backend jobs/saved record/channel、message桥/TCP lane | 后端read自动repair仍写NVS；Core no-writer测试只启动编排，非Flash证明 |
| Camera/PTP/Sony | generic backend契约、唯一PTP实例同fixture、session/discovery/control/producer/endpoint/JPEG槽 | 原fd/reservation辅助在legacy；fake backend不证明相机响应 |
| UI/display | surface/board、model/messages/frame lease、endpoint/bench/renderer停止/fixed文本 | 新增Debug/Release真实ui_jpeg_renderer控制流矩阵，decode库与surface仍stub，不证明真实像素/同步 |
| Core/maintenance | mode/boot barrier/order/failure cleanup/stop、独立Web+真实cJSON/OTA/store | HTTP/NVS fake不证明网络隔离/Flash/cache-off；SDK HTTP同步stop无严格项目join预算 |
| 构建/边界 | public/private扫描、真实component graph、静态库直接symbol edges、Release禁符号 | callback/ops间接路径须源码核对，remote CI及硬件未跑 |

最新host261全回归见 [帧总线集成](../records/module-frame-bus-20261006.md)；[Sony控制层合并](../records/module-sony-control-merge-20261006.md) 为260全回归及三LCD构建；ATOM两配置见 [五配置核对](../records/module-api-build-audit-20261006.md)；各批次源/命令和范围在 [记录目录](../records/README.md)。不使用主机绿灯代替硬件验收，也不将源码注释当通过证据。

### 后续测试计划

已存在的 `ptp_session`、`atom_protocol`、`gamepad_input`、`wifi_config` 不再列为待新增；其故障注入及硬件覆盖仍需扩展。

| 测试 | 所在目录 | 设计文档 |
| --- | --- | --- |
| `test_ptpip_packet`、`test_ptp_dataset`、`test_sony_props`、`test_sony_format`、`test_ui_presenter` | `tests/host/` | [Sony PTP/IP 客户端分层设计](../design/sony-ptpip-design.md#12-测试) |
| `test_matrix_status`（灯阵渲染） | `m5_atom_matrix/tests/` | [Matrix LED 状态显示设计](../design/matrix-led-design.md#主机渲染测试) |
| `test_gimbal_control` | `m5_atom_matrix/tests/` | [BLE 云台控制设计](../design/gimbal-design.md#7-测试) |
| OTA/trigger/Web/factory当前fixtures | `tests/host/` | [维护页面设计](../design/maintenance-design.md#7-验证范围)；原auth仅legacy保留，不应重新实现 |

### 新增回归（2026-10-02）

wifi_config 覆盖字段边界、100 字节记录、损坏记录及随机密码；wifi_apply 覆盖保存失败、显示开关、网络重启及回滚；debug_args 覆盖引号、转义、空参数和错误输入。主机 26 项全部通过，实机结果单独记录。

### 测试样本

- 当前提交的是四份整数标量属性裁剪样本，来源和 stream / transaction 见 [样本说明](../../tests/host/fixtures/README.md)；提取入口为 `tools/extract_property_sample.py`，裁剪排除设备标识、字符串和网络包。
- `tools/extract_liveview_sample.py` 仍是依赖本地固定抓包的历史 JPEG 复现脚本，当前 fixtures 没有 JPEG。
- 后续提交含设备身份的新样本时需脱敏；原始 `.pcapng` 不提交。

## 3. 实机功能测试

以各需求文档的“验收测试”一节为准：

- [Sony 相机连接需求](../request/sony-ptpip-request.md#验收测试)
- [界面显示方案](../request/ui-request.md#验收测试)
- [手柄控制方案](../request/gamepad-request.md#验收测试)
- [Matrix LED 状态显示需求](../request/matrix-led-request.md#实机测试)
- [Wi-Fi 热点需求](../request/wifi-ap-request.md#验收测试)
- [维护页面需求](../request/maintenance-request.md#验收测试)
- [BLE 云台控制需求](../request/gimbal-request.md#验收测试)

每次发布前至少执行：冷启动配对、连续取景 5 分钟、`s` / `j` 停止恢复、Start、X/Y、L1/R1、LT/RT、ATOM 拔插。

## 4. 故障注入

| 类别 | 方法 | 期望 |
| --- | --- | --- |
| 相机断电 / 重启 | 取景中关闭相机 | 返回连接页，显示原因，按退避重连 |
| Wi-Fi 断开 | 相机关闭 Wi-Fi；远离热点 | 同上 |
| TCP 异常 | PC 端模拟相机（回放抓包并注入截断、超长、错误事务号、连接复位） | 不崩溃、不越界，断开重连 |
| 事件积压 | 模拟相机连续发送事件 | 每帧读取上限生效，取景不被饿死 |
| PSRAM 分配失败 | 调试构建中用钩子让 `heap_caps_malloc` 第 N 次失败 | 记录日志并重试，不复位 |
| LCD 帧同步超时 | 调试构建中屏蔽帧完成回调 | 执行恢复流程（代码与主机故障注入通过；实机待验收） |
| I²C 错误 | 拔插 SDA / SCL；注入错位字节 | 3 次失败判定断开，恢复后自动重连，释放 S1/S2 |
| ATOM 事件溢出 | 调试构建中缩小缓存容量并快速按键 | 灯阵显示溢出，LCD 不误触发 |

PC 端模拟相机计划作为 `tools/` 下的新脚本，读取导出的样本并按需篡改。I²C 故障、ATOM 断开、事件溢出和手柄动作优先通过 [UART 调试控制台](../request/uart-debug-request.md) 注入，并用脚本回放，替代手工拔线和按键。

## 5. 稳定性测试

连续运行不少于 30 分钟，期间每分钟操作一次手柄，用 `tools/serial_log.py` 同时记录 LCD 和 ATOM 日志。记录指标：

| 指标 | 来源 | 通过标准（暂定） |
| --- | --- | --- |
| 最低 FPS | `LIVEVIEW ... fps=` 日志 | 全屏 ≥ 3.0 |
| 按键延迟 P95 | 手柄按下到界面变化，高速摄像或日志时间戳 | ≤ 150 ms |
| 重连 / 断连次数 | `Live-view disconnected` 日志 | 0 |
| 最低内部 RAM / PSRAM | 主循环 10 秒日志 | 无持续下降 |
| 解码任务栈余量 | `stack_free=` 日志 | ≥ 4KiB |
| I²C 事务失败率 | `atom_link` 日志 | ≤ 0.1% |

日志统计脚本计划加入 `tools/`，输入日志文件，输出上表。

## 6. CI

配置见 [GitHub Actions](../../.github/workflows/ci.yml)，推送、合并请求或手动触发。所有作业只读仓库权限；同一分支的新运行取消旧运行。

| 作业 | 环境 | 步骤 |
| --- | --- | --- |
| `host` | Ubuntu 24.04，Clang / Python | 离线文档链接检查、全部 CTest；Debug 保留 assert，启用 ASan / UBSan 与失败退出 |
| `firmware` | `espressif/idf:v5.5.1`，四项矩阵 | LCD / ATOM × debug / release；使用独立 sdkconfig，关闭模拟的版本检查模拟符号未链接 |

[构建入口](../../tools/ci_build.py) 使用工程 sdkconfig.defaults 与 tools/ci 对应覆盖，不读取本机根目录 sdkconfig。每项生成size.json并上传应用/bootloader/分区表、ELF、MAP、flash_args和sdkconfig。LCD额外执行实际component graph与静态库symbol门禁，上传component-graph/module-symbol-edges JSON；Release检查SIM/benchmark/fault禁符号，LCD限制5MiB且rollback启用。硬件测试、烧录和发布仍属于独立验收。

组件版本以 dependencies.lock 和组件 manifest 为准；当前不缓存 managed_components。大小增长超过 5% 的基线比较 / 提示尚未接入。要求所有作业通过才能合并是目标，仓库分支保护是否启用尚未确认。远端工作流未执行，不能把本机测试通过记为 Actions 通过。

以下日期小节保留当轮测试 / 烧录状态；最新总数与当前覆盖以上面的“当前注册测试”为准。

## 相机连接主机回归（2026-10-01）

```powershell
cmake -S tests/host -B build/host -G "MinGW Makefiles"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

新增 `camera_link`、`ptp_session`、`ptpip_transport`、`camera_identity`，与此前五项合计九项。网络/NVS 使用主机模拟接口：覆盖唯一候选和已绑定目标筛选、退避、部分收发/等待取消、整笔及嵌套期限、0x200F 后同会话继续取帧、未完成数据阶段/错误事务号、Probe、0xC203 刷新、GUID-only 迁移、保存失败与记录损坏。不能据此宣称实际 DHCP、Wi-Fi、NVS Flash 或整机停止时延通过。

待实机回归：让手机先占用 `.2` 再连接相机；首次确认和两端断电后的同身份重连；同时连接两个候选时拒绝随机选择；握手及收图途中 `s`；拔掉相机后退避重连；删除相机授权后 InitFail 暂停；空闲 `u` 后正常重新配对；临时 0x200F 后继续取景。

## 属性描述与参数目标回归（2026-10-02）

`sony_parse_descriptors` 替换旧 Mode 特征搜索；新增 `sony_descriptors`、四个 `props_*` 抓包裁剪样本测试、`setting_control`、`gamepad_input` 和 `camera_actions`，CTest 合计 17 项。属性测试覆盖双列表、数组中的伪属性头、全截断点、异常计数、未知类型 / 表单、重复控制属性和读写标志；Sony 写入测试覆盖 1/2/4 字节、类型 / 数值越界和空输出指针。

`setting_control` 覆盖十次连续输入合并、接受后旧回读保持 PENDING、实际值确认后发送最终目标、拒绝、10 秒超时、枚举变化、只读和计时回绕。新手柄映射及 Mode / Focus 命令执行和状态绘制已接入工作区；不能据此宣称实际设置效果、手柄输入时延或 LCD 状态布局通过。样本来源见 [样本说明](../../tests/host/fixtures/README.md)。

`gamepad_input` 覆盖 X/Y 一次性切换、L1/R1 变焦 / 条件 MF、未知能力、按 X 停止肩键、同按锁定、重复不追赶、扳机迟滞、交叉释放、带压连接、录像状态未知 / 待确认、缺口与投递失败。`camera_actions` 覆盖溢出及超时释放、旧代数拒绝、释放在成功写入后清锁、计时回绕。Sony 线格式测试覆盖 S1/S2 和录像的 u16 2/1、变焦的 i8 ±1/0。录像状态 parser 的 0/1/未知值和截断有合成测试，实际机型枚举待确认。

17 项 CTest 及 LCD 构建通过，尚未烧录。镜头类型在运行时仍 UNKNOWN，非电动变焦 MF 替代仅由测试注入能力验证，真实识别仍需实现。

## I²C v2 回归（2026-10-02）

统一 CTest 新增 `atom_protocol`、`ds4_events` 和 `ds4_report`，共 20 项。CRC 标准向量、请求丢 / 插单字节、半帧超时及计时回绕、完整响应每字节损坏和截断、短错误响应、重试 seq / ack 不变、boot_id 变化、版本不匹配、三次失败及非法本地按键均有覆盖。事件测试覆盖 L3 去重、溢出 gap 重复返回、确认后清除、确认前再溢出不误清。新失败批次计数重置，HELLO 不继承 POLL 的失败次数。

两端已接入独立 I²C 任务 / 主机状态机，并通过固件构建。未做实机：旧 legacy 从机驱动的残留软件缓冲、15 ms 预备时间、断线 / 单端重启与 30 分钟稳定性仍需验收。主机测试不证明这些硬件行为。

## 新从机与灯阵（2026-10-02）

新增 matrix_model 与 atom_slave_tx，共 22 项通过。模型测试覆盖参考图案、启动收尾 / 异步初始化、连接节奏、LCD 心跳 / 等待、异常优先级、真实滑动错误窗口、溢出重计时及计时回绕。适配测试执行实际 atom_slave_replace_reply 代码，通过模拟 FIFO / ringbuffer 检查未读完旧响应被 7 / 19 / 35 字节新响应完全替换、超时和队列失败；不能证明中断调度或硬件传输。

两端已烧录并启动，HELLO boot_id / 新从机 / HID 初始化均确认。首轮 DS4 高频上报后 I²C 大量重试，稳定性未验收；优先级调整和后续日志另记。不把首次成功握手视为稳定性通过。物理四角映射、灯阵视觉、RMT 故障恢复和 30 分钟稳定性仍需验证。

## 设置菜单回归（2026-10-02）

新增 camera_menu，CTest 合计 23 项通过。覆盖真实快照的 ISO / EV / WB / Focus / Metering、夹紧边界、反向目标合并、旧回报、signed EV、只读 / 缺失 / 重复 / 截断描述；快门 / 光圈相对步进的等待、反向合并、拒绝、超时、无值保护及计时回绕。gamepad_input 增加 SETTINGS 隔离、400 / 150 ms 重复、多方向互锁、gap 和界面切换后的松键要求。sony_write 增加两项 ControlDeviceB int8 线格式验证。LCD 构建通过，代码测试不证明相机实际接受单步命令或菜单视觉正确。

Ultimate 2 增量回归：`ultimate2_report` 覆盖实机 HID 描述严格匹配、所有截断 / 字段变化拒绝、33 字节输入、轴 / 扳机边界、方向和按键；`pad_publish` 覆盖 DS4 优先、持键接入不生成按下、来源切换清缓存、断线归零、SIM 切换和事件 ID / 丢弃计数保留。

2026-10-06 新增独立OTA真实源码14场景、Core健康检查9场景与Core启动OTA初始化失败1场景，主机147/147；JSON/SDK为fake，不能替代flash/HTTP/关闭竞争实机，见[证据](../records/module-maintenance-ota-20261006.md)。

2026-10-06 新增实际Web+真实cJSON独立回归12场景及共享偏好存储8场景，167/167；Linux host须libcjson-dev，本机cmake可传-DMODULE_CJSON_SOURCE_DIR=<SDK components/json/cJSON>，不将SDK/HTTP fake当实机网络证据。见[记录](../records/module-maintenance-web-20261006.md)。

真实Wi-Fi周期换代事件的payload与Camera契约已新增检查，修复前复现、修复后全261通过，见[网络事件核对](../records/module-network-event-20261006.md)。本fixture时间/发送仍fake，网络与SMP及时性待实机验证。

LCD NVS写入口增加允许/旁路/全局擦除正反例门禁，完整262通过，见[owner核对](../records/module-storage-owner-20261006.md)。词法门禁不代替Flash/SMP验证。

真实wifi_esp32 factory与facade新增SDK边界fixture，覆盖partial init清理、event loop归属、start失败以及stop timeout保留/重试；完整263通过，见[契约记录](../records/module-wifi-backend-contract-20261006.md)。
