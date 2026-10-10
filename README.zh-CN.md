# ESP32 Camera Remote

[English](README.md) · **简体中文** · [日本語](README.ja.md)

Waveshare ESP32-S3-Touch-LCD-7B通过Wi-Fi/PTP/IP连接Sony ZV-E10，显示实时取景并控制相机。M5Stack ATOM Matrix接收手柄，经I²C提供相机输入，并独立用BLE控制DJI RS 3 Mini。

## 当前状态

以2026-10-10源码为准；完整功能和验证边界见 [当前状态](docs/development/current-status.md)。

- LCD：1024×600 RGB565、18MHz像素时钟；不初始化触摸。LIVE、SETTINGS、MORE、参数回读确认、录像提示及信息档位已有实现。
- 相机：动态发现、正常相机配对确认、命令/事件双通道、JPEG取景、属性与控制状态机；当前镜头按用户声明为电动变焦，无自动镜头识别。
- ATOM：Classic+BLE双模、DS4与限定Ultimate 2报告、电量/灯阵、I²C v2。Ultimate 2全部实体映射/相机控制仍待验。
- RS 3 Mini：真实Classic DS4左摇杆、L3原生回中、首次唯一目标自动连接/保存、换机配对、独立两轴幅度、停止/重连门禁。用户确认摇杆/L3和云台关机重启正常；任意记录零位、软限位和板载设置菜单未实现。
- 主机267项及LCD/ATOM Debug/Release四配置通过；编译不是硬件验收。精确停止时延、完整30分钟并发和LCD云台故障视觉仍待验。

## 硬件与接线

| 项目 | 配置 |
| --- | --- |
| LCD | ESP32-S3-Touch-LCD-7B，16MB Flash、8MB Octal PSRAM |
| 扩展 | ATOM Matrix，经典ESP32，不能使用S3/C3替代此工程 |
| I²C | LCD SDA8/SCL9 → ATOM SDA26/SCL32，GND共地 |
| 总线 | 100kHz、3.3V上拉、从机地址0x42 |
| 相机/云台 | Sony ZV-E10 / DJI RS 3 Mini |
| SDK | ESP-IDF5.5.1，依赖由manifest/lock固定 |

两端分别USB供电时只接SDA/SCL/GND，不接Grove5V。详细引脚见 [硬件设计](docs/design/hardware-design.md)。

## 构建与升级

在已激活ESP-IDF5.5.1的终端、仓库根目录运行：

```sh
python tools/ci_build.py lcd debug --build-tag local
python tools/ci_build.py atom debug --build-tag local
# 关闭模拟功能的生产构建
python tools/ci_build.py lcd release --build-tag local
python tools/ci_build.py atom release --build-tag local
# LCD兼容时钟配置
python tools/ci_build.py lcd debug --profile stable --build-tag local
```

产物分别在build/ci-<board>-<flavour>[-stable]-local。ATOM须启用BTDM/BLE/GATTC；旧sdkconfig不会被defaults自动覆盖。Default实验时钟和Stable80MHz配置独立，构建通过不证明长期稳定性。

确认设备端口、现有分区和产物后按 [编译/烧录](docs/development/build-and-flash.md) 升级。COM8/COM6只是本机历史示例；ATOM曾验证115200波特率。已部署LCD优先Web OTA；不要把完整首次flash命令当成保留OTA槽的应用更新。v1→v2升级必须更新两端。

## 使用

1. 相机加入LCD屏幕显示的热点，启用PC远程/Wi-Fi接入点连接。首次请求在相机上确认配对；地址由DHCP发现，不固定为历史IP。
2. DS4首次按SHARE+PS至灯条快闪；已配对按PS唤醒。有效输入日志表示就绪，Matrix的LCD灯不能代替手柄连接判定。
3. RS 3 Mini先完成官方激活、平衡并解锁轴，断开Ronin App。首次无目标时自动选择唯一Mini；已绑定时只重连原机，换机在ATOM串口执行 `gimbal pair`。重连后先松杆/松L3。

| 输入 | 功能 |
| --- | --- |
| Options/Start | LIVE ↔ SETTINGS |
| L1/R1 | Wide/Tele；两肩键同按停止、松开后恢复 |
| 方块/X、三角/Y | 对焦模式、曝光Mode，单击切换 |
| R2/RT | 半压对焦S1、全压拍照S2 |
| L2/LT | 半压无动作，全压切换录像目标 |
| 方向键 | SETTINGS导航/修改；右增加EV、左减小 |
| 叉/A、圈/B | 确认/返回；MORE扩展参数，Wi-Fi为信息页 |
| 触摸板按下 | 当前无动作；显示档位通过启动维护网页保存 |
| 左摇杆、L3 | ATOM直接控制云台Pan/Tilt、原生回中 |

新连接先释放扳机；录像状态未知时不猜测目标。相机手柄来源选择与云台真实Classic DS4来源独立。UART模拟输入不驱动物理云台。详情见 [手柄](docs/user-guide/controller.md)、[相机](docs/user-guide/camera.md)。

```text
gimbal status
gimbal speed pan 120
gimbal speed tilt 240
gimbal off
gimbal on
```

以上命令在ATOM串口输入；速度范围20..400，是协议偏移幅度而非角度/秒。 `gimbal speed 120` 同时设置两轴；工厂默认120/120，本机已确认120/240。设置保存在NVS。

LCD串口 `j` 启动/恢复、`s` 停止、`S` 切换设置、`p` 停止时配对/重连诊断；完整命令见 [串口手册](docs/user-guide/serial.md)。

## 维护与测试

仅启动连接页开放HTTP维护入口；访问屏幕IP后进入独占MAINTENANCE，普通相机/手柄服务排空。Web修改热点、偏好、恢复出厂和OTA；无PIN/登录，热点客户端可维护设备。保存/退出/OTA成功后重启，不原地返回取景。正常手柄/UART不编辑热点、不执行factory reset。见 [快速上手](docs/user-guide/quick-start.md)。

```sh
cmake -S tests/host -B build/host
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
python tools/check_doc_links.py
python tools/check_module_boundaries.py
```

主机Web测试需要cJSON（Linux安装libcjson-dev，Windows可指定SDK源码）。测试证明软件边界，不能证明Flash、射频、像素或相机动作。原始日志/抓包和memory均保持Git忽略。

## 文档与代码

[文档总览](docs/README.md)按English、中文、日本語排序；使用/开发/需求/设计与按日期保存的 [历史记录](docs/records/README.md) 分开。

主要目录：main为LCD入口；components为Core、路由、网络、输入、相机、UI和板级模块；common为共享纯协议；m5_atom_matrix为独立ATOM工程；tests/host为主机回归；tools为构建/串口/抓包/分析。字体及许可证在components/app_ui/fonts，公开协议参考许可证在third_party/rs3-protocol。

这是非官方项目。保留厂商名称仅表示适配对象；协议与验证范围见文档，不承诺全部设备兼容。通信证据发布规则见 [公开范围](docs/README.md#通信记录的公开范围)。
