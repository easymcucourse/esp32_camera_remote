# 文档总览

本目录收录使用手册、开发文档、需求、设计、工具用法和实测记录。项目概况见根目录 [README](../README.md)，ATOM 子项目见 [m5_atom_matrix/README.md](../m5_atom_matrix/README.md)。

## 开发环境

| 项目 | 型号 / 版本 | 说明 |
| --- | --- | --- |
| 主控与显示 | 微雪 ESP32-S3-Touch-LCD-7B | [产品文档](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7B) |
| 无线外设桥接 | M5Stack ATOM Matrix | [产品文档](https://docs.m5stack.com/en/core/Atom-Matrix) |
| SDK | ESP-IDF 5.5 | 当前使用 5.5.1，安装路径见[编译与烧录](development/build-and-flash.md) |

## 适配目标

| 类别 | 型号 |
| --- | --- |
| 相机 | Sony ZV-E10 |
| 云台 | 大疆 RS 3 Mini |

手柄按两个独立维度区分：蓝牙链路是传统蓝牙（Classic / BR/EDR）或 BLE；输入报告是 Sony 兼容或 Xbox 兼容。传统蓝牙上也可以是 Xbox 兼容，BLE 上也可以是 Sony 兼容。两类报告不能共用解析器。

| 型号 | 蓝牙链路 | 输入兼容 | 状态 |
| --- | --- | --- | --- |
| DualShock 4 | 传统蓝牙 | Sony 兼容 | 当前固件 |
| 八位堂猎户座 2 代 | BLE | Xbox 兼容 | 未实现 |

八位堂以 [PC 版产品页](https://www.8bitdo.cn/ultimate-2-wireless-controller/)为准，其蓝牙连接写明为低功耗蓝牙。同名 NS 版的蓝牙连接对象是 Switch，不属于上表。按键动作规则见[手柄控制方案](request/gamepad-request.md)。

## 目录结构

```text
docs/
  README.md                      本文：文档索引、阅读顺序、免责声明与通信记录公开范围
  user-guide/                    使用手册：连接、操作、排错
    README.md
    quick-start.md               烧录、接线、第一次连接
    camera.md                    连接步骤、当前地址、能力边界与排错
    controller.md                ATOM 接线、DS4 配对和已实现按键
    serial.md                    LCD 串口命令
    troubleshooting.md           故障排查与恢复
  development/                   开发：编译、调试、测试
    README.md
    build-and-flash.md           编译与烧录
    serial-log.md                串口日志工具
    testing.md                   测试与持续集成设计
    licenses.md                  自有源码与第三方许可
  request/                       需求：要做什么、做到什么程度
    README.md
    sony-ptpip-request.md        Sony 相机连接需求
    ui-request.md                界面显示方案
    gamepad-request.md           手柄控制方案
    gimbal-request.md            BLE 云台控制需求
    matrix-led-request.md        ATOM Matrix LED 状态显示需求
    wifi-ap-request.md           Wi-Fi 热点需求
    uart-debug-request.md        LCD 与 ATOM 的 UART 调试需求
    maintenance-request.md       网页维护页面需求（OTA、热点设置）
    improvement-request.md       项目修改清单（按优先级）
  design/                        设计：怎么实现、接口与协议
    README.md
    architecture-design.md       当前系统架构
    hardware-design.md           LCD-7B 硬件配置
    sony-ptpip-design.md         Sony PTP/IP 连接与分层设计
    ui-design.md                 界面设计
    gamepad-design.md            手柄输入处理设计
    gimbal-design.md             BLE 云台控制设计
    i2c-protocol-design.md       LCD ↔ ATOM I²C 通信协议（版本 2）
    matrix-led-design.md         ATOM Matrix LED 状态显示设计
    wifi-ap-design.md            Wi-Fi 热点配置管理设计
    uart-debug-design.md         LCD 与 ATOM 的 UART 调试控制台设计
    maintenance-design.md        网页维护页面与 OTA 设计
  tools/                         工具：脚本用法
    README.md
    capture.md                   抓包、分析和样本提取
  records/                       记录：实测数据，不代表最新实现
    README.md
    protocol-analysis.md         Sony 通信分析与取景优化记录
```

## 主题对照

同一主题的需求与设计：

| 主题 | 需求 | 设计 |
| --- | --- | --- |
| 相机连接 | [sony-ptpip-request](request/sony-ptpip-request.md) | [sony-ptpip-design](design/sony-ptpip-design.md) |
| 界面 | [ui-request](request/ui-request.md) | [ui-design](design/ui-design.md) |
| 手柄 | [gamepad-request](request/gamepad-request.md) | [gamepad-design](design/gamepad-design.md) |
| 云台 | [gimbal-request](request/gimbal-request.md) | [gimbal-design](design/gimbal-design.md) |
| 灯阵 | [matrix-led-request](request/matrix-led-request.md) | [matrix-led-design](design/matrix-led-design.md) |
| Wi-Fi 热点 | [wifi-ap-request](request/wifi-ap-request.md) | [wifi-ap-design](design/wifi-ap-design.md) |
| UART 调试 | [uart-debug-request](request/uart-debug-request.md) | [uart-debug-design](design/uart-debug-design.md) |
| 维护页面 / OTA | [maintenance-request](request/maintenance-request.md) | [maintenance-design](design/maintenance-design.md) |
| LCD ↔ ATOM 链路 | 见手柄、云台、灯阵需求 | [i2c-protocol-design](design/i2c-protocol-design.md) |
| 整体 | [improvement-request](request/improvement-request.md) | [architecture-design](design/architecture-design.md)、[hardware-design](design/hardware-design.md)、[testing](development/testing.md) |

## request：需求

描述用户可见的功能和行为，以及验收标准。

| 文档 | 内容 | 状态 |
| --- | --- | --- |
| [Sony 相机连接需求](request/sony-ptpip-request.md) | 相机发现、配对与重连、取景帧率、停止与恢复、断线恢复、属性读取、相机控制、健壮性 | 草案；部分已实现 |
| [界面显示方案](request/ui-request.md) | 连接页、LIVE、SETTINGS 三种画面；状态栏、信息显示档位、手动对焦框、对焦放大、设置菜单和提示信息 | 部分实现，标注“规划”的尚未实现 |
| [手柄控制方案](request/gamepad-request.md) | DS4 按键总表；扳机拍照录像、曝光 Mode、对焦框、变焦、云台等操作规则；Sony 协议验证表 | 已实现 Start、LB/RB；MF 且确认无变焦时 X/Y 对焦已接入，待实机验收 |
| [BLE 云台控制需求](request/gimbal-request.md) | 云台连接、左摇杆控制、L3 回中、安全停止、本地校准与限位 | 草案；目标为大疆 RS 3 Mini，BLE 协议未确认，未实现 |
| [Matrix LED 状态显示需求](request/matrix-led-request.md) | ATOM 5×5 灯阵的启动进度、LCD 与无线设备连接灯、运行期异常图案；实机测试与验收标准 | 尚未实现 |
| [Wi-Fi 热点需求](request/wifi-ap-request.md) | 固定默认值（`easycamctrl` / `00000000`）、修改 SSID / 密码 / 信道、NVS 持久化、密码显示开关、开机显示 LCD IP、恢复出厂 | 草案；未实现 |
| [UART 调试需求](request/uart-debug-request.md) | 两端命令行控制台、状态查询、LCD 模拟 ATOM 与手柄、ATOM 模拟手柄与 LCD 主机、I²C 监视与故障注入、脚本回放 | 草案；未实现 |
| [维护页面需求](request/maintenance-request.md) | 维护模式开关与 PIN 登录、设备信息、网页修改 SSID / 密码 / 信道、LCD 固件 OTA 与自动回退 | 草案；未实现，需先调整分区表 |
| [项目修改清单](request/improvement-request.md) | 审查结果按 P0–P3 列出的待办项和建议实施顺序 | 持续更新，完成后勾选 |

## design：设计

描述模块划分、接口、协议格式和硬件参数，是实现时的依据。

| 文档 | 内容 | 状态 |
| --- | --- | --- |
| [当前系统架构](design/architecture-design.md) | 硬件连接、软件模块、启动顺序、任务与核心、队列与同步、缓冲区所有权、相机连接状态机、协议版本、持久化 | 草案；描述当前实现 |
| [硬件配置](design/hardware-design.md) | LCD-7B 引脚、扩展芯片、RGB 时序、帧缓冲；像素时钟选型 | 与当前实现一致 |
| [Sony PTP/IP 连接与分层设计](design/sony-ptpip-design.md) | 当前连接流程、网络配置、GUID、初始化、控制与验证范围；后续分层接口及迁移步骤 | 当前实现与目标设计分节说明；已拆分并接入 MF/扩展属性，完整接口尚未实施 |
| [界面设计](design/ui-design.md) | 当前各画面的坐标、字号、颜色和绘制流程；规划功能的界面状态、对焦框坐标换算、菜单与提示规则 | 草案；前半部分为当前实现 |
| [手柄输入处理设计](design/gamepad-design.md) | LCD 端 `gamepad_input` 模块：按键边沿、扳机状态机、S1/S2 合成、命令优先级、安全释放 | 草案；未实现 |
| [BLE 云台控制设计](design/gimbal-design.md) | ATOM 端云台模块：摇杆曲线、回中、软限位、停止条件、连接状态、NVS 配置、厂商协议适配层 | 草案；未实现 |
| [I²C 通信协议（版本 2）](design/i2c-protocol-design.md) | 帧格式、CRC8、HELLO/POLL 命令、事件缓存与 `gap` 标志；云台在 ATOM 本地控制 | 设计方案，尚未实现；当前版本 1 见 ATOM 子项目说明 |
| [Matrix LED 状态显示设计](design/matrix-led-design.md) | 物理映射、颜色取值、状态模型与状态来源、渲染时序、模块接口、Bluetooth 双模限制 | 设计方案，尚未实现 |
| [Wi-Fi 热点设计](design/wifi-ap-design.md) | NVS 记录格式、固定默认值与随机密码生成、校验与国家码、`wifi_ap` 任务与生效流程、串口命令与手柄热点页、连接页 IP 显示、按 MAC 取相机 RSSI、两级恢复出厂 | 草案；未实现 |
| [UART 调试控制台设计](design/uart-debug-design.md) | 两端共用的行输入控制台与输出格式、单字符命令兼容、手柄动作解析与定时执行、LCD 模拟 ATOM 传输层、ATOM 模拟手柄与从机故障注入、I²C 监视、`uart_script.py` 脚本格式 | 草案；未实现 |
| [维护页面设计](design/maintenance-design.md) | OTA 分区表、维护模式状态机、PIN 与令牌登录、HTTP 接口、网页热点设置、OTA 镜像检查与上传流程、启动自检与回退 | 草案；未实现 |

## user-guide：使用手册

面向已经拿到硬件的使用者。索引见 [user-guide/README.md](user-guide/README.md)。

| 文档 | 内容 |
| --- | --- |
| [快速上手](user-guide/quick-start.md) | 烧录 LCD 与 ATOM、接线、第一次连接相机 |
| [相机连接](user-guide/camera.md) | 热点、动态 DHCP 发现、配对与恢复步骤 |
| [手柄](user-guide/controller.md) | DS4 配对、已实现按键、Mode 切换的已知限制 |
| [串口命令](user-guide/serial.md) | `j` / `s` / `S` / `p` / `u` 和画面参数 |
| [故障排查与恢复](user-guide/troubleshooting.md) | 相机、显示、ATOM、编译的常见问题；清除 NVS 和恢复出厂设置 |

## development：开发

编译、调试和测试。索引见 [development/README.md](development/README.md)。

| 文档 | 内容 | 状态 |
| --- | --- | --- |
| [编译与烧录](development/build-and-flash.md) | 两个工程的编译烧录、`idf.ps1` 参数、哪些改动需要烧录哪一端、主机测试、字体工具 | 与当前脚本一致 |
| [串口日志](development/serial-log.md) | `serial_log.py` 参数和示例、LCD 串口命令、日志关键字含义 | 与当前脚本一致 |
| [测试与持续集成设计](development/testing.md) | 测试分层、主机测试约定与计划、故障注入、30 分钟稳定性指标、CI 作业 | 草案；CI 未建立 |

## tools：工具

只写脚本用法。对应脚本位于根目录 `tools/`。索引见 [tools/README.md](tools/README.md)。

| 文档 | 内容 |
| --- | --- |
| [抓包工具](tools/capture.md) | `capture.ps1`、`analyze.py`、`extract_liveview_sample.py` |

## records：记录

按时间顺序的实测数据。记录里的“当前状态”是当时的状态。索引见 [records/README.md](records/README.md)。

| 文档 | 内容 |
| --- | --- |
| [通信分析与实测记录](records/protocol-analysis.md) | Sony 初始化顺序、属性格式、配对与取景优化的各轮实测数据 |
| [2026-10-01 抓包与设计对照](records/protocol-analysis-20261001.md) | 三轮抓包、拍照/录像/MF、扩展参数及未确认映射 |
| [2026-10-01 烧录与连接测试](records/connection-test-20261001.md) | Flash 修复、配对保存、取景、停止与重启恢复 |

## 阅读顺序

- **使用者**：快速上手 → 相机连接 → 手柄。连不上时看故障排查与恢复。
- **新成员了解项目**：根目录 README → 当前系统架构 → 编译与烧录。
- **了解产品要做什么**：Sony 相机连接需求 → 界面显示方案 → 手柄控制方案。
- **参与相机协议开发**：Sony PTP/IP 连接与分层设计的“当前实现” → 2026-10-01 抓包与设计对照 → 后续分层设计。
- **参与 ATOM / 手柄链路开发**：I²C 通信协议 → 手柄输入处理设计 → Matrix LED 状态显示需求与设计 → BLE 云台控制需求与设计 → UART 调试需求与设计。
- **参与热点配置与维护功能开发**：Wi-Fi 热点需求与设计 → 维护页面需求与设计。
- **遇到问题**：故障排查与恢复 → 串口日志。
- **挑选下一步工作**：项目修改清单。

## 约定

- `request/` 与 `design/` 的文件名统一为 `<主题>-<类别>.md`：主题用小写英文和连字符，同一主题使用相同主题名，例如 `matrix-led-request.md` 与 `matrix-led-design.md`。其余目录以目录名表示类别，文件名不再重复后缀。
- 使用手册写操作步骤；开发文档写编译、调试和测试；需求文档写“做什么”和验收标准；设计文档写“怎么做”和接口；`tools/` 只写脚本用法；实测数据写入 `records/`。
- 尚未经过评审的文档在开头标注“草案”。
- 协议事实以抓包和实机结果为准，尚无证据的内容标注“待验证”或“待抓包确认”。
- 文档之间使用相对路径链接；移动文档时同步更新根目录 README、ATOM 子项目说明和本文中的链接。
- 涉及通信记录的内容遵守下文[通信记录的公开范围](#通信记录的公开范围)。

## 免责声明

- 本项目是个人开发的非官方项目，与 Sony、大疆、微雪、M5Stack、八位堂等厂商无关，未获其授权或认可。文中的公司名、产品名和商标归各自权利人所有，仅用于说明兼容对象。
- 协议信息来自对开发者自有设备之间通信的观察与实机测试，并参考 libgphoto2 等公开资料，不包含厂商 SDK、保密资料或固件代码。内容可能不完整或有误，标注“待验证”“待抓包确认”的部分尤其如此。
- 本项目不提供、也不接受绕过相机或云台配对、认证及其他访问控制的方法、密钥或代码。连接相机须按正常流程在相机上确认配对。
- 软件和文档按现状提供，不作任何保证。向相机或云台发送非官方命令可能导致设备工作异常、设置被改动、数据丢失或失去厂商保修，由使用者自行承担风险。
- 使用者须自行遵守所在地法律法规及相关软件、服务的使用条款。本文档不构成法律意见。

## 通信记录的公开范围

通信记录指抓包文件（`.pcapng`）、由其生成的 `.ptpip.csv`、导出的取景帧，以及串口日志中的原始报文。

只在本地保存，不提交到仓库，也不在 Issue、PR 或其他公开渠道发布：

- 原始 `.pcapng` 和未脱敏的 `.ptpip.csv`、串口日志，统一放在已被 `.gitignore` 忽略的 `captures/`。
- 设备标识：相机与云台序列号、GUID、MAC 地址、蓝牙地址、主机名。
- 凭证与配对信息：Wi-Fi SSID 和密码、配对记录、认证或会话令牌、密钥。
- 取景画面中的人物、车牌、文件、住所内部等可识别内容。
- 同一网络中其他设备的通信，包括 mDNS、SSDP 广播里的设备名和服务信息。
- 厂商 SDK、开发者计划或其他受保密约定限制的资料内容。

可以公开：

- 写进文档的协议事实：操作码、属性码、参数含义、数据格式、时序和统计结果。
- 私有网段 IP 地址（如 `192.168.x.x`），以及抓包文件名、TCP 流号和事务号等定位信息。
- 按[测试样本](development/testing.md#测试样本)约定导出的脱敏样本：只截取所需事务，设备标识替换为固定值，取景帧只使用纯色板、测试卡等不含可识别内容的画面。

抓包时：

- 在家用网络或本项目的专用热点中进行，不在公司、学校、公共 Wi-Fi 等共享网络中抓包。
- 只抓本机与自有设备之间的通信，不使用监听模式（monitor mode）截取他人的无线通信。

提交前检查 `git status`，确认没有通信记录。如果误提交或推送，从 Git 历史中彻底删除（如使用 `git filter-repo`）；已推送到公开仓库的内容视为已泄露，相关的配对记录和密码应在设备上重置。
