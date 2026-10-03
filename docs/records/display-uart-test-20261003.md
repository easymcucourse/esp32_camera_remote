# 2026-10-03 显示恢复与双端 UART 回归

本轮自动烧录 LCD（COM8，ESP32-S3）与 ATOM（COM6，ESP32），没有擦除 NVS。ESP-IDF 5.5.1，两端构建成功；33 项 CTest 通过。相机未加入热点，DS4 没有有效报告；以下证明离线显示恢复和串口行为，不证明取景、手柄动作或灯阵物理视觉。

## 首轮发现与修正

旧恢复实现删除面板后重新申请双帧缓冲。实机一次回调丢失后，三次申请均返回 `ESP_ERR_NO_MEM`，最终受控重启。重启期间另出现一次 IO 扩展芯片 NACK，随后再次启动到 READY。证据：本地 `build/uart-display-recovery-20261003.log`。尚未确定分配失败的底层原因。

改为保留缓冲，reset / init RGB 与 GDMA，先等待两次新完成回调，再填充后缓冲并发布；驱动错误才删除 / 重建。持续超时仍最多三次，随后清理、排空相机并在内部 RAM 栈软重启。扩展寄存器写入增加三次有限重试。

## 复测

使用 `tools/uart_script.py`，115200，默认打开端口不复位。烧录使用 esptool 的 `@flash_args`，LCD 460800、ATOM 115200。日志仅保存在本地 build 目录。

| 项目 | 结果与证据 |
| --- | --- |
| 两端烧录校验 | 成功；`build/flash-recovery-reuse.log`、`m5_atom_matrix/build/flash-recovery-reuse.log` |
| LCD / ATOM 控制台 | `console-smoke.uart` 各六命令通过；`build/uart-smoke-reuse-lcd.log`、`build/uart-smoke-reuse-atom.log` |
| 请求号 / 错误 | help、version、status、log 答复带匹配编号；非法 log 级别返回 ERR；全局 tag 正确解析为 `*` |
| 一次回调丢失 | 约 1 秒超时，首次恢复成功，随后 `display_failed=0`；内部 RAM 43519、PSRAM 2386504，恢复前后读数一致 |
| 持续回调丢失 | 三次恢复均超时；日志 `camera_drained=1; NVS preserved`，随后到 READY，`display_failed=0` |
| I²C 恢复 | 重启后 ATOM 在线，协议 v2、failures=0；仅此短窗口，无高频手柄负载 |

显示注入复测完整日志：`build/uart-display-reuse.log`；脚本 `tools/uart_scripts/lcd-display-recovery.uart`，五命令通过。此轮重启未出现 NACK，不能据一次启动认定该偶发问题彻底解决。

## 实现及边界

- 两端单一 UART0 消费任务，共享行编辑 / 引号解析 / 请求号 / version / log，状态读取使用短锁或原子快照。
- `uart_script.py` 支持多端口、新鲜 ACK、跨端等待和分段日志；热点异步操作等待匹配 token 终态。工具单元测试覆盖这些行为，本轮实机未修改热点或执行全部重置。
- `CONFIG_APP_DEBUG_FAULT_INJECTION` 控制 LCD 注入入口；一次 / 持续状态只在 RAM，重启清除。模拟手柄、I²C 故障与完整监视尚未实现，当前状态 `sim=0`。
- LCD 右上相机电量 / 对焦模式及 Matrix 第一行 DS、第二行 BLE、第三行云台电量已有代码与模型回归。未知 / 离线熄灭电量条，BLE / 云台目前只有提交 API，没有连接和真实电量来源。
- 活跃取景期间故障、连续运行 30 分钟、按键时延、灯阵视觉和相机状态实际显示仍待验收。
