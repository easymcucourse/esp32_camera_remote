# 串口日志工具

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

LCD 的 `pair_console` 按行读取，Enter 执行，单字符命令仍兼容；Wi-Fi 配置与两级恢复出厂命令见 [串口手册](../user-guide/serial.md)。

| 命令 | 功能 |
| --- | --- |
| `j` | 开始 / 恢复取景；已运行时忽略 |
| `s` | 取消网络等待、清除控制请求、排空解码任务并关闭连接，保留最后画面 |
| `S` | 切换设置显示模式 |
| `p` | 配对及同一 GUID 重连诊断，需先停止取景 |
| `u` | 空闲时清除相机身份；先 `s` 并等 `Camera task finished`，再 `u`、`j/p` 重新配对 |

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

| 关键字 | 来源 | 含义 |
| --- | --- | --- |
| `AP READY` | `wifi_ap` | 热点已启动 |
| `Connected clients:` / `Client MAC=` | `wifi_ap` | 每 10 秒的客户端列表、IP 和 RSSI |
| `INIT ACK: camera=` | `camera_pair` | 相机接受 PTP/IP 连接 |
| `InitFail reason=` | `camera_pair` | 相机拒绝连接，通常需要重新进入配对等待画面 |
| `SESSION VERIFIED` | `camera_pair` | OpenSession 成功 |
| `LIVEVIEW RUNNING` | `camera_pair` | 开始连续取景 |
| `LIVEVIEW frames=… fps=…` | `camera_pair` | 约每 5 秒一次的帧率、读取和显示耗时、解码任务栈余量 |
| `Setting 0x… target=…` / `Menu 0x…` | `camera_pair` | Mode 目标或菜单参数写入，成功响应仍需属性回读确认 |
| `Camera disconnected` | `camera_pair` | 会话中断，按 1–30 秒退避重试 |
| `CAMERA DISCOVERED` / `PAIRING SAVED` | 相机模块 | DHCP 目标已找到 / Sony 初始化成功后已保存绑定 |
| `LCD frame synchronization timed out` | `board_7b` | 显示失效，需要重启 |
| `ATOM v2 online` / `Transaction failed` / `ATOM link lost` | `atom_link` | v2 握手、重试及离线状态 |
| `DS4 … pressed/released` / `Controller buttons=` | `atom_link` | 缓存按键变化及实时输入快照，LCD 视角 |
| `uptime=… free_internal=… free_psram=…` | `remote` | 每 10 秒的内存余量 |
| `DualShock 4 connected; input ready` | ATOM `ds4_host` | 手柄有效输入已到达 |
| `Event overflow: dropped=` | ATOM `atom_i2c` | 事件缓存溢出；LCD 按 gap 取消旧输入 |

## 注意事项

- 同一时刻一个串口只能被一个程序打开；记录前关闭 `idf.py monitor` 等其他串口程序。
- 日志文件统一放在 `build/`（不提交）；需要长期保留的证据放在 `captures/`，同样不提交。
- 默认端口写死为 COM8，修改清单 P3 计划改为自动查找或必须显式指定。
