# RS 3 Mini BLE 实现与验证边界

[English](../en/design/rs3-mini-protocol.md) · **简体中文** · [日本語](../ja/design/rs3-mini-protocol.md)

日期：2026-10-10。此次用户请求增加 RS 3 Mini 控制，取代之前任务对云台运动的排除。控制运行于 ATOM，LCD 只接收 I²C v2 的状态；运动不依赖 LCD。云台故障显示的 LCD 候选已构建，未烧录。

## 协议证据

- [rs3mini-bridge](https://github.com/b-rutledge-code/rs3mini-bridge/tree/b64315754bb47be76d862ae8860764267215a7cf)：作者报告在 RS 3 Mini 上使用。`move-presets.json` 提供原始中立和运动报文；`lib/duml.js` 提供 CRC、序号；`lib/session-setup.js`、`lib/telemetry.js` 提供回中、轮询与电量字段。本项目尚需独立实机验证。
- [dji_rs3_control](https://github.com/jdesbonnet/dji_rs3_control/tree/689884f2249e721c5b65c9c74804caddc1e7a68c)：作者测试的是 RS 3，作为轴布局、通知、CCCD 与 DUML 的交叉证据，不能直接作为 Mini 的验收。
- 官方 [RS 3 Mini 手册](https://dl.djicdn.com/downloads/DJI_RS_3_Mini/UM/20230118/DJI_RS_3_Mini_User_Manual_V1.0_en.pdf) 用于设备操作；上述 BLE 运动命令不是官方 SDK。

实现代码重新编写；公开项目的 MIT 许可与来源保存在 `third_party/rs3-protocol/`。不包含实机蓝牙地址、密码或密钥。

## 线格式

服务 `FFF0`；运动写入 `FFF5`（Write Without Response）；接收 `FFF4`（Notify）。动态发现特征与 `2902` CCCD，不固定 handle。协商 MTU 185；低于 25 拒绝控制，防止 22 字节摇杆帧截断。

本台实测 FFF0 范围16..23、FFF5 handle21/property0x0c（WRITE/WRITE_NR）、FFF4 handle18/property0x10（仅Notify）。必须向动态发现的2902描述符写`01 00`开启通知；不能要求FFF4特征本身可写。先前基于Mini参考客户端增加的强制FFF4写属性校验，在本台造成control service unavailable，现已纠正。

仅在FFF4实际声明WRITE或WRITE_NR时，才额外向特征写`01 00`并检查完成状态；这种额外会话写的设备分支尚未实机验证。本台Notify-only路径以CCCD写入成功初始化会话，等待300ms再提交中立/轮询，并要求收到有效DUML才能发布Connected。特征属性由设备实际发现，不从名称猜测。

DUML：`55 / length低 / version+length高 / CRC8 / sender / receiver / seq LE16 / flags / set / id / payload / CRC16 LE16`。CRC8 初值 `0x77`、反射多项式 `0x8c`；CRC16 初值 `0x3692`、反射多项式 `0x8408`。接收采用 256 字节有界重组，验证长度、版本和两级 CRC，处理分片、拼接与损坏后的重新同步。

| 操作 | receiver / flags / set / id | payload |
| --- | --- | --- |
| 摇杆 / 停止 | `04 / 40 / 04 / 01` | Tilt、Roll、Pan 三个 LE16；中立均 1024；尾部 `00 00 02` |
| 原生回中 | `04 / 40 / 04 / 4c` | `fe 01` |
| 当前状态轮询 | `04 / 00 / 04 / 12` | Mini参考builder提供的20字节轮询，1Hz；参考raw capture为E5，见下方实机增量 |
| 电量接收 | sender `e5` → receiver `02`，`0d / 02` | 仅接受恰好 21 字节 payload，末字节 0–100；15 秒无更新清为未知 |

## 模块与行为

`ble_clients` 唯一注册 GAP/GATTC 回调。扫描使用原子 owner：手柄与云台串行扫描，各自只收到自己的扫描结果；鉴权和 GATT 回调交给各模块按 app ID/interface/peer 过滤。

`gimbal_link` 在独立 20ms 任务处理有界事件队列、发现、订阅、NVS、写入和重连。首次只自动选择一台广告名符合 RS3 Mini 的设备；多台不选。控制通道、通知订阅、首次中立提交及来自云台的有效 DUML 帧全部满足后发布 Connected。此状态证明协议传输已建立，不代表已看到物理运动。成功后保存目标地址，后续只匹配该目标。`gimbal pair` 清除应用层目标并重新选择，不清除手柄配对。

已保存目标按广播地址匹配，即使该包没有名称或扫描响应也可重连；首次配对仍要求有效Mini名称。GAP任务使用临界区保护的目标快照，worker再次验证当前目标；不会因为另一台Mini带有同类名称就替换保存设备。名称通常可能随扫描响应到达，见[Bleak扫描说明](https://bleak.readthedocs.io/en/latest/api/scanner.html)。

`gimbal_tx` 限制已被蓝牙栈受理、尚未收到 WRITE_CHAR 完成事件的写入：普通控制/轮询只允许一笔，另保留一笔中立命令用于停止；中立已经在途时不再重复排队。繁忙时保留控制器原状态，下个周期按最新 DS4 快照重新计算，不缓存过期目标。最早在途写入 500ms 无完成事件时关闭链路并置故障。移动中按 L3 时先提交中立，保留按下沿，待在途写入完成再提交回中。限速仍是手动运动 200ms 一次，停止不受此间隔限制。

`gimbal_control` 只消费真实 Classic DS4（`ds4_host_get_gimbal`），不依赖 LCD 选择的输入来源或 I²C；UART SIM 开启时禁止驱动物理云台。报告时间来自同一 esp_timer 时钟。重连/DS4 连接代数变化先停止，待新鲜输入、松开 L3 且摇杆回中心后才解除门禁。

左摇杆 X→Pan；Y 默认反向→Tilt；13/127 死区、剩余范围二次曲线，输出为整数控制偏移。默认最大偏移 120，可调 20–400；这是控制协议单位，不能称为度/秒。Roll 始终中立。手动运动 200ms 一次，保留最新目标；松杆停止绕过间隔。断开/报告年龄≥200ms 立即请求中立。L3 按下沿发送云台原生回中，按住时忽略摇杆偏移；释放后推杆或 5 秒超时先请求中立，再恢复手动。

写入返回失败或异步失败时禁止运动并重试中立；连续五次失败、初始化超时、有效通知静默超过 5 秒、事件队列溢出关闭链路并置故障。关闭等待栈的终态，不遗忘尚未结束的 open 请求。Matrix 显示无线故障图案；I²C v2 `POLL[5]` 上报状态，故障 bit3 上报云台故障，HELLO feature bit2 声明云台支持。连接恢复后清故障。

GATT CLOSE与DISCONNECT均处理终态；主动关闭同时释放虚拟GATT客户端，并对该云台peer请求物理断开，使其可重新广播。status的phase按bit0..7依次为registered/scanning/opening/opened/subscribed/session_ready/closing/writable；scan_named为Mini名称匹配广播数，scan_saved为通过保存目标过滤的广播数。

LCD 输入 provider 读取 `POLL[7]` 的 `ATOM_FAULT_GIMBAL`（bit3），经 typed input state 传给 UI。连接页显示红色 `Gimbal: Fault`；取景/设置页显示红色 `GIMBAL FAULT`，隐藏普通信息时仍保留此提示。新正常状态清除提示；ATOM 离线/重启、协议不匹配及 UI 退出时清除旧故障。此显示路径已构建及主机验证，LCD 尚未烧录和视觉验收。

Bluedroid 会向多个 GATT app 广播物理断开事件；BLE HID 按自身 connection/peer 过滤，避免云台断开误清手柄状态。云台链路关闭或重置时清除旧电量，重新收到有效电量后显示。

## 本地命令

ATOM UART：`gimbal status`、`on`、`off`、`pair`、`stop`、`speed 20..400`、`speed pan 20..400`、`speed tilt 20..400`、`invert 0|1`、`calibrate`。原 `speed <n>` 同时设置两轴；带pan/tilt只设置对应轴。status的 `span` 为Pan，`tilt_span` 为Tilt。均为协议偏移幅度，不是每秒角度；死区/二次曲线/5Hz和停止门禁保持不变。

启用、目标、Pan速度、Y方向、漂移偏移仍保存在 `gimbal/cfg`（version1），独立Tilt速度保存在同一namespace的 `tilt_span` uint16键，不改变旧blob布局或设备绑定。旧设备没有此键时继承旧共享速度；工厂默认两轴120。用户2026-10-10反馈上下太慢后，本机已配置Pan120/Tilt240并跨RTS重启保持；激活后用户确认“摇杆正常”，保留该设置，见接入记录。校准需新鲜真实 DS4 输入且两轴偏移≤32，使用当前中心读数。命令返回queued表示入队，NVS错误另行记录；stop不持久化。

重复 `on` 保留当前 Connected 状态且不重复写 NVS；status 额外报告 armed、真实 DS4/报告年龄、有效 RX 帧/年龄、在途写入数及运动/中立/回中提交计数。`*_queued` 是栈受理数，不代表物理动作或成功执行。调试 `log gimbal debug` 可查看运动TX完整DUML与姿态类包payload；控制命令回复（01/4c/0f）记录无设备身份的header/payload。结束后恢复 `log gimbal info`。

20:44实机增量：steady `04/12` 改发送给端点4，flags0、20字节payload `1051010000000c00005000f1036624c01d00001c`，每秒一次。这采用Mini参考 `session-setup.js` 的builder行为；该参考的 `capture-samples.json` 原始抓包仍指向E5，两者并不相同，不能把端点4写成原始抓包事实。当前实机E5版本可收遥测但用户多次无动作，端点4版本用户确认摇杆和L3都生效，且L3收到同sequence的 `04/4c flags80 payload00` 回复。保留此已验配置；具体控制权限/心跳语义尚未完全解析，不能推断所有固件兼容。

## 尚待验收或实现

- 本台基础摇杆/L3、Pan120/Tilt240和云台断电自动重连已有用户确认。其他固件兼容性、停止≤100ms、推杆取消、电量准确性、ATOM真实冷启动及30min并发仍待实测。
- Mini 来源报告完整 App prologue 对回中/遥测可能必需；当前不重放尚未明确语义的整套 App 初始化。若不产生有效通知或回中无效，先获取该实机抓包再补齐必要握手。
- 原生回中是云台自己的零位，不是任意记录零位；用户零位和软限位尚未实现。没有高频可信角度反馈，不提供虚假的边界保护。
- 激活前短采样观察到 `04/66` 的31字节TLV，`22/23/24` 为两字节字段，约每秒一次；`04/10` 中同名tag也可能只有一字节，不能混用。只读 `rs3_pose_raw`：CRC/端点/04/66/header1门禁，三轴必须齐全、各两字节、不得重复，完整遍历TLV，拒绝截断/孤立尾字节。通过后原子发布一个三轴快照及独立时间戳；断开清除valid，status报告raw值和年龄。单位/绝对零点未标定，不用于运动/闭环/软限位。已通过主机/IDF并合入本机烧录的0x107c40，激活前曾实测valid1。用户确认激活后的本次04/66变为19字节、缺22/23/24，解析器保持valid0；当前没有可用三轴反馈，不能沿用旧布局推断角度。
- 板载按键设置菜单、BLE 手柄驱动云台、模拟摇杆驱动实机暂未接入；相机模拟保持原功能。
- 编译与主机测试证明软件行为；烧录、连接、实际动作与 30 分钟稳定性独立记入实测记录。
