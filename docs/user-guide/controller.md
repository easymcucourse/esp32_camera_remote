# 手柄

[English](../en/user-guide/controller.md) · **简体中文** · [日本語](../ja/user-guide/controller.md)

M5Stack ATOM Matrix 通过经典蓝牙接收 DualShock 4，再经 I²C v2 向 LCD 上报输入。ATOM还包含BLE手柄客户端和Ultimate 2报告解析；其已测范围见 [实施状态](../development/implementation-status.md)，不能泛称全部Xbox兼容手柄可用。

## 接线与配对

LCD GPIO8（SDA）、GPIO9（SCL）、GND 接 ATOM GPIO26、GPIO32、GND。总线 100 kHz、3.3 V 上拉、地址 `0x42`；分别 USB 供电时不要连接 Grove 5V。升级 v1 至 v2 必须同时烧录两端；以后只修改界面或相机控制可以仅更新 LCD。

首次配对按住 SHARE + PS，直到灯条快闪；已保存过配对时按 PS 唤醒。Matrix 底行左灯表示 LCD 链路，经典蓝牙灯表示 DS4 连接过程；手柄就绪以日志 `input ready` 与有效报告为准。

## 当前按键

本文 X / Y 按 Xbox 布局命名，分别对应 DS4 方块 / 三角。

| 输入 | 功能 |
| --- | --- |
| Start / Options | LIVE 与 SETTINGS 切换 |
| L1 / R1 | Wide / Tele 变焦；确认非电动变焦镜头且 MF 时近 / 远对焦 |
| Y / 三角 | 循环切换下一个曝光 Mode，按住不重复 |
| X / 方块 | 循环切换下一个对焦模式，按住不重复 |
| RT / R2 | 半压 S1，全压 S2；退出全压释放 S2，完全松开释放 S1 |
| LT / L2 | 半压无动作；每次进入全压，根据相机确认状态请求开始 / 停止录像；松开不改变对焦或录像状态 |
| 方向键上下 | SETTINGS 中移动七项参数、MORE及Wi-Fi信息光标，首尾循环 |
| 方向键左右 | SETTINGS中修改光标参数；枚举首尾循环，EV右增大/左减小 |
| A / DS4 叉 | SETTINGS 中确认；MORE进入扩展参数，Wi-Fi仅显示信息 |
| B / DS4 圈 | MORE返回主参数页；无热点编辑器 |
| 触摸板按下 | 当前Input服务忽略；显示档位通过启动维护Web保存后重启加载 |
| 左摇杆 / L3 | ATOM本地RS 3 Mini Pan/Tilt / 原生回中 |

相机控制在 LIVE / SETTINGS 使用同样的肩键、X/Y 与扳机映射。两肩键同时按住会停止操作，必须都松开再按；X 取消当前肩键操作。方向键按住 400 ms 后每 150 ms 重复，迟到的轮询不补发历史重复。切换界面、重连或事件缺失后，已按住的方向键必须先松开。

## 参数与确认

菜单项为 Shutter、F-Number、ISO、EV、WB、Focus、Metering。光标高亮；缺少能力或只读的项灰显。相机枚举决定可选值，连续输入围绕最新目标合并，等待旧目标回读确认后再发送后续目标。

快门和光圈有可用枚举时选择邻项；没有枚举但有有效可写当前值时走单步控制，每次等待当前值变化后再继续，拒绝或超时清除剩余步数。这两个单步命令尚需 ZV-E10 实机协议验证。未取到有效当前值时不发送。

底部 `TO ...` 表示待确认目标；实际参数行保持相机回报值。`PENDING` 持续到回读确认，`APPLIED`、`REJECTED`、`TIMEOUT` 短暂显示。请求被接受不等于参数已生效，部分曝光 Mode 更新约需 5 秒。

<a id="热点页"></a>

## 热点设置

LCD 的 Wi-Fi 行只显示网络信息，不再打开编辑器。热点 SSID、密码、信道及密码显示偏好通过启动时访问维护网页修改，保存成功后设备重启。恢复出厂也只在网页操作；全部重置后需要重新确认相机配对，ATOM 绑定保留。当前拆分版实机效果仍待验收。

## 验证范围与限制

新映射已烧录过；当前参数与消息路径已接入主机回归，构建 / 烧录与相机效果以 [实施状态](../development/implementation-status.md) 为准。

真实镜头类型识别尚未完成；当前按用户确认的电动变焦镜头使用POWER_ZOOM，不启用非电动变焦MF替代。扳机必须在连接后先完全松开才允许拍摄。录像状态未知时不猜测开始 / 停止目标。断开、事件缺失或会话变化会清理旧输入，重连不重放旧命令。

Select 对焦框、右摇杆定位及 R3 回中仍待实现。RS 3 Mini 已新增 ATOM 本地控制：真实 DS4 左摇杆 Pan/Tilt、L3 原生回中，连接后先回中心才可运动；松杆、断线及输入超时请求停止。使用与实测边界见 [RS 3 Mini 实现](../design/rs3-mini-protocol.md)。完整需求见 [手柄控制方案](../request/gamepad-request.md)，链路实测见 [I²C v2 记录](../records/i2c-v2-test-20261002.md)。

云台先完成官方激活/平衡/解锁并断开Ronin App；无保存目标时选唯一Mini，多台不选；已有绑定需ATOM串口 `gimbal pair` 换机，不自动替换。 `gimbal speed pan 120` / `gimbal speed tilt 240` 独立调轴， `gimbal speed 120` 同时设置，20..400为协议幅度而非角速度，NVS保存。当前用户确认120/240摇杆/L3和云台关机恢复正常；任意零位/软限位/板载菜单未实现，精确停止/取消/30分钟仍待验。UART SIM不会驱动物理云台。
