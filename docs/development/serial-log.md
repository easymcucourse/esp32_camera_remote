# 串口日志工具

[English](../en/development/serial-log.md) · **简体中文** · [日本語](../ja/development/serial-log.md)

`tools/serial_log.py` 在限定时间内记录串口输出，可选择先复位设备或发送一条串口命令。用于实测记录、问题复现和长时间稳定性测试。

## 依赖

需要 `pyserial`。ESP-IDF 的 Python 环境已包含；其他 Python 环境执行：

```sh
python -m pip install -r tools/requirements.txt
```

## 参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--port` | `COM8` | 串口名。LCD 本机为 COM8，ATOM 为 COM6，其他机器按实际端口指定 |
| `--seconds` | `25` | 最长记录时间（秒） |
| `--output` | `build/serial-boot.log` | 输出文件，目录不存在时自动创建；文件已存在时覆盖 |
| `--reset` | 关闭 | 打开串口后拉一次 RTS 复位设备，用于记录启动日志 |
| `--command` | 无 | 打开串口后发送一行 ASCII 文本（自动加换行），不复位设备 |
| `--until` | 无 | 日志中出现该 ASCII 文本时提前结束 |

行为说明：

- 打开串口前把 DTR、RTS 置为无效，**默认不会复位设备**；记录器退出也不会停止正在运行的取景。
- 原始字节写入文件，同时以 UTF-8（无法解码的字节替换）打印到终端。
- 波特率固定为 115200。

## LCD 串口命令

LCD 的 Console UART gateway 按行读取，Enter 执行，并通过 typed message 请求相应 endpoint。Wi-Fi 和 UI 偏好仅可查询；配置、清除身份和恢复出厂须在启动界面的维护 Web 操作。完整命令见 [串口手册](../user-guide/serial.md)。

| 命令 | 功能 |
| --- | --- |
| `j` | 开始 / 恢复取景；已运行时忽略 |
| `s` | 取消网络等待、清除控制请求、排空解码任务并关闭连接，保留最后画面 |
| `S` | 切换设置显示模式 |
| `p` | 配对及同一 GUID 重连诊断，需先停止取景 |
| `wifi show` | 查询当前和保存的热点配置 |
| `ui info` / `ui pad` | 查询启动时加载的显示和手柄偏好 |

## 常用示例

记录启动过程：

```sh
python tools/serial_log.py --port COM8 --reset --seconds 30 --output build/boot.log
```

开始取景并记录 30 秒：

```sh
python tools/serial_log.py --port COM8 --command j --seconds 30 --output build/liveview.log
```

停止取景，看到任务结束日志即退出：

```sh
python tools/serial_log.py --port COM8 --command s --seconds 15 --until "Camera task finished" --output build/stop.log
```

同时记录 LCD 和 ATOM（在两个终端分别运行）：

```powershell
python tools/serial_log.py --port COM6 --seconds 60 --output build/atom-input.log
python tools/serial_log.py --port COM8 --seconds 60 --output build/lcd-input.log
```

记录 ATOM 的 DS4 调试日志两分钟：

```powershell
python tools/serial_log.py --port COM6 --seconds 120 --output captures/atom-ds4-debug.log
```

## 自动相机连接测试

先完成固件烧录，关闭其他串口程序，再让相机连接热点并开启 PC 远程。使用包含 pyserial 的 ESP-IDF Python 环境执行：

```sh
python tools/test_camera_connection.py --port COM8 --connect-wait 120 --steady 30 --output captures/connection-hardware-test
```

脚本依次检查等待状态停止、重复启动保护、恢复后停止、首次取景、持续取景、取景中停止和已配对重连，并检测 panic／看门狗。它不会复位设备、清除配对或修改相机参数；测试完成后保留取景运行。需初始相机任务正在运行，否则初次停止检查会失败。相机未及时准备好或未取得 DHCP 租约也会使测试失败，应结合日志区分前置条件与固件故障。

`--connect-wait` 为等待首帧秒数，`--steady` 为连续取景观察秒数，`--output` 为日志前缀，生成 `.log` 和 `.json`。全部检查通过返回 0，任一检查失败返回 1。重启持久化需另用 `serial_log.py --reset` 观察 `stored_peer=1` 及首帧；该项不包含在此脚本中。本轮结果见 [实机记录](../records/connection-test-20261001.md)。

## 日志中的关键字

以下按当前源码列出。旧AP READY/client列表、INIT ACK/InitFail和逐vendor参数日志已随旧实现删除；当前使用UART status的Wi-Fi/Camera语义快照与错误码排查，旧日志不能作新固件必达标记。

| 关键字 | 来源 | 含义 |
| --- | --- | --- |
| `SESSION VERIFIED` | `camera_pair` | backend完整Sony初始化成功，不只OpenSession |
| `LIVEVIEW RUNNING` | `camera_pair` | 开始连续取景 |
| `LIVEVIEW frames=… fps=…` | `app_ui` | 约每 5 秒一次的帧率、Camera 读取和 UI 显示耗时、UI endpoint 栈余量 |
| `CAMERA DISCOVERED` / `PAIRING SAVED` | 相机模块 | DHCP 目标已找到 / Sony 初始化成功后已保存绑定 |
| `LCD frame synchronization timed out` | `board_7b` | 显示失效，需要重启 |
| `ATOM v2 online` / `Transaction failed` / `ATOM link lost` | `atom_link` | v2 握手、重试及离线状态 |
| `DS4 … pressed/released` / `Controller buttons=` | `atom_link` | 缓存按键变化及实时输入快照，LCD 视角 |
| `uptime=… free_internal=… free_psram=…` | `remote` | 每 10 秒的内存余量 |
| `DualShock 4 connected; input ready` | ATOM `ds4_host` | 手柄有效输入已到达 |
| `Event overflow: dropped=` | ATOM `atom_i2c` | 事件缓存溢出；LCD 按 gap 取消旧输入 |

## 注意事项

取景性能记录使用独立全屏 / 设置页日志，可用 `python tools/analyze_liveview.py build/liveview-full.log --expect-settings 0 --output build/liveview-full-summary.json` 统计加权 FPS、连续窗口、日志抽样 read/display/JPEG 分位数、堆低水位和异常行；设置页改用 `--expect-settings 1`。页面分项缺失或冲突时仍保留统计但返回失败，避免混合页面成为对比基线。首帧短窗口及复位/断流之间的间隔不会混入 FPS。`read` / `display` / `JPEG` 仅为约每五秒最后一帧样本，工具不会自动判定性能或稳定性验收。至少保留 615 秒日志以覆盖不少于 600 秒完整 FPS 窗口；环境、模式、版本、视觉确认仍需单独记录。

- 同一时刻一个串口只能被一个程序打开；记录前关闭 `idf.py monitor` 等其他串口程序。
- 日志文件统一放在 `build/`（不提交）；需要长期保留的证据放在 `captures/`，同样不提交。
- 默认端口写死为 COM8，修改清单 P3 计划改为自动查找或必须显式指定。
