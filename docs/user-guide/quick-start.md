# 快速上手

[English](../en/user-guide/quick-start.md) · **简体中文** · [日本語](../ja/user-guide/quick-start.md)

需要 Waveshare ESP32-S3-Touch-LCD-7B、M5Stack ATOM Matrix、Sony ZV-E10 和 DualShock 4。LCD 工程目标为 ESP32-S3，ATOM 工程目标为经典 ESP32。两块板分别 USB 供电。

完整编译说明、包装脚本和“改哪一端要烧哪一端”见 [编译与烧录](../development/build-and-flash.md)。

## 烧录 LCD

在已激活 ESP-IDF 5.5.1 的终端，于仓库根目录执行。把 `COM8` 换成实际串口。

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

监视器按Ctrl+]退出。未擦除NVS时配对身份保留；完整USB flash会写分区/初始OTA数据，可能改变已部署设备的启动槽。已有LCD优先Web OTA，或先核对活动槽再应用更新；整片擦除会删除身份并需重配。

## 烧录 ATOM

```powershell
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash
```

COM8/COM6是历史端口示例，执行前确认LCD/ATOM的当前实际串口，不能按示例猜设备。ATOM 使用 115200 波特率烧录。当前 I²C 协议为 v2，从 v1 升级时必须同时烧录 LCD 和 ATOM。

## 接线

LCD GPIO8（SDA）、GPIO9（SCL）、GND 连接 ATOM GPIO26、GPIO32、GND。两端分别 USB 供电时不要连接 Grove 5V。

## 连接相机

1. 板子启动后显示连接页。无保存配置时，热点为 **easycamctrl**、密码 **00000000**、信道 6；已配置过时以屏幕或 `wifi show` 为准。
2. 相机连接该热点，启用 PC 远程，选择 Wi-Fi 接入点连接。
3. 首次连接若提示确认，允许 **ESP32-Camera-Remote**。
4. 第一帧解码成功后进入连续取景。失败后会回到状态页并自动重试。

配对、动态地址发现及更换相机步骤见 [相机连接](camera.md)。按键见 [手柄](controller.md)。连不上时见 [故障排查与恢复](troubleshooting.md)。

## 维护网页

需要修改热点、显示偏好、恢复出厂或OTA时，重启LCD后在启动连接页连接设备热点，用手机/电脑浏览器打开屏幕显示的IP（默认 http://192.168.4.1/）。应在设备进入普通模式前访问；普通模式已关闭端口80，只能重新启动再尝试。

首次请求触发独占切换并重定向到维护首页。LCD排空普通服务后只显示MAINTENANCE；相机、手柄和串口不能继续控制，也不能退出回取景。网页无PIN或登录，任何加入热点的客户端都有全部权限。页面设置保存、退出维护、重启和OTA成功都重启LCD；手机可能需要重新连接新热点。

热点重置保留相机绑定；全部重置另清LCD相机绑定和显示偏好，ATOM绑定保留。重置失败可能部分改变记录，应查看页面错误与启动日志。固件上传只选LCD应用bin，不能选择ATOM、bootloader或merged镜像。首次切换双OTA分区仍需USB写入分区表，见 [构建说明](../development/build-and-flash.md)。当前拆分版浏览器、LCD及OTA实机效果尚待验收。
