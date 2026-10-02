# 故障排查与恢复

本文按现象列出常见问题的排查方法，以及恢复出厂设置（清除配对身份）的操作。日志记录方法见 [串口日志工具](../development/serial-log.md)，日志关键字的含义见该文档“日志中的关键字”一节。

> 当前有手柄热点页和串口两级重置，u 仅清除相机身份。全部重置已有代码 / 主机回归，真实设备重置与重新配对仍待验收，见 [实施状态](../development/implementation-status.md)。

## 相机连接

| 现象 | 可能原因 | 处理 |
| --- | --- | --- |
| 一直等待或发现相机 | 相机未加入热点、没有 DHCP 租约、PC 远程未开启，或与保存的绑定不匹配 | 核对客户端 IP 和相机 PC 远程；更换相机先 `s`，等任务退出，再 `u`、`j/p` |
| 相机 IP 不是 `192.168.4.2` | DHCP 分配的地址变化 | 新代码会自动取租约，无需改源码 |
| `Multiple cameras - connect only one` | 未绑定时存在多个可达 TCP 15740 候选 | 暂时只连接一台相机，再完成配对 |
| `Camera service unavailable` | 目标 TCP 15740 尚未可用 | 开启 PC 远程并退出相机网络设置页；固件按退避重试 |
| `InitFail reason=` / `Pairing rejected` | 相机拒绝初始化 | 在相机上进入配对等待或确认授权，再 `j/p`；此错误不会自动循环重试 |
| `Camera identity changed` | 同 MAC 返回了不同相机 GUID | 确认确实更换/重置相机后，空闲时用 `u` 重新配对 |
| 停在配对确认 | 等待用户确认，首次期限 120 秒 | 允许 **ESP32-Camera-Remote**；可用 `s` 取消网络等待 |
| `Pairing identity invalid` | NVS GUID/peer 长度错误或孤立记录 | 等任务结束后 `u` 清除相机身份；无需先擦除整片 Flash |
| `GetObject` 返回 `0x200F` | 相机暂时拒绝取景 | 完整拒绝会重用会话；连续超过 50 次才重连。保存日志核对相机模式 |
| `Camera disconnected` | 网络、超时、协议或解码失败 | 看之前的操作码/事务号/状态；按 1–30 秒退避重试 |
| 曝光 Mode 切换后显示没变 | 相机约 5 秒后才更新属性 | 查看 PENDING；连续输入已合并最终目标，等待期间约 500 ms 回读，10 秒未确认则 TIMEOUT。实际响应时延仍待验收 |
| 参数灰显 / `UNAVAILABLE` | 属性缺失、只读、未启用或可选值无效 | 检查相机状态，先等待有效属性快照 |

## 显示

| 现象 | 可能原因 | 处理 |
| --- | --- | --- |
| 画面撕裂、扫描起点上下跳动 | 像素时钟过高、PSRAM 带宽不足 | 保持 18MHz 像素时钟；检查是否误改 `board_7b.c` 时序 |
| 画面冻结，日志 `LCD frame synchronization timed out` | 帧同步超时后显示永久失效（尚无恢复流程） | 复位 LCD；记录发生前日志供分析 |
| 帧率明显低于 3.5 FPS | Wi-Fi 干扰；设置页（约 2.4 FPS）；画面细节多导致 JPEG 变大 | 看 `LIVEVIEW ... read=…ms display=…ms`：读取慢查 Wi-Fi，显示慢查解码路径 |
| 启动时随机崩溃或 PSRAM 内存测试失败 | PSRAM 120MHz 属于实验配置，个别板卡或高温下不稳定 | 在 menuconfig 中把 PSRAM 改为 80MHz；或备份本地 sdkconfig 后重新生成较低频率配置并编译 |

## ATOM 与手柄

| 现象 | 可能原因 | 处理 |
| --- | --- | --- |
| 连接页 ATOM 显示 Disconnected | 接线错误；未共地；地址探测失败 | 检查 LCD GPIO8/9 ↔ ATOM GPIO26/32 和 GND；两端分别 USB 供电时不要连 Grove 5V |
| ATOM 时连时断，日志 Transaction failed / ATOM link lost | 传输错误、CRC / 序号错误或响应准备超时 | 检查 3.3V 上拉、线长和 Reply / I2C transport 日志；对照 I²C v2 实测记录 |
| ATOM 灯阵显示启动进度或异常图案 | 初始化未完成、I²C 协议错误、事件溢出或蓝牙异常 | 对照 [灯阵设计](../design/matrix-led-design.md) 与 ATOM 日志；当前不再以整屏红 / 绿表示 LCD 离线 / 在线 |
| DS4 无法连接 | 未进入配对模式；手柄插着 USB；非 Sony 原厂手柄 | 断开 USB，按住 SHARE + PS 直到灯条快闪；已配对手柄按 PS 唤醒；只支持 VID/PID `054C:05C4`、`054C:09CC` |
| ATOM 显示 DS4 已连接但 LCD 不响应按键 | I²C 协议版本不一致 | 两端同时烧录最新固件 |
| 日志 `Event overflow: dropped=` | LCD 长时间未读取事件（I²C 断开或 LCD 卡住） | 检查 LCD 侧 `atom_link` 日志 |

## 编译与烧录

| 现象 | 处理 |
| --- | --- |
| 首次构建下载组件失败 | 需要网络访问 Espressif 组件仓库；检查代理 |
| `ESP-IDF not found at …` | 用 `-IdfPath` 指定 ESP-IDF 根目录，见 [编译与烧录](../development/build-and-flash.md) |
| 修改 sdkconfig.defaults 不生效 | 已有 sdkconfig 不会被 reconfigure 覆盖；用 menuconfig 修改，或备份并移开后重新生成 |
| ATOM 烧录失败 | 使用 `-b 115200` |
| 串口被占用 | 关闭 `idf.py monitor`、`serial_log.py` 等其他串口程序 |

## 恢复出厂设置

### 保存了什么

| 设备 | 内容 | 清除后的影响 |
| --- | --- | --- |
| LCD | NVS sony_remote/guid 与 peer：PTP/IP 身份及相机绑定 | 相机视为新设备，需要重新配对；旧授权可在相机菜单中删除 |
| LCD | NVS wifi_ap/cfg：SSID、密码、信道及显示开关 | 恢复 easycamctrl / 00000000 / 信道 6 / 显示密码 |
| ATOM | NVS `ds4_host`：上次手柄地址；蓝牙绑定信息 | 手柄需要重新按 SHARE + PS 配对 |

热点与相机身份分开保存。常规烧录保留两者，擦除 LCD NVS 会同时清除两者；ATOM 的 NVS 位于另一块设备。

### 使用已接入的重置入口

- 只恢复热点：factory wifi 后 10 秒内 factory confirm，或热点页 RESET WI-FI 后 3 秒内再按 A；相机身份保留。
- 热点与相机身份：factory all 后 10 秒内 factory confirm，或热点页 RESET ALL 后 3 秒内再按 A；等待相机停止 / 排空，成功后重启 LCD。ATOM 手柄绑定保留。
- 只清除相机身份：先 s，等待 Camera task finished，再 u，最后 j/p 重新配对。

全部重置失败不重启；身份记录可能已部分清除，应查看日志。具体命令和菜单步骤见 [串口手册](serial.md) 与 [手柄手册](controller.md)。

### 无法使用控制台时擦除 NVS

只擦除 `nvs` 分区，固件保留，无需重新烧录。在已激活 ESP-IDF 的终端中执行：

```sh
# LCD
parttool.py --port COM8 erase_partition --partition-name=nvs

# ATOM
parttool.py --port COM6 --baud 115200 erase_partition --partition-name=nvs
```

`parttool.py` 位于 `$IDF_PATH/components/partition_table/`，会从设备读取分区表。LCD 的 `nvs` 分区位于 `0x9000`，大小 `0x6000`，也可以直接用：

```sh
esptool.py --port COM8 erase_region 0x9000 0x6000
```

### 擦除整片 Flash

```sh
idf.py -p COM8 erase-flash
idf.py -p COM8 flash
```

擦除后必须重新烧录固件。整片擦除会删除固件和持久化数据，通常优先使用上面的重置入口或 NVS 分区擦除。当前 LCD 的 NVS 初始化失败会保留数据、使用默认热点继续运行，配置保存和相机身份可能不可用；NVS 分区自动修复尚未实现。

### 计划

当前已接入热点页两级重置和串口 u；NVS 分区故障恢复、界面独立清除相机身份 / 重配对入口与完整实机验收仍需完善，见 [Wi-Fi 热点需求](../request/wifi-ap-request.md#5-恢复出厂设置)。
