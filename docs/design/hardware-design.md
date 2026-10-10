# ESP32-S3-Touch-LCD-7B 硬件配置

[English](../en/design/hardware-design.md) · **简体中文** · [日本語](../ja/design/hardware-design.md)

目标为 Waveshare ESP32-S3-Touch-LCD-7B，当前不初始化触摸设备。参数对应 `components/board_7b/board_7b_backend.c`，不要套用 LCD-7 的 CH422G 驱动。

| 信号 | GPIO / 配置 |
| --- | --- |
| I²C SDA / SCL | 8 / 9，100kHz |
| 扩展芯片 | 地址 0x24，模式寄存器 0x02、输出 0x03、PWM 0x05 |
| LCD 电源 / 背光 | EXIO6 / EXIO2 |
| HSYNC / VSYNC / DE / PCLK | 46 / 3 / 5 / 7 |
| RGB D0–D15 | 14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40 |
| HSync / HBP / HFP | 162 / 152 / 48 |
| VSync / VBP / VFP | 45 / 13 / 3 |
| 像素时钟 | 18MHz，负沿有效 |
| 帧缓冲 | 两个 PSRAM 缓冲，每个 1024×600×2 字节 |
| DMA bounce buffer | 每个 10 行，两块共 40KiB 内部 RAM；2026-10-06 启动内存修复 |

启动先关背光，配置屏幕电源，初始化帧缓冲及 RGB 外设，再开背光。实时取景为 1024×576，居中坐标 `(0,12)`。

ATOM Matrix 作为 Grove I²C 从系统，地址 `0x42`，复用 LCD 的 GPIO8/9 主总线。ATOM SDA/SCL 为 GPIO26/32，两端共地。LCD 通过 v2 HELLO / POLL 读取状态、输入快照、缓存事件和板载按键次数；灯阵由 ATOM 独立状态任务控制，不再由 LCD 设置颜色。接线及协议见 [ATOM 子项目说明](../../m5_atom_matrix/README.md)，图案见 [灯阵设计](matrix-led-design.md)。

30MHz 像素时钟在连续取景时曾出现扫描起点上下跳动；40MHz 实测显示异常。18MHz 配合双帧缓冲已由用户确认稳定。LCD 扫描频率不同于右上角的画面更新 FPS。

本机串口 COM8（CH343）。首次移植前的完整 Flash 备份为 `backups/com8-before-idf.bin`，大小 16,777,216 字节，SHA256：`E66B2E21643C52492B89FA029845FBEAC4A34B3FDA74E55994F80EC4AC8C8F72`。备份仅保存在本地，不随仓库发布。
