# 文档总览

[English](README.md) · **简体中文** · [日本語](ja/README.md)

当前行为与验证边界以 [2026-10-10当前状态](development/current-status.md) 和源码为准；[根目录中文说明](../README.zh-CN.md)提供概况。需求描述完整目标，设计区分生产实现和后续规划；历史记录保留日期，不重写为当前通过。

## 适配目标

LCD-7B、ATOM Matrix、Sony ZV-E10、DJI RS 3 Mini和Classic DS4。Ultimate 2 BLE采用限定HID描述，实体映射/相机效果待验。Classic/BLE是链路分类，Sony/Xbox兼容是报告布局，两者独立。

## 阅读顺序

1. [快速上手](user-guide/quick-start.md)：接线、构建、相机和启动页维护。
2. [相机](user-guide/camera.md)、[手柄/云台](user-guide/controller.md)、[串口](user-guide/serial.md)、[排错](user-guide/troubleshooting.md)。
3. [开发目录](development/README.md)：构建/日志/测试、当前验证和模块拆分状态。
4. [需求目录](request/README.md)和 [设计目录](design/README.md)：接口与未完成验收要求。
5. [工具目录](tools/README.md)和 [历史记录](records/README.md)。

## 当前主题

| 主题 | 使用/需求 | 设计与状态 |
| --- | --- | --- |
| 相机/PTP/IP | [相机](user-guide/camera.md)、[需求](request/sony-ptpip-request.md) | [PTP/IP](design/sony-ptpip-design.md)、[参数菜单](design/camera-menu-design.md) |
| UI | [需求](request/ui-request.md) | [布局](design/ui-design.md)，部分对焦框/放大仍为目标 |
| 手柄 | [使用](user-guide/controller.md)、[需求](request/gamepad-request.md) | [输入owner](design/gamepad-design.md)，当前镜头声明POWER_ZOOM |
| 云台 | [需求](request/gimbal-request.md) | [完整目标](design/gimbal-design.md)、[RS 3 Mini生产实现](design/rs3-mini-protocol.md)，基础运动/关机恢复已确认，零位/限位未实现 |
| Matrix | [需求](request/matrix-led-request.md) | [状态/电量](design/matrix-led-design.md)，真实数据与视觉独立验收 |
| 网络/I²C | [热点需求](request/wifi-ap-request.md) | [热点owner](design/wifi-ap-design.md)、[I²C v2](design/i2c-protocol-design.md) |
| 维护/OTA | [启动页操作](user-guide/quick-start.md#维护网页)、[需求](request/maintenance-request.md) | [独占Web](design/maintenance-design.md)，无PIN/登录，成功后重启 |
| UART/模拟 | [手册](user-guide/serial.md)、[需求](request/uart-debug-request.md) | [控制台](design/uart-debug-design.md)，Debug模拟/故障，Release排除 |
| 架构 | [验收清单](development/module-split-checklist.md) | [系统](design/architecture-design.md)、[消息契约](design/module-message-contracts.md)、[依赖](design/module-dependency-graph.md)、[资源owner](design/module-resource-ownership.md) |
| 性能 | [阶段计划](design/liveview-memory-fps-plan.md) | [当前状态](development/current-status.md)，原优化门禁未全部达到 |

## 文档约定

使用手册写当前操作；开发文档写构建/调试/测试；需求保留完整验收目标；设计标明生产路径与规划；records按日期保留实测。所有“通过”注明软件、构建、烧录或实机范围。三语按英、中、日导航，命令/符号/协议字节保持相同。

## 免责声明

本项目非官方，厂商名称仅表示兼容对象，不表示授权。公开协议参考及自有设备观察可能不完整；按设备正常流程配对/激活。软件与文档按现状提供，未验证项不承诺效果。

## 通信记录的公开范围

原始抓包、未脱敏UART、设备身份、真实凭证/绑定/token以及包含可识别内容的画面只保存在Git忽略目录；不发布其他设备流量或受保密限制资料。只在私人测试网络抓自有设备。

可以发布操作码/属性码、格式、时序/统计、证据文件名、事务号与固定替换身份的裁剪fixture。图像样本使用测试卡；规则见 [测试样本](development/testing.md#测试样本)。发布前检查暂存内容。
