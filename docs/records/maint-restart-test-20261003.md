# 2026-10-03 维护安全重启

已接入网页“重启设备”按钮和 `/api/reboot`。浏览器确认后只提交 `confirm:true`，接口先预留唯一请求，完整发送响应后提交 1500 ms 截止时间；未发送成功取消。预留不会触发重启；重复请求 409。只允许登录会话，确认缺失、类型错误、重复或额外字段返回 400。

重启由已有内部 RAM 栈的 health 执行。维护任务通过终止标志拒绝新开启，串行关闭 HTTP、清除鉴权并释放自己持有的相机 lease，终止时不重新启动相机；health 确认关闭后再取得相机 lease 排空，随后软重启。关闭和排空各最多三秒，失败时记录失败并尝试重启，不宣称失败退路已成功排空。LCD 三次恢复失败也使用此流程，修复维护先占 lease 导致二次取得失败的路径。没有改变分区或擦除 NVS。

## 验证

- 主机 47 / 47：`build/maint-restart-host-test.log`；新增 restart_schedule 测试覆盖预留 / 取消、响应后提交、重复拒绝、延迟边界和回绕。
- LCD 开发 / 关闭模拟构建通过：`build/maint-restart-final-build.log`、`build/maint-restart-final-release.log`；镜像大小分别 `0x343070` / `0x33f420`，仍使用原 factory 分区。COM8 开发版烧录通过：`build/flash-maint-restart.log`，保留 NVS；ATOM 未重新烧录，生产版未烧录。
- `maint-restart.uart` 12 条命令通过：`build/maint-restart-test.log`。分别在 `maint on stop` 与 `maint on` 下注入持续 LCD 故障，两次均三次恢复失败后完成 `maintenance_closed=1` / `camera_drained=1`、重启到 READY。前一种已持有 lease，验证关闭后能重新取得。
- 网页重启开发探测 21 个真实 TCP / HTTP 请求通过：13 个基础登录 / 信息 / 会话测试，再检查未登录 401、五种无效确认 400、成功 200 与正确延迟、重复 409。成功响应后尚未到期且维护仍开，随后 health 执行 Web restart，确认关闭和排空均为 1；再次 READY，维护 / SIM / SETTINGS 关闭，热点原配置和 full 信息档位可读取。
- 最终真实 I²C 回归 `pair-maint-gamepad.uart` 八条命令及 14 个 HTTP 请求通过：`build/maint-restart-pair-regression.log`，确认重启后可再次用手柄输入开启 / 退出维护，不残留终止标志。结束维护关闭、SIM0 / SETTINGS0、相机自动连接开启等待已配对相机。

## 边界

相机会话始终为 0，排空的是等待连接任务；没有证明真实在线拍摄动作、held S1 / S2 / zoom 释放或取景恢复时延。HTTP 回环不证明浏览器视觉或手机空口；发送失败取消有纯调度测试与源码，未注入实际断网发送失败。维护开启过渡竞争、队列满、关闭 / 排空超时、多个客户端和并行配置写入整机路径仍待验收。NVS 保留有未擦除路径及配置 / 显示读取证据，未执行重新配对来证明身份完整性。OTA 上传、双分区 / 回滚、BLE Xbox、相机 UI 缺项和长期稳定性继续实施。
