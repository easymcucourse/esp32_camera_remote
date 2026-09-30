# M5Stack ATOM Matrix 子项目

这是一个独立的 ESP-IDF 5.5.1 工程，目标芯片为经典 ESP32。ATOM 作为 LCD 主系统的 I²C 从机，地址为 `0x42`。启动时灯阵为红色，LCD 首次成功连接后将灯阵设置为绿色。按键保留本地颜色切换，同时向 LCD 上报按键状态和累计按下次数。

## 编译与烧录

在已激活 ESP-IDF 环境的 PowerShell 中执行：

```powershell
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash monitor
```

把 `COM6` 替换为实际串口。本机 LCD 为 COM8，ATOM 为 COM6；ATOM 使用 115200 烧录已验证可用。串口监视器按 `Ctrl+]` 退出。

## 板载资源

| 资源 | GPIO | 说明 |
| --- | ---: | --- |
| 5×5 RGB LED | 27 | 25 颗 WS2812，GRB 顺序 |
| 正面按键 | 39 | 低电平按下，板上已有外部上拉 |
| Grove I²C SDA | 26 | Port A 黄色线 |
| Grove I²C SCL | 32 | Port A 白色线 |

代码不依赖 Arduino 或外部组件，LED 由 ESP-IDF RMT 驱动。DualShock 4 使用 ESP-IDF 公共 Classic Bluetooth HID Host API。

## DualShock 4（PS4）自动配对

1. ATOM 上电后自动搜索手柄。手柄断开 USB，按住 **SHARE + PS**，直到灯条快速闪烁，进入蓝牙配对模式。
2. ATOM 自动发现 `Wireless Controller`、连接并完成蓝牙认证，无需电脑或手动填写 MAC。收到有效输入后串口显示 `DualShock 4 connected; input ready`。
3. 固件把成功连接的手柄地址保存到 NVS，蓝牙栈保存绑定信息。下次 ATOM 上电或连接断开后，自动尝试上次的手柄；手柄关闭时需按 PS 唤醒。未连接时继续搜索，也可用 SHARE + PS 配对另一只手柄。
4. 每次扫描约 10 秒，连接超时约 20 秒。连接失败后继续重试。配对多只手柄时，一次只让目标手柄进入配对模式。

支持 Sony DS4 第一代（VID/PID `054C:05C4`）和第二代（`054C:09CC`），连接后检查 SDP 设备标识，排除其他同名设备。未声明这些标识的第三方手柄不在当前支持范围内。当前不实现震动、灯条控制或触摸坐标。

串口在按键变化时及每秒输出按键、左右摇杆、L2/R2 和电池原始值。摇杆范围 -128～127，中心约 0，扳机 0～255。电池值通常为 0～10，短输入报告不包含电池时为 255。断开后输入状态清零。

`ds4_host` 标签启用 DEBUG，其余标签保持 INFO。诊断日志包含扫描到的设备名称/CoD、连接来源、HID 阶段（connecting/connected/disconnected）、设备标识、认证结果、断开原因，以及输入报告 ID、长度、接收/拒绝累计数。每次连接的前 5 份输入报告输出前 16 字节，随后每 10 秒输出一次统计。`HID open status=0 phase=connecting` 仅表示请求已受理，`DualShock 4 connected; input ready` 才表示有效输入已到达。

在项目根目录抓取两分钟日志（不复位设备）：

```powershell
python tools/serial_log.py --port COM6 --seconds 120 --output captures/atom-ds4-debug.log
```

如需同时检查启动和已保存手柄重连，可加 `--reset`。手柄关闭时需要按 PS 唤醒；首次配对按 SHARE + PS。

按键位图 bit 0～17 依次为 Share、L3、R3、Options、上、右、下、左、L2、R2、L1、R1、三角、圆圈、叉、方块、PS、触摸板按下。`ds4_host_get_state()` 返回线程安全快照。LCD 收到 Start（DS4 Options）从松开到按下的变化时，切换设置界面和预览界面；持续按住不重复切换，松开后可再次按下切换。串口 `S` 仍可切换同一界面模式。

L1 切换上一个曝光 Mode，R1 切换下一个，按相机 `0x500E` 属性返回的可选值顺序循环；同时按下两者不切换，按住不重复。最多排队 32 次请求，由相机通信任务在预览事务之间执行：刷新 `0x9209` 属性、检查模式可写及可选值、通过 `0x9205` 的 UINT32 数据阶段设置、成功后回读实际模式。相机未连接、模式只读或无有效枚举时输出原因，不修改显示来假装设置成功。协议参考 [libgphoto2 Sony 属性设置](https://github.com/gphoto/libgphoto2/blob/master/camlibs/ptp2/ptp.c) 与 [Sony 属性描述解析](https://github.com/gphoto/libgphoto2/blob/master/camlibs/ptp2/ptp-pack.c)。

目标为经典 ESP32（ATOM Matrix 的 ESP32-PICO），不适用于 ESP32-S3/C3。输入布局参考 [Bluepad32 DS4 parser](https://github.com/ricardoquesada/bluepad32/blob/main/src/components/bluepad32/parser/uni_hid_parser_ds4.c)。

如已有旧 `sdkconfig`，在 `idf.py menuconfig` 中启用 Bluetooth、Bluedroid、Classic Bluetooth、HID、HID Host，选择 BR/EDR Only，关闭 SPP。首次生成配置自动采用 `sdkconfig.defaults`。

报告解析测试（在项目根目录、有主机 GCC 的环境运行）：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I m5_atom_matrix/main m5_atom_matrix/main/ds4_report.c m5_atom_matrix/tests/test_ds4_report.c -o m5_atom_matrix/build/test_ds4_report.exe
./m5_atom_matrix/build/test_ds4_report.exe
```

## LCD 主从通信

LCD-7B GPIO8（SDA）连接 ATOM GPIO26，GPIO9（SCL）连接 ATOM GPIO32，并共地。两端分别用 USB 供电时只连接 SDA、SCL、GND，不连接 Grove 的 5V。总线由 LCD 以 100 kHz 驱动；上拉至 3.3V，不能上拉至 5V。ATOM 的 `0x42` 地址避开 LCD 的 `0x24` 扩展芯片。

LCD 在 `main/atom_link.c` 中复用屏幕现有 I²C 主总线。未连接 ATOM 时每秒尝试一次；在线时约每 50ms 请求手柄状态（写命令后等待 30ms，读取后等待 20ms，另加总线耗时）。状态不变时无需等待屏幕绘制锁。LCD 串口以 `atom_link` 标签输出 DS4 连接变化、缓存事件序号、按键名称及 `pressed` / `released`，并在实时按键位图变化时及每秒输出摇杆、扳机和电池。Start（Options）按下切换设置/预览界面，L1/R1 切换曝光 Mode；其余按键仅上报日志。Mode 生效延迟及连续快速切换的限制见根目录 README。

ATOM 在有效蓝牙输入回调中缓存每次按键位图变化，按顺序保存最多 128 个事件（一次完整短按通常占按下、松开两个事件）。LCD 每次读取一个事件，下一条查询携带已处理事件 ID 作为确认。未确认的事件重复返回，LCD 按 ID 去重；因此两次轮询之间完成的短按和多次连续短按仍可逐个处理，重复响应不会重复触发 Start 切换。缓存只保存在 RAM，ATOM 重启后清空；超过 128 次变化时丢弃最旧事件并输出 `DS4 event cache overflow`。ATOM 重启后使用随机起始 ID，降低与旧确认号冲突的可能。ATOM 板载按键仍单独提供累计次数。

协议使用固定 8 字节命令，版本为 1。命令 `0`、`1` 返回 8 字节，命令 `2` 返回 18 字节实时状态，新增命令 `3` 返回 26 字节实时状态及缓存事件。一次写命令后，主机等待至少 30ms，再单独读取对应长度的响应；不能使用立即重复起始读取。每条命令必须读取响应后才能发送下一条。缓存上报需要同时更新 LCD 和 ATOM 固件。

| 字节 | 命令（LCD → ATOM） | 响应（ATOM → LCD） |
| --- | --- | --- |
| 0 | `0xA5` | `0x5A` |
| 1 | 版本 `1` | 版本 `1` |
| 2 | 请求序号 | 对应请求序号 |
| 3 | `0` 查询；`1` 设置全屏 RGB；`2` 查询手柄；`3` 查询缓存事件 | `0` 成功；`1` 格式错误；`2` 未知命令 |
| 4 | 红色 0–20 | 按键：0 松开，1 按下 |
| 5 | 绿色 0–20 | 累计按下次数低字节 |
| 6 | 蓝色 0–20 | 累计按下次数高字节 |
| 7 | 保留，填 0 | DualShock 4 连接状态：0 未连接，1 已连接 |

命令 `2` 的响应字节 0–7 与上表相同，新增字段如下：

| 响应字节 | 内容 |
| --- | --- |
| 8–10 | DS4 按键 bit 0–17，小端 24 位位图 |
| 11–14 | LX、LY、RX、RY，有符号 8 位，范围 -128～127 |
| 15–16 | L2、R2，范围 0～255 |
| 17 | 电池原始值 0～10，255 表示不可用 |

LCD 检查响应头、版本、请求序号和结果；通信失败时清除手柄连接状态和本地按键快照。ATOM 在收到查询后重新获取线程安全的 DS4 快照，避免使用主循环之前缓存的数据。

命令 `3` 的命令字节 4–7 改为已处理事件 ID（小端 32 位），0 表示无确认。响应字节 0–17 与命令 `2` 相同，18 为事件有效标志（0/1），19–22 为事件 ID（小端 32 位，非零），23–25 为该事件发生后的按键位图（小端 24 位）。按键动作由有序缓存事件触发，实时快照仅用于状态显示/日志，避免重复触发。LCD 通信失败时保留事件确认号和事件按键状态，以便恢复后继续确认、去重。

缓存测试：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I m5_atom_matrix/main m5_atom_matrix/main/ds4_events.c m5_atom_matrix/tests/test_ds4_events.c -o m5_atom_matrix/build/test_ds4_events.exe
./m5_atom_matrix/build/test_ds4_events.exe
```

RGB 超过 20 时从机会限制为 20。按键计数在复位时清零，65535 后回绕。ATOM 使用 IDF 5.5.1 的 legacy I²C 从机驱动；LCD 使用新版主机驱动。固定帧长度必须严格遵守。
