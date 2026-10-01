# 快速上手

需要 Waveshare ESP32-S3-Touch-LCD-7B、M5Stack ATOM Matrix、Sony ZV-E10 和 DualShock 4。LCD 工程目标为 ESP32-S3，ATOM 工程目标为经典 ESP32。两块板分别 USB 供电。

完整编译说明、包装脚本和“改哪一端要烧哪一端”见 [编译与烧录](../development/build-and-flash.md)。

## 烧录 LCD

在已激活 ESP-IDF 5.5.1 的终端，于仓库根目录执行。把 `COM8` 换成实际串口。

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

监视器按 `Ctrl+]` 退出。常规烧录会保留 NVS 里的相机配对身份；擦除整片 Flash 后需要重新配对。

## 烧录 ATOM

```powershell
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash
```

本机 LCD 为 COM8、ATOM 为 COM6。ATOM 使用 115200 波特率烧录。

## 接线

LCD GPIO8（SDA）、GPIO9（SCL）、GND 连接 ATOM GPIO26、GPIO32、GND。两端分别 USB 供电时不要连接 Grove 5V。

## 连接相机

1. 板子启动后显示连接页。当前固件热点为 SSID **esp32camap**、密码 **00000000**，信道 6。
2. 相机连接该热点，启用 PC 远程，选择 Wi-Fi 接入点连接。
3. 首次连接若提示确认，允许 **ESP32-Camera-Remote**。
4. 第一帧解码成功后进入连续取景。失败后会回到状态页并自动重试。

配对、动态地址发现及更换相机步骤见 [相机连接](camera.md)。按键见 [手柄](controller.md)。连不上时见 [故障排查与恢复](troubleshooting.md)。
