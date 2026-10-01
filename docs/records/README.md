# 记录

按时间保留的抓包分析和实机测试。文中的“当前状态”是当时的状态，最新行为以 [相机连接设计的当前实现](../design/sony-ptpip-design.md#当前实现连接与运行)、根目录 README 和 [系统架构](../design/architecture-design.md) 为准。

| 文档 | 内容 |
| --- | --- |
| [通信分析与实测记录](protocol-analysis.md) | Sony 初始化顺序、属性格式、配对和取景优化各轮数据 |
| [2026-10-01 抓包与设计对照](protocol-analysis-20261001.md) | 三轮抓包、S1/S2/录像/MF、截图参数、取景拒绝及 JPEG 边界证据 |
| [2026-10-01 烧录与连接测试](connection-test-20261001.md) | 固件校验、启动、停止／恢复、重复启动与 DHCP 前置条件 |

抓包命令见 [抓包工具](../tools/capture.md)。原始 `.pcapng` 和未脱敏日志不提交，范围见 [通信记录的公开范围](../README.md#通信记录的公开范围)。
