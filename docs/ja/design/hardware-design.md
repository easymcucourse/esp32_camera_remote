# ハードウェア構成

[English](../../en/design/hardware-design.md) · [简体中文](../../design/hardware-design.md) · **日本語**

Waveshare **ESP32-S3-Touch-LCD-7B**、flash16MiB/Octal PSRAM8MiBです。touch未初期化。LCD-7 CH422G設定は流用できません。pinは `components/board_7b/board_7b_backend.c` にあります。

| 信号 | GPIO / 値 |
| --- | --- |
| I²C SDA/SCL | 8/9、100kHz、expander0x24とATOM0x42。 |
| expander mode/output/PWM | register0x02/0x03/0x05。 |
| power/backlight | EXIO6/EXIO2。 |
| HSYNC/VSYNC/DE/PCLK | 46/3/5/7。 |
| RGB D0–D15 | 14,38,18,17,10,39,0,45,48,47,21,1,2,42,41,40。 |
| HSync/HBP/HFP | 162/152/48。 |
| VSync/VBP/VFP | 45/13/3。 |
| pixel clock | 18MHz、負edge。 |
| frame | PSRAM1024×600×2byteを二個。 |
| DMA bounce | 10行を二個、内部40960byte。 |

backlight off→power→buffer/RGB→backlight on。LIVEは(0,12)の1024×576。scan周波数とCamera FPSは別です。旧30/40MHzは画面障害があり、18MHz二bufferは当該機で目視確認済みです。

ATOM Matrixは従来ESP32、S3/C3ではありません。LCD SDA8→ATOM26、SCL9→32、GND、3.3V pullupです。別USB供電ではGrove5Vを接続しません。LED27、active-low button39。0x42のv2 HELLO/POLL、LEDは本体所有です。[I²C](i2c-protocol-design.md)と[Matrix](matrix-led-design.md)を参照してください。

COM8/COM6は過去のlocal例です。raw flash backupはignoredに保持し、app更新前に実OTA slotを確認します。buildだけでは別board/温度/長期の合格を証明しません。
