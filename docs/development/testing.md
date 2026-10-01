# 测试与持续集成设计

本文汇总项目的测试分层、现有测试、计划新增的测试、实机与故障注入测试、长时间稳定性测试以及 CI 方案。各模块的具体测试用例写在对应设计文档中，本文只做汇总和约定。

> 草案：目前只有 ATOM 的两个主机测试，CI 尚未建立。

## 1. 测试分层

| 层级 | 运行环境 | 目的 | 现状 |
| --- | --- | --- | --- |
| 主机单元测试 | PC，gcc / clang | 纯 C 解析、编码、状态机 | 2 个（ATOM） |
| 主机工具验证 | PC | 字体渲染、界面截图、抓包样本解析 | `font_preview_host` |
| 编译检查 | CI，ESP-IDF 5.5.1 | 两个工程都能编译 | 手工 |
| 实机功能测试 | 开发板 + 相机 + 手柄 | 按需求文档的验收测试逐项确认 | 手工 |
| 故障注入测试 | 实机 | 断电、断网、异常数据、资源耗尽 | 未进行 |
| 稳定性测试 | 实机，≥ 30 分钟 | 帧率、延迟、内存、重连次数 | 未系统进行 |

## 2. 主机单元测试

### 约定

- 纯 C11，只用 `assert` 和标准库，不引入测试框架；与现有 `m5_atom_matrix/tests` 风格一致。
- 被测模块不依赖 FreeRTOS、lwIP、`esp_log`；需要时间的接口由调用方传入 tick 或微秒。
- 编译选项：`-std=c11 -Wall -Wextra -Werror`；CI 中另加 `-fsanitize=address,undefined`。
- 每个测试是独立可执行文件，返回 0 表示通过。

### 现有

| 测试 | 被测 | 命令 |
| --- | --- | --- |
| `test_ds4_report` | `ds4_report.c` | 见 [ATOM 子项目说明](../../m5_atom_matrix/README.md) |
| `test_ds4_events` | `ds4_events.c` | 同上 |

### 计划

| 测试 | 所在目录 | 设计文档 |
| --- | --- | --- |
| `test_ptpip_packet`、`test_ptp_dataset`、`test_sony_props`、`test_sony_format`、`test_sony_liveview`、`test_ui_presenter` | `tests/host/` | [Sony PTP/IP 客户端分层设计](../design/sony-ptpip-design.md#12-测试) |
| `test_ptp_session`（假 transport 切片回放） | `tests/host/` | 同上 |
| `test_atom_protocol`（CRC8、编解码、重新同步） | `tests/host/`，两端共用 | [I²C 通信协议](../design/i2c-protocol-design.md#测试) |
| `test_gamepad_input`（扳机、安全释放） | `tests/host/` | [手柄输入处理设计](../design/gamepad-design.md#9-测试) |
| `test_matrix_status`（灯阵渲染） | `m5_atom_matrix/tests/` | [Matrix LED 状态显示设计](../design/matrix-led-design.md#主机渲染测试) |
| `test_gimbal_control` | `m5_atom_matrix/tests/` | [BLE 云台控制设计](../design/gimbal-design.md#7-测试) |
| `test_wifi_config`（校验、默认值、NVS 记录） | `tests/host/` | [Wi-Fi 热点设计](../design/wifi-ap-design.md#13-测试) |
| `test_pad_cmd`、`test_dbg_line`、`test_i2c_monitor` | `tests/host/`，两端共用 | [UART 调试控制台设计](../design/uart-debug-design.md#12-测试) |
| `test_maint_auth`、`test_ota_header` | `tests/host/` | [维护页面设计](../design/maintenance-design.md#10-测试) |

### 测试样本

- 由 `tools/extract_liveview_sample.py` 从本地抓包导出，放在 `tests/host/fixtures/`。
- 导出时把 GUID、相机序列号替换为固定值；不提交原始 `.pcapng`。
- 每个样本附一行说明：来源抓包文件名、TCP 流、事务号、相机固件版本。

## 3. 实机功能测试

以各需求文档的“验收测试”一节为准：

- [Sony 相机连接需求](../request/sony-ptpip-request.md#验收测试)
- [界面显示方案](../request/ui-request.md#验收测试)
- [手柄控制方案](../request/gamepad-request.md#验收测试)
- [Matrix LED 状态显示需求](../request/matrix-led-request.md#实机测试)
- [Wi-Fi 热点需求](../request/wifi-ap-request.md#验收测试)
- [维护页面需求](../request/maintenance-request.md#验收测试)
- [BLE 云台控制需求](../request/gimbal-request.md#验收测试)

每次发布前至少执行：冷启动配对、连续取景 5 分钟、`s` / `j` 停止恢复、Start 与 L1/R1、ATOM 拔插。

## 4. 故障注入

| 类别 | 方法 | 期望 |
| --- | --- | --- |
| 相机断电 / 重启 | 取景中关闭相机 | 返回连接页，显示原因，按退避重连 |
| Wi-Fi 断开 | 相机关闭 Wi-Fi；远离热点 | 同上 |
| TCP 异常 | PC 端模拟相机（回放抓包并注入截断、超长、错误事务号、连接复位） | 不崩溃、不越界，断开重连 |
| 事件积压 | 模拟相机连续发送事件 | 每帧读取上限生效，取景不被饿死 |
| PSRAM 分配失败 | 调试构建中用钩子让 `heap_caps_malloc` 第 N 次失败 | 记录日志并重试，不复位 |
| LCD 帧同步超时 | 调试构建中屏蔽帧完成回调 | 执行恢复流程（实现后） |
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

平台使用仓库托管方提供的 CI（GitHub Actions），触发条件为推送和合并请求。

| 作业 | 环境 | 步骤 |
| --- | --- | --- |
| `build-lcd` | `espressif/idf:v5.5.1` 容器 | `idf.py set-target esp32s3 build`；上传 `.bin` 和 `size` 报告 |
| `build-atom` | 同上 | `cd m5_atom_matrix && idf.py set-target esp32 build` |
| `host-tests` | `ubuntu-latest`，gcc | 编译并运行全部主机测试，开启 ASan / UBSan |
| `docs-links` | `ubuntu-latest` | 检查 Markdown 相对链接有效 |

约定：

- 所有作业通过才能合并。
- 组件依赖由 `dependencies.lock` 固定；CI 缓存 `managed_components/`。
- 固件大小报告与上一次对比，增长超过 5% 时在合并请求中提示。

## 相机连接主机回归（2026-10-01）

```powershell
cmake -S tests/host -B build/host -G "MinGW Makefiles"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

新增 `camera_link`、`ptp_session`、`ptpip_transport`、`camera_identity`，与此前五项合计九项。网络/NVS 使用主机模拟接口：覆盖唯一候选和已绑定目标筛选、退避、部分收发/等待取消、整笔及嵌套期限、0x200F 后同会话继续取帧、未完成数据阶段/错误事务号、Probe、0xC203 刷新、GUID-only 迁移、保存失败与记录损坏。不能据此宣称实际 DHCP、Wi-Fi、NVS Flash 或整机停止时延通过。

待实机回归：让手机先占用 `.2` 再连接相机；首次确认和两端断电后的同身份重连；同时连接两个候选时拒绝随机选择；握手及收图途中 `s`；拔掉相机后退避重连；删除相机授权后 InitFail 暂停；空闲 `u` 后正常重新配对；临时 0x200F 后继续取景。
