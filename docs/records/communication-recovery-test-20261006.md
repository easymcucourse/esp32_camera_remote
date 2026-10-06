# 2026-10-06 启动内存与相机／I²C 通信修复

状态：2026-10-06 22:11 按用户要求暂停。在线菜单和在线故障短测通过；30 分钟稳定性测试失败，LCD 显示异常未解决。

用户要求烧录测试，反馈启动失败重启、切换设置页面出错及反复断流，随后要求自动解决相机和 I²C 通信问题。最后反馈“lcd spi显示不正常”，并要求“今天暂停，整理提交推送”。本记录随模块迁移及通信修复一起保存为阶段提交，硬件目标尚未完成。

## 已定位与修复

- 启动失败：30 行 LCD bounce buffer 占用两块共 122880 字节内部 RAM，Messages/router 创建内部栈时只剩 3019 字节、最大块 1984 字节。改成 10 行共 40960 字节；endpoint 消息载荷存储放 PSRAM、FreeRTOS 控制块仍在内部 RAM，部分分配失败释放两块内存。保持 18MHz、两块全屏 framebuffer 和启动时 512KiB×2 相机包缓冲。20 行中间版本仍在 UART 初始化时失败，不能作为通过版本。
- 相机重复断流：诊断捕获 `Stream stage=events backend=6 elapsed=0ms`，同时存在实际 5 秒等待后失败；不是 JPEG 解码失败。Router 原来在请求入队和回复时使用零等待 mutex，短暂 SMP 锁竞争直接返回 TIMEOUT，或丢掉回复。阻塞 request/request_many 和 reply 改为在原绝对 deadline 内等待 router 锁；队列容量仍零等待，普通 send 仍零等待。未扩大网络 timeout，未重发有副作用的 PTP 命令。
- 输入状态误离线：相机已经连接且帧率日志 4.43 fps 时，UART 快照出现 atom=0/source_epoch=0/report_id=0，而两端真实 I²C 仍在正常轮询。Owner 在一个 tick 内同步动作回调期间，provider 可发布捕获时间晚于 tick 起始 now 的新报告；无符号相减把新报告当成超一秒过期。改为有符号时序比较，保留真正超一秒报告的安全释放。新增模拟回调期间入队及毫秒计数回绕测试；修复前新用例在 report_id=201 断言失败，修复后通过。
- 在线菜单过期：输入时序修复版持续取景两分钟，Input保持在线；ATOM模拟Start经真实I²C已到LCD，但settings仍0。Input菜单100ms deadline不足以等待UI sole consumer完成当前约200–300ms JPEG；仅菜单动作调整到500ms，周期查询和Camera动作／安全释放仍100ms。不重发可能已执行的toggle。新增250ms UI占用回归，旧代码断言失败，修复后复测中。

## 已获得的证据

- 双端实际分区表与构建分区一致；LCD COM8 只写运行 ota_0 的应用偏移 0x20000，ATOM COM6 只写应用偏移 0x10000。Hash 校验通过；NVS、otadata、bootloader 和分区表保留。备份和原始日志只在忽略目录 build 保存，本文不复制密码或设备身份。
- ATOM 使用已有双模 Debug 构建目录 `build/ci-atom-debug`，应用大小 0x1043f0；本地子工程旧 sdkconfig 的 BR/EDR-only 构建失败，未把失败构建用于烧录。真实 DS4 已连接，protocol=2。
- LCD 10 行版本启动 READY、主任务返回后 free_internal=72331；Router internal=80847。最新菜单修复版 Default 0x358ec0、Stable 0x3580b0、Release 0x34ce70 构建均通过，Release 禁模拟符号检查、模块边界和 diff 检查通过。当前只烧录 Default，Stable/Release 的实机效果没有验证。
- Host 263/263 通过，覆盖 Router 请求、批量请求与回复的短暂锁竞争、原截止时间及普通 send 零等待；Input 新报告并发时序、真正过期释放和回绕。Host 协作式调度不代替 SMP 实机验证。
- 消息锁修复版：12 条双端 smoke 通过，包含设置页往返、显示一次故障恢复、SIM0、ATOM 在线；当时相机离线。
- 19 条真实 I²C 故障脚本通过：两次 CRC 保持在线，三次 CRC、三次丢响应、100ms 延时后均自动恢复。测试读到坏头，统计 timeout=0，不能将其描述为真实驱动 timeout 已覆盖。
- 双端 100 次 status、20 次 LCD 设置页切换，共 224 命令通过。LCD 504 次 I²C 事务失败 0，ATOM 503 次失败 0；窗口约 25 秒，相机离线。
- ATOM 模拟菜单事件经真实 I²C：10 次 Start/Options 往返，共 44 命令通过，终态 SETTINGS0/SIM0；相机离线，不是实体按钮视觉验收。
- 首次在线 soak 开始时相机 session=1，I²C 正常，但 Input 错误离线，因此失败；该证据促成第二项输入修复，不计为稳定性通过。输入修复版已构建并烧录。
- 输入修复版默认烧录后120秒双端记录通过：session1/atom1/source_epoch3持续，未观察到Stream退出或重建；帧率短窗2.78–约4fps，不代表全屏最低3fps性能验收。在线菜单44命令脚本首个Start失败，SIM已单独关闭并确认；该失败促成菜单deadline修复。
- 菜单时限修复版已 COM8 应用烧录、hash 和 READY 通过。21:38 双端脚本运行约 1005 秒后主动结束，保留日志；相机在第 936 秒加入，此前离线时间不计为取景稳定性。LCD/ATOM 各约 19302 次事务失败 0；随后约 69 秒在线片段未观察到 Stream 退出，不能替代 30 分钟在线验收。
- 在线菜单归一化脚本 42 命令通过：ATOM 模拟 Start/Options 经真实 I²C 完成 10 次往返，21 次 LCD 查询保持 session1/atom1，窗口约 18 秒，Stream 退出 0。首个未归一化脚本把用户已打开的 SETTINGS1 当成 SETTINGS0，预期错误；该次不能计为固件失败。测试后另行关闭 SIM 并确认。
- 在线 I²C 故障脚本 19 命令通过，CRC/丢响应/延时后恢复且相机会话保持；记录 5 次 CRC 和 6 次坏头，driver timeout=0。清理与单次显示恢复脚本另 8 命令通过，短窗口 session1/atom1/display_failed0。该显示健康标志不能证明实际屏幕扫描和画面视觉正确。
- 最后从 21:59 开始的 30 分钟在线脚本在约 123 秒失败，错误为第 38 行等待 session=1 超时。日志持续出现取景读取响应 0x200F、liveview backend=12，共 5 次 Stream 退出，shown=0、frame_failed=0，未取得帧率窗口。最后 session0/fps0，双端各 2411 次 I²C 事务失败 0，atom1/SIM0/SETTINGS0/display_failed0。短测成功后仍出现拒绝取景与重连，原因尚未定位，不能宣称通信修复完成。
- 用户同期反馈 LCD 显示不正常，异常是花屏、错位、撕裂还是冻结尚未明确。实际板卡为 RGB LCD；10 行 bounce buffer 是待核查项，尚无证据证明它是根因。暂停时串口脚本已失败退出，无仍运行的串口测试进程；没有继续烧录或硬件操作。
- 提交前重新运行 Host 263/263、模块边界检查通过；最终文档链接与暂存内容检查结果见阶段整理记录。

## 尚待证明

恢复工作后优先定位相机 0x200F 拒绝取景和 LCD 显示异常，随后重做不少于 30 分钟在线通信稳定性及实体切页验收。在线菜单和故障恢复只有短测证据；其他完整模块迁移验收项目没有因本次通信短测而自动通过。

本次通信目标的完成判据（与整个模块迁移计划分开核验）：

| 判据 | 当前证据 / 状态 |
| --- | --- |
| 最终固件启动且不再因初始化内存不足重启 | Default菜单修复版hash/READY通过；当前持续运行，无panic或新启动记录 |
| 两端I²C正常轮询无误离线 | 最后失败窗口两端各2411事务失败0；完整30min在线目标未完成 |
| I²C CRC/丢响应/延时后自动恢复 | 最终版相机在线19命令通过；driver timeout场景未覆盖 |
| 真实相机持续取景无重复会话退出 | 最终30min脚本失败，123秒内5次退出、shown0；未通过 |
| 在线设置页切换可靠，取景继续 | 最终版真实I²C在线10次往返短测通过；实体及长期验收待完成 |
| 显示恢复后相机／输入继续工作 | 最终版在线单次恢复短测通过；后续仍发生取景拒绝，连续性未通过 |
| LCD视觉与实体DS4切页正确 | 用户报告显示不正常，待定位与再次人工验收 |
| 验证后SIM0/SETTINGS0/录像停止/显示健康 | 最后记录SIM0/SETTINGS0/display_failed0；录像状态known0不能证明相机已停止录像 |
| 构建、主机回归、文档与本机记忆一致 | 263host、三LCD构建、Release禁SIM符号、boundary通过；按暂停终态更新正式记录与记忆 |

证据索引：`build/flash-test-20261006-*`，尤其 `router-{smoke,i2c-fault,stress,i2c-menu}.log`、`comms-soak.log`（失败）、`input-race-before.log`（修复前失败）、`input-race-host.log`、`input-race-lcd-{build,flash}.log`、`ui-budget-soak.log`（部分窗口主动结束）、`ui-budget-online-menu-normalized.log`、`online-i2c-fault.log`、`online-fault-cleanup-display.log`、`online-comms-soak.log`（失败）。原始日志只保存在本机忽略目录。
