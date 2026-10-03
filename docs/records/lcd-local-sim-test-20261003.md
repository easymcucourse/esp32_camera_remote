# 2026-10-03 LCD 本地 ATOM 模拟回归

## 实现

LCD 增加 RAM-only `atom sim on|off`、上线 / 离线 / 重启 / 版本以及 fail / crc / timeout 故障计数，手柄连接、电量、gap / overflow、动作序列和云台链路模拟。`common/atom_sim` 复用协议解析 / CRC、已有事件队列构造 HELLO / POLL；随后走实际 atom_client 与 gamepad_input，不直接切 UI 或绕过安全释放。

启用时停止物理 ATOM probe / transmit / receive，关闭后重新 HELLO 恢复。来源 epoch 改变先释放旧输入并清基线；通过任务通知打断一秒 / 五秒重连等待，主动离线立即进入同样的释放入口。主动离线按探测失败处理，避免标成版本不匹配。POLL 重启 / 版本 / 连续失败仍走原客户端。10ms 播放器动作异步完成，断开取消并释放；SIM 标记在本地模拟离线时仍保留。统计只计实际 I²C。

## 初轮验证

40 / 40 CTest 通过，新增模型 / 客户端联合测试：事件 ACK 重复及确认、gap 清除、两次 CRC 不离线、三次失败断开、重启、版本不匹配、离线与有限超时计数。LCD 开发和关闭模拟独立构建通过；发布构建同时关闭显示故障注入，ELF 中无 atom_sim_transact / pad_cmd_parse / pad_player_tick。

初轮 COM8 / 460800 烧录后，本地脚本 46 条命令、真实 I²C 输入 18 条、真实故障恢复 19 条通过。确认 Start 切页、模拟量 / release all、gap / overflow 不切页、故障重连、版本恢复、boot_id=2、主动离线输入清零、参数边界。ATOM 实测 lcd=0 且心跳年龄约 8 秒；退出本地模拟后 lcd=1，确认物理轮询暂停和恢复。

初轮发现主动 offline 被 HELLO 失败误记为版本故障，已修改成模拟探测离线分支。修正后的开发 / 发布构建、COM8 二次烧录与 46 命令复测通过。专门截取 atom offline → atom online 日志，确认无错误版本警告且 mismatch=0。7 命令重启脚本验证：模拟开启时单次显示故障恢复成功，持续故障三次失败后 camera_drained=1、NVS preserved、重启到 READY；最终 SIM=0 / display_failed=0。随后真实 I²C 输入 18 命令通过。ATOM 维持上轮 Matrix 调试固件，未再次烧录。

本地证据：`build/sim-host-test.log`、`sim-lcd-build.log`、`sim-lcd-release-build.log`、`sim-lcd-final-build.log`、`sim-lcd-release-final-build.log`、`lcd-local-sim-final.log`、`local-sim-pair-pad-test.log`、`local-sim-pair-fault-test.log`；最终证据为 `build/flash-local-sim-final.log`、`build/lcd-local-sim-fixed.log`、`build/lcd-local-sim-reset-test.log`、`build/local-sim-fixed-pair-pad.log`。

## 边界

相机 session=0，默认脚本先检查该条件。RT 全压 / 主动离线只验证 LCD 快照与软件释放路径，不证明在线相机收到实际 S1/S2 / 变焦释放。尚未拔下 ATOM 单独测试；停物理心跳已有实测。左摇杆仍未通过 v2 转发，云台仅模拟连接状态。未观察 UI / Matrix 物理视觉，取景负载、真实输入竞争与 30 分钟稳定性仍待验收。
