# 2026-10-03 ATOM 手柄模拟回归

ATOM `pad sim`、动作解析 / 播放已实现并通过真实 I²C 送到 LCD。35 项 CTest 通过；双端开发固件构建 / 烧录校验成功，ATOM 关闭 `CONFIG_REMOTE_DBG_SIM` 的独立构建成功，其 ELF 不含 pad_cmd_parse、pad_player_tick、player_task 符号。关闭版本没有烧录，设备当前仍运行开发版本。

## 实机证据

LCD COM8、ATOM COM6，115200。相机离线、真实 DS4 无有效报告；没有执行 shoot / record、热点修改、全部重置或 NVS 擦除。

```powershell
python tools/uart_script.py --port lcd=COM8 --port atom=COM6 --script tools/uart_scripts/pair-pad-sim.uart --log build/pair-pad-sim-test.log
```

18 命令通过：

- 模拟连接后 LCD `ds4=1 sim=1`。
- `tap start 100` 经真实 I²C 触发 SETTINGS；`tap options 100` 回到预览状态，分别 `settings=1 / 0`。
- 右摇杆 `(-128,127)`、RT 半压 153 到达 LCD；`release all` 后摇杆与扳机为 0。
- `pad overflow` 后的 Start 按下带 gap；LCD `settings=0`，没有误切页，释放 / 确认后故障清除。
- `pad sim off` 后 LCD 记录旧输入已释放，并回到 `ds4=0 sim=0`。

另重复 50 次 `tap start 100`：50 个 DONE 均报告 100ms，LCD 记录 50 次切页；窗口内没有 `Transaction failed` / ERR / FAIL。记录是调度器时间和日志观察，不是外部仪器测量的相机响应延迟，也不是 30 分钟稳定性验收。证据：本地 `build/pad-timing.uart`、`build/pad-timing-test.log`。

最后两端 console-smoke 各六命令通过；`build/console-post-pad-lcd.log`、`build/console-post-pad-atom.log`。关闭模拟状态后结束测试。

## 实现与边界

`common/pad_cmd.*` 解析别名、组合、模拟量、shoot / record、seq；最多 32 动作，解析失败不提交部分动作。`pad_player.*` 使用绝对截止时间、四项工作队列及取消释放，支持计时回绕；ATOM task 优先级 3、3072 字节栈、10ms 周期，控制台保持低优先级。

真实 / 模拟共享 ds4_host 的 apply_locked 快照 / 事件发布。模拟时真实报告不改选中输入，但蓝牙连接维持。退出取消所有排队动作，释放并恢复真实来源，真实快照由后续报告更新。POLL 偏移 27 新增 SIM / 输入代数，LCD 对来源边界先安全释放并丢弃缓存；两端必须一起更新。快速 source off / on 的保护有代码，尚未单独做高频切换实机验收。

模拟电量可供 Matrix 模型使用，LCD 仍只显示相机电量。LCD 连接页 / LIVE / SETTINGS 已绘制 SIM 文本，status 及 I²C 标记实测通过；物理屏幕 / 灯阵视觉未验收。

主机回归覆盖别名、范围、嵌套 / 超量拒绝、阈值典型值、队列、计时回绕、取消、合并发布与迟唤醒不累计漂移；脚本工具新增异步时长预算测试。本轮未测试相机快门 / 录像 / 变焦效果、真实 DS4 与模拟输入竞争、全部重置或长期高频 I²C。

LCD 本地模拟 ATOM、I²C 监视 / 故障命令、灯阵调试、BLE Xbox、网页维护 / OTA 等继续未完成。
