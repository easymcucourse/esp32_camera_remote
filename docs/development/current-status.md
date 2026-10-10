# 当前实现与验证边界

[English](../en/development/current-status.md) · **简体中文** · [日本語](../ja/development/current-status.md)

按2026-10-10源码和本轮验证更新。需求保留未完成目标；历史记录中的“当前”只代表记录日期。

## 当前实现

| 领域 | 当前代码 | 验证边界 |
| --- | --- | --- |
| LCD架构 | Core组合根；Console typed路由；Wi-Fi/Input/Camera/UI独立owner；UI→display_surface→board_7b；退休API只在tests/support/legacy | 新架构全部停止/错误路径和长期运行未全部验收 |
| 相机 | Sony ZV-E10、AP DHCP发现/相机确认配对、PTP/IP双通道、0x9209属性、0xFFFFC002 JPEG、参数目标/回读和安全释放 | 部分动作有历史实测，不保证所有镜头、枚举或相机 |
| 输入 | Classic DS4和限定Ultimate 2 BLE报告经ATOM/I²C；当前镜头按用户声明为POWER_ZOOM | 未自动识别镜头；非电动变焦MF替代当前不启用；BLE实体映射/相机控制待验 |
| UI/维护 | LIVE/SETTINGS/MORE、信息档位、录像提示；启动页HTTP进入独占维护，Web写热点/偏好/恢复/OTA，成功后重启 | 普通UART/手柄无热点编辑/恢复出厂入口；Web无PIN/登录；手机及完整故障覆盖待验 |
| ATOM | Classic+BLE双模、共享BLE扫描、独立灯阵、电量、I²C v2、真实DS4新鲜报告 | BLE连接不等于实体输入已验；灯阵/LCD视觉独立验收 |
| RS 3 Mini | 首次唯一候选/保存目标、通知通道、DS4左摇杆、L3原生回中、停止门禁、NVS独立Pan/Tilt幅度 | 用户确认摇杆/L3、调速120/240和云台关机重启正常；任意记录零位/软限位/板载设置菜单未实现 |

## 本轮软件验证

代码提交 `11266fe` 已推送到origin/main。主机CTest267/267通过（C场景和Python测试容器，条目数不是断言数）。全新 `cleanup20261010` 标签的LCD/ATOM Debug及Release四配置通过，LCD component graph/直接symbol owner及Release模拟符号门禁通过；LCD镜像小于5MiB。文档整理未进行硬件烧录，不能把新构建当成设备运行版本。

## 硬件结果与待验

RS 3 Mini实机FFF4仅Notify，经CCCD初始化；heartbeat发送端点4后用户确认摇杆/L3生效。本机Pan120/Tilt240已跨RTS软件重启保持，工厂默认仍120/120；幅度不是角速度。真实云台关机后自动恢复、DS4保持新鲜，用户回复正常；ATOM真实断电冷启动仍待验。

激活前04/66三轴TLV曾可解析，激活后19字节包缺三轴字段，strict pose_raw保持invalid。raw单位未标定，不用于闭环/限位。松杆≤100ms、回中取消、异常断连及完整30分钟并发待验；约14分钟部分采样健康不算30分钟。LCD云台故障路径仅实现/构建/主机验证，未烧录和视觉验收。

取景内存配置保留已测阶段值：Wi-Fi/lwIP优先PSRAM、内部预留32768、静态TX6、缓存32、main栈24576；Default实验时钟与Stable80MHz独立。2026-10-08长测无NO_MEM，但平均约4.76FPS仍未达原性能门禁；该优化任务已停止，不从本轮构建推断性能达标。

## 证据

- [RS 3 Mini协议](../design/rs3-mini-protocol.md)、[实测](../records/rs3-mini-test-20261010.md)、[验收](../records/rs3-mini-acceptance-20261010.md)。
- [取景实测](../records/liveview-execution-20261008.md)、[性能验收](../records/liveview-acceptance-checklist-20261008.md)。
- [模块状态](module-split-status.md)、[逐项验收](module-split-checklist.md)。

原始UART、抓包、镜像及真实设备身份保持Git忽略；正式记录只保留脱敏事实/统计。
