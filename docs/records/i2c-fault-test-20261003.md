# 2026-10-03 ATOM I²C 故障注入回归

开发构建新增 `i2c drop 0..10000`、`i2c corrupt 0..10000`、`i2c delay 0..200`（毫秒）。计数及延时仅在 RAM；0 取消，重启清除。非法请求不消耗注入次数，drop 优先且不消耗 corrupt 次数。生产关闭 `CONFIG_REMOTE_DBG_SIM` 后不注册这些命令，`atom_fault_take` 未链接。

丢弃 / 延时前清空旧软件发送缓冲与 FIFO，避免 LCD 重试读到上一次响应。延时只暂停从机任务，控制台可发送 delay 0；正在执行的一次延时最多 200ms，随后恢复处理排队请求。丢弃响应使 ATOM 监视记录无响应，LCD 按实际收到的数据分类；这不等同于驱动超时。

## 验证与证据

37 项 CTest 通过，新增有限计数 / 优先级 / 延时重置回归，扩展发送适配测试验证 NULL/0 清空旧缓冲、禁用 TX 中断以及后续正常响应恢复。ATOM 开发 / 关闭模拟构建通过；本轮只烧录 ATOM COM6（115200），LCD COM8 继续上一轮监视固件。常规 `@flash_args` 烧录，未擦除 NVS。

`pair-i2c-fault.uart` 19 命令实机通过，见本地 `build/pair-i2c-fault-test.log`：

| 注入 | 观察 |
| --- | --- |
| corrupt 2 | 两次 BAD_CRC，重试请求 seq 与参数相同；后续成功、仍 atom=1，failures=0 |
| corrupt 3 | 同一请求三次 BAD_CRC，第三次后 link lost，退避后 HELLO / POLL 恢复 |
| drop 3 | 三次响应为无效头（00 / FF），BAD_HEADER，第三次后 link lost，自动恢复 |
| delay 100 | 三次迟到 / 部分响应 BAD_HEADER，link lost；delay 0 后自动恢复 |
| 边界 | delay 201、corrupt -1 拒绝；末尾三项均显式设为 0 |

结束 LCD 统计 total=58、failed=11、bad_crc=5、bad_header=6、timeout=0、max_ms=19、log_dropped=0。该板卡在空 FIFO 下返回数据，故本轮没有验证实际驱动超时路径。

随后 `pair-pad-sim.uart` 18 命令与 ATOM console-smoke 六命令通过，证明注入清除后正常输入 / 控制台继续工作。结束 sim=0、settings=0、I²C 日志 off。证据 `build/pad-after-fault-test.log`、`build/atom-after-fault-smoke.log`；构建 / 主机日志 `build/fault-atom-build.log`、`build/fault-release-build.log`、`build/fault-host-test.log`；烧录 `m5_atom_matrix/build/flash-i2c-fault.log`。

相机离线、真实 DS4 无有效报告。LCD 的链路离线代码会调用安全释放，但本轮没有实际按住快门 / 变焦并对在线相机验证释放。主动取景负载、实际驱动超时及 30 分钟稳定性继续待验收。`i2c req`、LCD 本地模拟与灯阵调试仍未实现。
