# 设计

描述模块划分、接口、协议格式和硬件参数。对应的验收标准在 [需求](../request/README.md)。测试约定在 [开发文档](../development/testing.md)。

| 文档 | 内容 |
| --- | --- |
| [当前系统架构](architecture-design.md) | 模块、任务、状态机、持久化 |
| [硬件配置](hardware-design.md) | LCD-7B 引脚、时序、帧缓冲 |
| [Sony PTP/IP 客户端分层设计](sony-ptpip-design.md) | 当前连接流程、实现边界、协议事实及后续目标分层 |
| [界面设计](ui-design.md) | 坐标、字号、绘制流程 |
| [手柄输入处理设计](gamepad-design.md) | LCD 端按键到命令 |
| [BLE 云台控制设计](gimbal-design.md) | ATOM 端云台模块 |
| [I²C 通信协议（版本 2）](i2c-protocol-design.md) | LCD 与 ATOM 的下一版帧格式 |
| [Matrix LED 状态显示设计](matrix-led-design.md) | 灯阵状态模型和接口 |
| [Wi-Fi 热点设计](wifi-ap-design.md) | NVS、校验和生效流程 |
| [UART 调试控制台设计](uart-debug-design.md) | 命令行、模拟和脚本 |
| [维护页面设计](maintenance-design.md) | OTA 分区、登录和 HTTP 接口 |
