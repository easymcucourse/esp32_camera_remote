# 2026-10-03 双端 I²C 监视回归

新增 `common/i2c_monitor.*` 纯 C 统计 / 缓冲与 `i2c_debug.*` 短锁 / 控制台适配，两端支持 `i2c log on|off|changes`、`i2c stats [reset]`。默认日志关闭，统计持续更新；最多 32 条日志，满时丢弃新日志并计数，不等待 UART。打印只在控制台任务，每次 poll 最多取两条。

`changes` 比较请求 / 响应的语义字段，忽略每次递增的序号及依赖它的 CRC；失败帧总是记录。统计包含总数、失败数、超时、CRC、头部、序号、远端结果码、驱动错误、无效载荷、最长耗时及日志丢弃。LCD 统计完整收发尝试，未计 probe；ATOM 统计完整候选请求，垃圾 / 接收队列溢出另由 status 的 invalid 计数。ATOM 保留解析器重同步前的真实候选字节，避免错误日志被后续移位覆盖。

## 验证

- 36 项 CTest 全部通过：新增 i2c_monitor 覆盖分类、截断、安全边界、变化过滤、失败保留、环形顺序 / 溢出与统计重置。
- 双端开发构建 / 烧录校验成功；ATOM 关闭模拟选项的独立构建也通过，监视功能保留。关闭版本未烧录。
- `pair-i2c-monitor.uart` 20 命令实机通过：开启 changes / 全量日志、模拟 Start 切页、读取统计、关闭日志 / 模拟、非法日志模式拒绝。两次 Start 恢复初始界面状态。
- 另运行 208 命令脚本，其中两端各 100 条 status：所有编号答复通过，双方结束时 total=168、failed=0、log_dropped=0；LCD max_ms=19，ATOM max_ms=0（毫秒计时精度）。该短窗口不证明取景负载或长期稳定性。

本机 LCD COM8、ATOM COM6，串口 115200；LCD 烧录 460800、ATOM 115200，使用各自 build 的 `@flash_args`，没有擦除 NVS。相机未上线，真实 DS4 无有效报告。

证据：本地 `build/monitor-host-test.log`、`build/i2c-monitor-lcd-build.log`、`build/i2c-monitor-atom-build.log`、`build/monitor-release-build.log`、双端 `build/flash-i2c-monitor.log`；实机 `build/pair-i2c-monitor-final.log`、`build/monitor-stress.uart`、`build/monitor-stress-test.log`。测试结束日志 off、sim=0、settings=0。

CRC / 序号 / 超时分类已有主机回归，本轮没有实际注入这些总线故障。I²C req / drop / corrupt / delay、LCD 本地模拟、灯阵调试及其他目标继续未完成。
