# M5Stack ATOM Matrix 子项目

[English](README.md) · **简体中文** · [日本語](README.ja.md)

这是一个独立的 ESP-IDF 5.5.1 工程，目标芯片为经典 ESP32。ATOM 作为 LCD 主系统的 I²C 从机，地址为 `0x42`。灯阵由独立状态任务显示启动进度、LCD / 无线连接与异常；LCD 不下发颜色命令。板载按键只上报状态和累计按下次数，不再切换颜色。

普通状态前三行分别显示 DS 手柄、BLE 手柄、云台电量，最多五颗从左向右表示容量；≤20% 红闪，未连接或未知时熄灭。第五行保留连接灯，启动与故障图案仍优先覆盖。DS 使用实际报告，BLE 标准电量已接入；RS 3 Mini 云台电量解码已接入，真实读数与验收见最新实测记录。布局细则见 [Matrix 显示需求](../docs/request/matrix-led-request.md#普通状态布局)。

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

LCD映射：Y（△）曝光Mode，X（□）对焦模式，单击循环、目标合并/真实回读；L1/R1为Wide/Tele，两键同按停止并锁定至松开。当前镜头按用户声明为POWER_ZOOM，无自动识别，不启用非电动变焦MF替代。全部相机动作仍需逐项验收，见 [手柄需求](../docs/request/gamepad-request.md)。

目标为经典 ESP32（ATOM Matrix 的 ESP32-PICO），不适用于 ESP32-S3/C3。输入布局参考 [Bluepad32 DS4 parser](https://github.com/ricardoquesada/bluepad32/blob/main/src/components/bluepad32/parser/uni_hid_parser_ds4.c)。

如已有旧 `sdkconfig`，在 `idf.py menuconfig` 中启用 Bluetooth、Bluedroid、Classic Bluetooth、HID、HID Host，选择 BTDM 双模，启用 BLE / GATTC，保留至少两条 BLE 连接，关闭 SPP。首次生成配置自动采用 `sdkconfig.defaults`；BR/EDR Only 旧配置不能构建现有 BLE 模块。

## RS 3 Mini

ATOM无保存目标时自动选择唯一名称匹配的RS 3 Mini，多候选不选；已保存时严格重连原目标，不自动替换。通知初始化/中立提交/有效RX后保存目标，换机用 `gimbal pair`；不改变DS4绑定。请关闭 Ronin App 的连接，先给云台平衡并解锁轴。真实 DS4 左摇杆控制 Pan/Tilt，L3 请求云台原生回中；连接后先松开 L3、摇杆回中心才能运动。LCD 断线不影响控制。UART `gimbal status` 查看链路，`gimbal off/on` 持久化启用，`pair` 重新选目标，`stop` 请求中立；`gimbal speed tilt 240` 单独提高上下幅度，`gimbal speed pan 120` 单独设置左右，范围20..400，原 `gimbal speed 120` 同时设置两轴，均持久化。完整协议和未实现项见 [当前设计](../docs/design/rs3-mini-protocol.md)。

当前仅真实 Classic DS4 驱动云台；相机来源选择仍独立。UART SIM 开启时云台运动被禁止。软限位、任意记录零位和板载设置菜单尚未实现，构建成功不代表运动与回中已经实测。

报告解析测试（在项目根目录、有主机 GCC 的环境运行）：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I common -I m5_atom_matrix/main m5_atom_matrix/main/ds4_report.c m5_atom_matrix/tests/test_ds4_report.c -o m5_atom_matrix/build/test_ds4_report.exe
./m5_atom_matrix/build/test_ds4_report.exe
```

## LCD 主从通信

LCD-7B GPIO8（SDA）连接 ATOM GPIO26，GPIO9（SCL）连接 ATOM GPIO32，并共地。两端分别用 USB 供电时只连接 SDA、SCL、GND，不连接 Grove 的 5V。总线由 LCD 以 100 kHz 驱动；上拉至 3.3V，不能上拉至 5V。ATOM 的 `0x42` 地址避开 LCD 的 `0x24` 扩展芯片。

工作区已升级协议 v2，必须同时更新 LCD 与 ATOM；与旧版 v1 不兼容；两端已有 v2 烧录与握手记录。离线每秒探测，HELLO 校验协议、能力及 boot_id，在线 POLL 周期 50 ms，写后等待 15 ms。单次失败保持 seq / ack 重试，连续三次失败才离线；版本不匹配显示提示，每 5 秒重试。

| 命令 | 请求 | 成功响应 | 内容 |
| --- | --- | --- | --- |
| HELLO `0x01` | 9 字节，param=0x0202 | 19 字节 | boot_id、固件版本、能力、缓存容量、local_mask |
| POLL `0x10` | 9 字节，param=ack_id | 35 字节 | boot_id、设备状态、故障、电池、按键、最新输入、缓存事件 |

CRC-8/SMBUS 校验值 `123456789` → `0xF4`。错误响应按 len=0 的 7 字节定位 CRC。共用模块为 `common/atom_protocol.*`、`common/atom_client.*`，完整偏移见 [I²C v2 设计](../docs/design/i2c-protocol-design.md)。ATOM 独立任务支持垃圾字节搜索、CRC 校验及 20 ms 半帧超时；已采用新版从机 v2 驱动；ESP-IDF 5.5.1 专用适配层在核心 0 清理旧软件缓冲 / FIFO 并替换响应，SDK 升级须重审。

缓存容量 128，入队前清除本地 L3 并去重，左摇杆不出现在协议中。溢出返回 gap，确认后清故障；再次溢出不能误清。LCD 对 gap 只同步位图，ATOM 重启立即清旧输入并重新 HELLO，重连先丢弃旧缓存，不重放命令。RGB 命令已移除，独立 matrix_status 任务显示五阶段启动进度、固定连接灯及三种异常图案，纯 C 模型有主机验证；四角映射和视觉仍待验收。板载按键次数在 65535 后回绕。云台状态由gimbal_link实时上报，0为关闭/断开、1搜索、2连接中、3控制ready，fault另有bit3。

主机回归与双端构建：

```powershell
cmake --build build/host -j 4
ctest --test-dir build/host --output-on-failure
./tools/idf.ps1 build
./tools/idf.ps1 build -ProjectDirectory ./m5_atom_matrix -Port COM6
```

已烧录，确认 v2 HELLO、新从机和 HID 初始化。首轮 DS4 高频输入下出现大量重试，提高回复任务 / ISR 优先级后 65 秒仍有一次 CRC 错误；IRAM 修正已烧录，单端复位恢复已观察，高频输入窗口尚待复现；单端重启、拔线、残留 FIFO 注入、15 ms 响应上限和 30 分钟稳定性仍待验收。

2026-10-03：双端行控制台与 ATOM pad sim 已烧录；真实 I²C 上切页、摇杆 / 扳机、溢出保护及 50 次 100ms 点按通过。生产可关闭 CONFIG_REMOTE_DBG_SIM，独立构建已通过；当前设备运行开发版本。命令见 [串口手册](../docs/user-guide/serial.md)，证据与边界见 [模拟输入实测](../docs/records/pad-sim-test-20261003.md)。


## BLE 手柄电量

ATOM使用Classic+BLE，ble_clients统一GAP/GATTC回调并仲裁BLE手柄/云台扫描，Classic DS4独立。BLE手柄按gamepad/joystick外观或支持名称筛选，连接后须验证HID服务；唯一候选才连接，多候选/列表溢出不选择。使用无输入输出绑定，标准Battery Service0x180F/Level0x2A19约10秒更新，断开/非法值清为255。

Ultimate 2 已在本机读到 88%；实体按键 / 相机控制仍需验收，已新增限定描述的输入适配。`status` 显示连接状态与三行电量；开发日志 `log ble_gamepad debug` 可观察广播、报告描述和受限频率的输入字节，不记录在正式文档中的原始设备身份。旧 BR/EDR-only sdkconfig 必须备份后重新生成；CI 入口检查双模与 GATTC 开关。

Ultimate 2 输入适配使用实机 113 字节 HID 描述严格匹配和唯一通知特征作为门禁。33 字节报告转换为共用快照 / 事件，再经 I²C v2 交给 LCD；DS4 优先，BLE 无输入一秒释放。`ble map` 输出缓存的描述用于诊断。实体字母键 / 扳机方向、相机操作、重启自动重连及长期稳定性仍需实机验证。
