# Hardware configuration

**English** · [简体中文](../../design/hardware-design.md) · [日本語](../../ja/design/hardware-design.md)

Use Waveshare **ESP32-S3-Touch-LCD-7B**, 16MiB flash/8MiB Octal PSRAM. Touch is not initialized. LCD-7 CH422G configuration is not interchangeable. Current pins belong to `components/board_7b/board_7b_backend.c`.

| Signal | GPIO / value |
| --- | --- |
| I²C SDA/SCL | 8/9,100kHz; expander0x24 and ATOM0x42 share the bus. |
| Expander mode/output/PWM | registers0x02/0x03/0x05. |
| LCD power/backlight | EXIO6/EXIO2. |
| HSYNC/VSYNC/DE/PCLK | 46/3/5/7. |
| RGB D0–D15 | 14,38,18,17,10,39,0,45,48,47,21,1,2,42,41,40. |
| HSync/HBP/HFP | 162/152/48. |
| VSync/VBP/VFP | 45/13/3. |
| Pixel clock | 18MHz, negative edge. |
| Framebuffers | two 1024×600×2-byte PSRAM buffers. |
| DMA bounce | two 10-row buffers, total 40960 internal bytes. |

Disable backlight, configure power, initialize buffers/RGB, then enable backlight. LIVE is 1024×576 at(0,12). Scan refresh differs from camera update FPS. Historical30/40MHz pixel-clock experiments caused visual faults;18MHz/double buffering was user-confirmed on the tested board.

ATOM Matrix is classic ESP32, not S3/C3. Connect LCD SDA8→ATOM26, SCL9→32 and GND; pull up to3.3V. With separate USB supplies, do not connect Grove5V. ATOM LED isGPIO27 and active-low buttonGPIO39. Address0x42 uses v2 HELLO/POLL; LED colors are locally owned. See [I²C](i2c-protocol-design.md) and [Matrix](matrix-led-design.md).

COM8/COM6 are local historical examples, not portable board identities. Keep raw full-flash backups ignored and verify the current active OTA slot before application updates. New build validation does not prove new-board timing, temperature or long stability.
