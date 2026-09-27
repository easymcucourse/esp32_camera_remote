# ESP32-S3 Sony ZV-E10 Wi-Fi Remote

使用 ESP32-S3 创建 Wi-Fi 热点，通过 PTP/IP 连接 Sony ZV-E10，将实时取景显示在 Waveshare LCD-7B 上。基于 ESP-IDF 5.5.1，当前支持配对、重连及连续取景，尚未实现拍照、录像和参数设置。

## 功能与实测

- WPA2 热点、PTP/IP 命令及事件通道，配对 GUID 持久化到 NVS。
- 读取取景对象 `0xFFFFC002`，以 1024×576 原尺寸居中显示。
- CPU0 接收，CPU1 独立任务使用 ESP32-S3 SIMD JPEG 解码。
- 两个 1MiB 接收缓冲、LCD 双帧缓冲、30 行 DMA bounce buffer。
- 右上角显示实际更新 FPS，每约一秒更新；工程源码全局 `-O2` 优化。
- 串口暂停、恢复、配对诊断；通信失败后重试连接。

实测约 **3.5–4.0 FPS**，随 Wi-Fi 和画面内容变化。画面及 FPS 已由用户确认稳定，暂停／恢复测试通过；断电断网恢复尚未单独进行故障注入测试。

## 硬件与依赖

| 项目 | 配置 |
| --- | --- |
| 开发板 | Waveshare ESP32-S3-LCD-7B，本项目不启用触摸 |
| 内存 | 16MB Flash、8MB Octal PSRAM，PSRAM 80MHz |
| CPU | ESP32-S3 双核，240MHz |
| LCD | 1024×600 RGB565，18MHz 像素时钟 |
| 相机 | Sony ZV-E10，已测试固件 2.00 |
| SDK | ESP-IDF 5.5.1 |
| JPEG | espressif/esp_new_jpeg 1.0.2 |
| 串口 | 115200 baud，本机 COM8 |

引脚、扩展芯片和时序见 [硬件配置](docs/hardware.md)。JPEG 组件由组件管理器下载，版本固定在 manifest 和 `dependencies.lock`；首次构建需要网络。

## 编译与烧录

在已激活 ESP-IDF 5.5.1 的终端执行：

```sh
git clone https://github.com/easymcucourse/esp32_camera_remote.git
cd esp32_camera_remote
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

将 COM8 替换为实际串口；监视器按 `Ctrl+]` 退出。默认配置在 `sdkconfig.defaults`，应用分区为 4MiB factory，不提供 OTA。常规烧录保留 NVS 配对身份；擦除整片 Flash 会丢失该身份。

Windows 包装脚本：

```powershell
.\tools\idf.ps1 build
.\tools\idf.ps1 flash -Port COM8
.\tools\idf.ps1 monitor -Port COM8
```

默认 SDK 为 `C:\Espressif\frameworks\esp-idf-v5.5.1`，Python 为 `C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe`；不同安装位置通过 `-IdfPath`、`-IdfPython` 指定。

全局 O2 由 `CONFIG_COMPILER_OPTIMIZATION_PERF=y` 启用；厂商预编译 JPEG/Wi-Fi 库沿用其原有编译结果。

## 相机连接

1. 板子启动显示色条，并创建热点：SSID **esp32camap**，密码 **00000000**，信道 6。
2. 相机连接热点，启用 PC 远程功能，选择 Wi-Fi 接入点连接。
3. 首次连接进入配对等待画面；若提示确认，允许 **ESP32-Camera-Remote**。
4. 握手成功自动开始连续取景，上下各留 12 像素黑边。

当前针对实测设备配置：AP 为 `192.168.4.1`，相机为 `192.168.4.2`。更换相机或网络时，修改 `main/camera_pair.c` 的 `CAMERA_IP` 和 `camera_mac`；热点配置在 `main/wifi_ap.c` 的 `AP_SSID`、`AP_PASSWORD`。尚未实现通用设备发现或配置界面。

## 串口控制

| 命令 | 功能 |
| --- | --- |
| `j` | 开始／恢复取景，已运行时忽略重复请求 |
| `s` | 完成当前事务、排空解码任务并关闭会话，保留最后画面 |
| `p` | 配对及同一 GUID 重连诊断，需先停止取景 |

每约 5 秒记录显示帧率，每 10 秒报告客户端及内存。FPS 初始显示 0.0，约一秒后得到统计值；暂停时保留最后读数。握手等待确认时，停止可能需等待最长 120 秒。

使用 IDF Python 环境，或先执行 `python -m pip install -r tools/requirements.txt`：

```sh
python tools/serial_log.py --port COM8 --command j --seconds 30 --output build/liveview.log
python tools/serial_log.py --port COM8 --command s --seconds 15 --until "Diagnostic finished;" --output build/stop.log
```

同一时刻只打开一个串口程序。记录器退出不会停止取景，添加 `--reset` 才会主动复位。

## 项目结构

```text
main/                  启动、Wi-Fi AP、相机协议及解码任务流水线
components/board_7b/   LCD 初始化、JPEG 解码、帧同步、FPS 绘制
docs/                  硬件配置、协议分析和实测记录
tools/                 编译、串口记录、抓包与离线分析
sdkconfig.defaults     目标、内存和 O2 默认配置
dependencies.lock      固定组件依赖版本
partitions.csv         NVS、PHY 和应用分区
```

`build/`、`managed_components/`、`captures/`、`backups/`、`.reference/` 和本地 `sdkconfig` 不提交。文档引用的原始抓包、固件备份和日志仅保存在开发机器上。

## 抓包与协议分析

安装 Wireshark（含 dumpcap、tshark；Windows 需可用的抓包驱动），用 `dumpcap -D` 查询网卡编号，然后执行：

```powershell
.\tools\capture.ps1 -CameraIP 192.168.110.46 -Interface 1 -Seconds 120
python tools/analyze.py captures/your-capture.pcapng
```

替换实际 IP 和网卡编号。抓包包含相机及 SSDP/mDNS 发现流量，写入 `captures/`。分析工具重组 TCP 后解析 PTP/IP，输出同名 `.ptpip.csv`；可用 `--tshark` 指定程序路径。

`tools/extract_liveview_sample.py` 是首轮抓包复现脚本，固定读取 `captures/remote-20260927-193841.pcapng`、TCP stream 0、事务 11。该原始文件不随仓库提供；Pillow 仅用于可选的 JPEG 尺寸验证。

协议细节及性能迭代见 [通信分析与实测记录](docs/capture-analysis.md)。

## 参考资料

- [Waveshare LCD-7B](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7B)
- [官方 06_LCD 示例](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7B/tree/c652c902db607f7ffb376257393cfd7657aa6428/examples/ESP-IDF/06_LCD)
- [Sony ZV-E10 PC Remote 帮助](https://helpguide.sony.net/ilc/2070/v1/en/contents/TP0002392805.html)
- [Espressif esp_new_jpeg](https://components.espressif.com/components/espressif/esp_new_jpeg/versions/1.0.2/readme)
