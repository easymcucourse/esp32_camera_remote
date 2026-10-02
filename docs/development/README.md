# 开发文档

编译、烧录、串口记录和测试约定。模块怎么划分、接口和协议见 [设计](../design/README.md)，要做什么见 [需求](../request/README.md)。

| 文档 | 内容 |
| --- | --- |
| [编译与烧录](build-and-flash.md) | 两个工程的编译烧录、`idf.ps1`、改动要烧哪一端、主机测试、字体工具 |
| [串口日志](serial-log.md) | `serial_log.py` 参数和示例、LCD 串口命令、日志关键字 |
| [测试](testing.md) | 测试分层、主机测试约定、故障注入、稳定性指标、CI |
| [全需求实施状态](implementation-status.md) | 除云台外全部需求的实施范围、现有证据和未完成验收 |
| [字体资源与许可](../../components/board_7b/fonts/README.md) | 字体来源、裁剪方法及随仓库提供的许可证 |

当前实现的模块和启动顺序见 [系统架构](../design/architecture-design.md)，引脚和时序见 [硬件配置](../design/hardware-design.md)。
