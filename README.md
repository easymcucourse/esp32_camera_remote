# ESP32-S3 Sony ZV-E10 Wi-Fi Remote

使用 ESP32-S3 创建 Wi-Fi 热点，通过 PTP/IP 连接 Sony ZV-E10，将实时取景显示在 Waveshare ESP32-S3-Touch-LCD-7B 上。M5Stack ATOM Matrix 通过蓝牙连接 DualShock 4，再通过 I²C 向 LCD 上报手柄状态及按键事件。基于 ESP-IDF 5.5.1，当前支持配对、重连、连续取景、参数显示和曝光 Mode 切换；拍照、录像、变焦和七项菜单控制已接入代码，实际相机效果待逐项验收。

## 功能与实测

- WPA2 热点、动态 DHCP 地址发现、PTP/IP 双通道、配对 GUID 与相机绑定持久化。
- 可取消网络等待，1–30 秒退避重连，取景 `0x200F` 临时拒绝复用会话。
- 读取取景对象 `0xFFFFC002`，以 1024×576 原尺寸居中显示。
- CPU0 接收，CPU1 独立任务使用 ESP32-S3 SIMD JPEG 解码。
- 两个 1MiB 接收缓冲、LCD 双帧缓冲、30 行 DMA bounce buffer。
- 右上角显示相机电量、对焦模式、Wi-Fi RSSI、实际 FPS、相机型号、相机固件版本、曝光模式和 DS4 连接状态（屏幕不显示手柄电量）；设置界面底部也显示 DS4 状态，连接绿色、断开红色。
- Start（DS4 Options）或串口 `S` 切换预览和设置界面；当前映射为 Y 切曝光 Mode、X 切对焦模式、L1/R1 变焦或条件手动对焦，已烧录，待相机效果验收。
- ATOM 在蓝牙输入回调中缓存最多 128 次按键位图变化，LCD 确认后删除事件，并按事件 ID 去重，保留短按和连续按键。
- Inter、思源黑体和 JetBrains Mono 字体使用 FreeType 灰度抗锯齿渲染，字体资源和许可证随工程提交。
- 每 5 秒读取 Sony `0x9209` 属性数据，显示 ISO、快门、光圈、EV、白平衡、对焦、测光和闪光状态。
- 工程源码全局使用 `-O2` 优化，Octal PSRAM 运行于 120MHz。
- 串口暂停、恢复、配对诊断；通信失败后重试连接。

当前代码已接入离线维护网页、PIN 登录、热点设置、安全重启与双分区 OTA。Matrix 第一、第二、第三行分别显示 DS 手柄、BLE 手柄、云台电量；BLE 标准电池服务已接入并读到 Ultimate 2 的 88%；云台真实电量来源仍待接入。

最新显示局部合成 JPEG 基准约 **6.5 FPS**，设置页约 **3.26 FPS**；实际相机取景窗口约 **2.8 FPS**，尚未达到性能目标。基准不代表网络取景或长期稳定性，详见 [显示实测](docs/records/display-profile-test-20261003.md)。

## 硬件与依赖

| 项目 | 配置 |
| --- | --- |
| 开发板 | Waveshare ESP32-S3-Touch-LCD-7B，本项目不启用触摸 |
| 内存 | 16MB Flash（启动报告 80MHz）、8MB Octal PSRAM（120MHz） |
| CPU | ESP32-S3 双核，240MHz |
| LCD | 1024×600 RGB565，18MHz 像素时钟 |
| 相机 | Sony ZV-E10，当前固件 2.0.3 |
| 手柄扩展 | M5Stack ATOM Matrix（经典 ESP32），DualShock 4 |
| SDK | ESP-IDF 5.5.1 |
| JPEG | espressif/esp_new_jpeg 1.0.2 |
| 串口 | 115200 baud，本机 COM8 |

引脚、扩展芯片和时序见 [硬件配置](docs/design/hardware-design.md)。JPEG 组件由组件管理器下载，版本固定在 manifest 和 `dependencies.lock`；首次构建需要网络。

## 编译与烧录

在已激活 ESP-IDF 5.5.1 的终端执行：

```sh
git clone https://github.com/easymcucourse/esp32_camera_remote.git
cd esp32_camera_remote
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

将 COM8 替换为实际串口；监视器按 `Ctrl+]` 退出。默认配置在 `sdkconfig.defaults`，应用分区为两个 6MiB OTA 槽，另有 `otadata` 和 0x3E0000 字节 `data` SPIFFS 分区。常规烧录保留 NVS 配对身份，但完整烧录会初始化 OTA 启动元数据；已有 OTA 设备的更新方式见 [编译与烧录](docs/development/build-and-flash.md)。擦除整片 Flash 会丢失配对身份。

Windows 包装脚本：

```powershell
.\tools\idf.ps1 build
.\tools\idf.ps1 flash -Port COM8
.\tools\idf.ps1 monitor -Port COM8
```

ATOM 也可使用包装脚本：`./tools/idf.ps1 build -ProjectDirectory ./m5_atom_matrix -Port COM6`；ATOM 烧录指定 115200 的命令见下文。

默认 SDK 为 `C:\Espressif\frameworks\esp-idf-v5.5.1`，Python 为 `C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe`；不同安装位置通过 `-IdfPath`、`-IdfPython` 指定。

全局 O2 由 `CONFIG_COMPILER_OPTIMIZATION_PERF=y` 启用；厂商预编译 JPEG/Wi-Fi 库沿用其原有编译结果。

Flash / PSRAM 的高频配置参考 [Waveshare 官方性能配置](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7B/Instructions-For-Use#performance-notes-esp-idf--lvgl)，启用 `CONFIG_IDF_EXPERIMENTAL_FEATURES=y`。当前板载 Flash 不支持 IDF 的 HPM 模式，启动日志报告 80MHz；Octal PSRAM 以 120MHz 通过启动内存测试并完成连续取景验证。PSRAM 120MHz 在 ESP-IDF 中仍属于实验性功能，环境温度或不同批次存储器可能影响稳定性。

## 相机连接

1. 板子启动显示 `easymcucourse camera station` 连接页面，默认 SSID **easycamctrl**、密码 **00000000** 各占一行，下面显示 ATOM、手柄和相机连接状态，信道 6。
2. 相机连接热点，启用 PC 远程功能，选择 Wi-Fi 接入点连接。
3. 首次连接进入配对等待画面；若提示确认，允许 **ESP32-Camera-Remote**。
4. 页面依次显示等待相机、连接会话、配对确认和等待预览；第一帧成功解码显示后自动进入连续取景，上下各留 12 像素黑边。连接失败或取景断开后返回状态页并自动重试。

相机地址从 AP 客户端 DHCP 租约动态取得；未绑定时只接受唯一可达候选，绑定后按已保存 MAC 和相机 GUID 重连。更换相机先停止，再串口 `u` 清除相机身份。热点默认值在 `main/wifi_config.h`，用户配置保存到 NVS `wifi_ap/cfg`，可通过手柄热点页或串口修改。连接恢复已完成主机回归和实机连接测试，见 [烧录与连接测试](docs/records/connection-test-20261001.md)；热点重启后的 DHCP 恢复仍需关注。流程见 [相机连接](docs/user-guide/camera.md)。

## 串口控制

输入命令后按 Enter。Wi-Fi 配置、密码显示开关及二次确认的热点 / 相机身份重置见 [串口手册](docs/user-guide/serial.md)。

| 命令 | 功能 |
| --- | --- |
| `j` | 开始／恢复取景，已运行时忽略重复请求 |
| `S` | 切换设置显示模式：左侧 768×432 缩略图，右侧显示模式、ISO、快门、光圈、EV、白平衡、对焦、测光和闪光状态 |
| `s` | 取消网络等待、清除控制请求、排空解码任务并关闭连接，保留最后画面 |
| `p` | 配对及同一 GUID 重连诊断，需先停止取景 |
| `u` | 空闲时清除相机身份；先 `s` 并等 `Camera task finished`，再 `u`、`j/p` 重新配对 |

每约 5 秒记录显示帧率，每 10 秒报告客户端及内存。FPS 初始显示 0.0，约一秒后得到统计值；暂停时保留最后读数。握手和网络收发可取消；本轮取景中停止约 63 ms，完整结果见 [实机记录](docs/records/connection-test-20261001.md)。

详细属性来自相机 `0x9209` 数据集，每 5 秒更新。解析采用 Sony 扩展布局中的当前值字段（属性代码、类型、get/set、enabled、默认值、当前值、表单）。Sony 编码按 `F-number × 100`、快门高低 16 位分子/分母、ISO 低 24 位和 EV × 1000 转为可读格式；例如快门显示为 `1/50`。`--` 表示相机报告“无值”或尚未取得数据。

Sony 扩展枚举按属性代码解释。抓包中的曝光模式 `0x00078051` 显示为 `MOVIE A`（视频光圈优先），测光模式 `0x8001` 显示为 `MULTI`（多重测光）；相同数值出现在其他属性时可能具有不同含义。固件已收录常用拍摄模式、白平衡、对焦和测光枚举，尚未确认的值继续显示原始十六进制。映射与 [libgphoto2 Sony 实现](https://github.com/gphoto/libgphoto2/blob/master/camlibs/ptp2/config.c) 和 [pysonycam 枚举](https://github.com/olkham/pysonycam/blob/main/pysonycam/constants.py) 交叉验证。

使用 IDF Python 环境，或先执行 `python -m pip install -r tools/requirements.txt`：

```sh
python tools/serial_log.py --port COM8 --command j --seconds 30 --output build/liveview.log
python tools/serial_log.py --port COM8 --command s --seconds 15 --until "Camera task finished" --output build/stop.log
```

同一时刻只打开一个串口程序。记录器退出不会停止取景，添加 `--reset` 才会主动复位。

## ATOM 与手柄控制

LCD GPIO8（SDA）、GPIO9（SCL）、GND 连接 ATOM GPIO26、GPIO32、GND；两端分别 USB 供电时不连接 Grove 5V。总线为 100kHz，上拉到 3.3V，ATOM 从机地址 `0x42`。详细接线、蓝牙配对、I²C 协议与测试见 [ATOM 子项目](m5_atom_matrix/README.md)。

在已激活 ESP-IDF 环境的终端编译并烧录 ATOM：

```powershell
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash
```

本机 LCD 为 COM8、ATOM 为 COM6，其他机器按实际端口替换。ATOM 高波特率烧录曾失败，115200 已验证可用。I²C 缓存协议更新需同时烧录两端，单独修改 LCD 界面或相机控制只需更新 LCD。

当前使用 I²C v2 的 HELLO / POLL、CRC8、boot_id 与 gap，两端必须同时升级，实机验收尚未完成。LCD 三次失败后释放输入，版本不匹配显示提示；ATOM 新版从机与独立回复任务已接入，灯阵改由独立状态渲染任务控制。已烧录首次验证，DS4 高频输入下的 I²C 重试正在修正；协议与验证限制见 [ATOM 说明](m5_atom_matrix/README.md#lcd-主从通信)。

| 手柄按键 | 功能 |
| --- | --- |
| Start / Options | 切换设置与预览界面 |
| L1 | Wide（拉远）；确认非电动变焦镜头且 MF 时近对焦（+1） |
| R1 | Tele（拉近）；确认非电动变焦镜头且 MF 时远对焦（−1） |
| X / 方块 | 按相机可选枚举循环切换对焦模式 |
| Y / 三角 | 循环切换下一个曝光 Mode |
| LT / RT | 半压合并为 S1；LT 全压请求录像目标，RT 全压 S2 拍照 |
| 方向键 | SETTINGS：上下循环移动七项参数及 WI-FI 光标；左右修改参数，400 ms 后每 150 ms 重复 |
| A / 叉、B / 圈 | A 或右方向进入 WI-FI；热点页 A 确认、B 返回 |

以上映射已编译并烧录，尚待相机动作实机验收，LIVE / SETTINGS 相同。X/Y 单击切换、按住不重复；Mode / Focus 目标合并并等待相机回读确认。L1/R1 同按停止，必须两键松开后重新操作；按 X 取消当前肩键操作。

肩键 MF 替代仅在镜头明确为非电动变焦且相机为 MF 时启用：首步立即发送，400 ms 后每 150 ms 请求一步。即使数字变焦可用，此条件下也优先对焦。目前没有已验证的镜头类型来源，运行时类型未知，不启用这一分支；不能用变焦不可用推断镜头类型。详情见 [手柄需求](docs/request/gamepad-request.md)。

首次配对按住 SHARE + PS 至灯条快闪；已保存配对时按 PS 唤醒。ATOM 绿灯只表示 LCD 与 ATOM 的 I²C 通信建立，DS4 就绪以 `DualShock 4 connected; input ready` 和有效输入为准。LCD 的 `atom_link` 标签输出缓存事件、按键及输入快照。

**实测限制：**相机设置响应 `0x2001` 后，紧接着回读仍可能是旧 Mode，约 5 秒后才更新。新状态机合并连续输入，等待期间约 500 ms 查询，10 秒超时；效果待实机验收。事件缓存为 RAM，ATOM 重启即清空；工作区 v2 使用 boot_id 和 gap 明确报告重启及溢出，真实断线恢复仍待验收。

```powershell
python tools/serial_log.py --port COM6 --seconds 60 --output build/atom-input.log
python tools/serial_log.py --port COM8 --seconds 60 --output build/lcd-input.log
```

在两个终端分别运行可同时记录。蓝牙连接失败先检查 ATOM 的 `ds4_host` 日志；LCD 显示断开还需对照 `atom_link`，区分蓝牙与 I²C 链路。

## 故障排查与恢复

常见连接、显示及 ATOM 问题见 [故障排查](docs/user-guide/troubleshooting.md)。只重置热点可用 factory wifi，热点、LCD 相机身份和显示档位一起重置可用 factory all，两者都需 10 秒内输入 factory confirm。全部重置停止相机后清除指定记录，成功重启 LCD，ATOM 手柄绑定保留；该路径的实机效果仍待验收。热点菜单也提供两级二次确认，操作见 [手柄手册](docs/user-guide/controller.md#热点页)。

## 项目结构

```text
main/                  启动、Wi-Fi AP、连接控制、身份、手柄输入及解码流水线
components/board_7b/   LCD 初始化、JPEG 解码、帧同步、字体及参数显示
components/ptpip/      PTP/IP 传输、报文、会话及标准数据集解析
components/sony_camera/ Sony 扩展命令、属性及能力解析
common/                两端共用 I²C 协议、LCD 链路状态机和串口参数解析
tests/host/            54 项 CTest（含维护 JSON / 热点、UART / I²C 监视 / 手柄模拟、ATOM 测试和四份属性样本）
m5_atom_matrix/        M5Stack ATOM Matrix 独立 ESP-IDF 子项目
docs/                  硬件配置、协议分析和实测记录
tools/                 编译、串口记录、自动连接测试、抓包与离线分析
sdkconfig.defaults     目标、内存和 O2 默认配置
dependencies.lock      固定组件依赖版本
partitions.csv         NVS、OTA 元数据、PHY、双 6MiB 应用和 data 分区
```

`build/`、`managed_components/`、`captures/`、`backups/`、`.reference/` 和本地 `sdkconfig` 不提交。文档引用的原始抓包、固件备份和日志仅保存在开发机器上；脱敏属性裁剪样本提交在 `tests/host/fixtures/`。代码与文档核对结果见 [实施状态](docs/development/implementation-status.md#代码与文档核对2026-10-03)。

LCD 字体采用 Inter（英文）、思源黑体（中文）和 JetBrains Mono（参数数字），通过 FreeType 按实际字号进行灰度抗锯齿渲染，字形缓存置于 PSRAM。裁剪后的字体及原始许可证随工程提交，具体覆盖范围、内存开销和资源生成方法见 [字体说明](components/board_7b/fonts/README.md)。

## 抓包与协议分析

安装 Wireshark（含 dumpcap、tshark；Windows 需可用的抓包驱动），用 `dumpcap -D` 查询网卡编号，然后执行：

```powershell
.\tools\capture.ps1 -CameraIP 192.168.110.46 -Interface 1 -Seconds 120
python tools/analyze.py captures/your-capture.pcapng
```

替换实际 IP 和网卡编号。抓包包含相机及 SSDP/mDNS 发现流量，写入 `captures/`。分析工具重组 TCP 后解析 PTP/IP，输出同名 `.ptpip.csv`；可用 `--tshark` 指定程序路径。

`tools/extract_liveview_sample.py` 是首轮抓包复现脚本，固定读取 `captures/remote-20260927-193841.pcapng`、TCP stream 0、事务 11。该原始文件不随仓库提供；Pillow 仅用于可选的 JPEG 尺寸验证。

协议细节及性能迭代见 [通信分析与实测记录](docs/records/protocol-analysis.md)。

## 参考资料

- [Waveshare LCD-7B](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7B)
- [官方 06_LCD 示例](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7B/tree/c652c902db607f7ffb376257393cfd7657aa6428/examples/ESP-IDF/06_LCD)
- [Sony ZV-E10 PC Remote 帮助](https://helpguide.sony.net/ilc/2070/v1/en/contents/TP0002392805.html)
- [libgphoto2 Sony PTP 属性实现](https://github.com/gphoto/libgphoto2/blob/master/camlibs/ptp2/config.c)
- [pysonycam Sony PTP 枚举](https://github.com/olkham/pysonycam/blob/main/pysonycam/constants.py)
- [Espressif esp_new_jpeg](https://components.espressif.com/components/espressif/esp_new_jpeg/versions/1.0.2/readme)
