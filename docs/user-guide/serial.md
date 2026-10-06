# 串口命令

LCD 和 ATOM 串口波特率均为 115200。输入命令后按 Enter（包括单字符命令）；最多 255 字节，支持双引号、单引号和反斜杠转义。用脚本发送并保存日志的方法见 [串口日志工具](../development/serial-log.md)。两端共用 `help`、`version`、`status`、`log <tag|*> <none|error|warn|info|debug|verbose>`；可在命令前加 `#123`，同步答复带相同请求号。LCD 保留下面的相机、热点命令；ATOM 的 `status` 提供 LCD 心跳、手柄、事件队列与灯阵状态。

开发构建的 LCD 支持 `display fault off|once|persistent`：一次回调丢失验证扫描恢复，持续丢失验证三次恢复失败后的排空和重启。仅改变 RAM 状态；命令需 `CONFIG_REMOTE_DBG_SIM` 与底层 `CONFIG_APP_DEBUG_FAULT_INJECTION` 支持，生产构建关闭后不可用。

可重复测试：`python tools/uart_script.py --port COM8 --script tools/uart_scripts/console-smoke.uart --log build/console-test.log`。COM8/COM6为历史示例，须替换为本次确认的LCD/ATOM端口。支持 `--port lcd=COM8 --port atom=COM6` 与脚本中的 `@lcd` / `@atom`；`!` 期待错误，`expect` / `expect-any` 等待新输出，`wait` 等待毫秒数。工具自动添加请求号，等待热点 / 手柄异步操作的匹配 token 终态，按动作时长延长超时。`lcd-display-recovery.uart` 包含受控重启。

ATOM 开发构建支持 `pad sim on|off`、`pad connect|disconnect`、`pad battery 0..10|none`、`pad overflow`。先开启模拟并连接，再发送 `tap start 100`、`hold a+b`、`release all`、`stick r -128 127`、`trigger rt half|full|off` 或 `seq hold rt; wait 300; release all`。名称接受 Xbox / DS4 别名，不区分大小写；最多 32 个展开动作、四项工作队列；动作由独立任务执行。`shoot` / `record` 会向在线相机发送真实动作，执行前输出 SIM 警告。关闭模拟时取消排队动作、释放当前输入并恢复真实报告；状态与显示标注 SIM。生产可关闭 `CONFIG_REMOTE_DBG_SIM`，相关命令不注册，解析器和播放器不编译。

`pair-pad-sim.uart` 验证真实 I²C 上的 Start 切页、模拟量与溢出保护。两端 `i2c log on|off|changes` 开关逐帧监视，默认 off；changes 忽略序号及其 CRC，只输出语义变化与失败帧。`i2c stats` 查询总数 / 分类 / 最长耗时 / 日志丢弃，`i2c stats reset` 只清统计。监视队列最多 32 条，满时丢日志不阻塞总线。`pair-i2c-monitor.uart` 提供双端验证。LCD 本地模拟命令见下方。

ATOM 开发构建另支持 `i2c drop <0..10000>`、`i2c corrupt <0..10000>`、`i2c delay <0..200>`：丢弃后续合法请求的响应、翻转响应 CRC、延后发送毫秒数。0 取消；计数 / 延时重启清除，生产关闭模拟时不可用。会使 LCD 重试及断开，适用于开发验收。`pair-i2c-fault.uart` 验证两次 CRC 不断开、三次故障断开及恢复。LCD 本地提供 atom fail / crc / timeout（见下方）。

ATOM 开发构建还支持 `i2c req <9 个十六进制字节>`，可连写或逐字节空格分隔。例如 `i2c req a5 02 01 01 02 02 00 00 86`。请求排入 I²C 任务，先返回 `OK SIM i2c req queued token=N`，随后 `DONE SIM i2c req token=N status=OK reply=...`；错误 CRC 返回 `BAD_CRC`，无完整帧返回 `NO_RESPONSE`，格式错误直接 `ERR`。同一时刻仅允许一个请求；回放工具等待对应 token。复用实际协议解析 / 响应与 POLL ACK，虚拟请求不写真实 TX FIFO、不刷新 LCD 心跳、不计真实 I²C 统计、不消费故障计数。LCD 在线时会警告 ACK 可能确认真实事件；开发测试须考虑这一影响。生产关闭模拟时不提供此命令。脚本见 `tools/uart_scripts/atom-i2c-req.uart`。

ATOM 开发构建支持 `led test`：切换四角校准模式，每秒按左上红、右上绿、右下蓝、左下白循环。再次执行恢复正常渲染；`led fault bt|i2c|overflow on|off` 设置独立强制图案，优先级仍为蓝牙 → I²C → 溢出，真实故障保持。校准优先于故障图案。`led off` 清除校准与全部强制图案，真实故障不清除。设置只保存在 RAM，重启清除，生产关闭模拟后命令不可用。`status` 的 `led calibration=... forced=... SIM` 显示本地调试状态；它只改变 Matrix 显示，不向 LCD 伪报链路故障。回放脚本为 `tools/uart_scripts/atom-led.uart`，物理方向需观察灯阵确认。

LCD 开发构建支持 `atom sim on|off`、`atom online|offline|reboot`、`atom version 0..255`、`atom fail|crc|timeout 0..10000`。独立SIM provider经同一CRC/HELLO/POLL协议内核处理，物理ATOM provider继续I²C轮询；Input服务只选择一个来源，切换先释放旧输入。版本故障和重启经过原有判定路径；关闭后Input选择真实来源；物理链路无需重新启动。状态与界面保留 SIM 标记，即使模拟 ATOM 离线。设置只在 RAM，重启后关闭；关闭 `CONFIG_REMOTE_DBG_SIM` 不提供模拟命令。

LCD 模拟开启后还支持 `pad connect|disconnect`、`pad battery 0..10|none`、`pad gap|overflow`、`gimbal state off|search|connecting|connected`，以及同 ATOM 的 tap / hold / release / stick / trigger / shoot / record / seq。动作时序仍由 10ms 播放器执行；事件使用已有 ACK / gap 队列。左摇杆保留在模拟快照但现有协议不转发，云台仅模拟链路状态，运动不在范围。模拟拍照 / 录像仍会操作在线相机。默认本地回放脚本先要求 session=0，避免其中 RT 全压实际拍摄。当前 I²C 监视统计只记录实际总线事务，不混入本地模拟；脚本 `lcd-local-sim.uart` 包含两端检查，需已接ATOM验证模拟期间及退出后物理心跳均继续；脚本预期已更新，当前拆分版未实机运行。`lcd-local-sim-reset.uart` 在模拟开启时验证既有显示恢复和重启清除模拟，需要相机 session=0，包含受控重启且保留 NVS。


| 命令 | 功能 |
| --- | --- |
| `j` | 开始或恢复取景。已经在取景时忽略重复请求 |
| `S` | 切换设置显示：左侧 768×432 缩略图，右侧显示模式、ISO、快门、光圈、EV、白平衡、对焦、测光和闪光；下方增加画幅、驱动、效果、DRO、对焦区域、无线闪光及白平衡色温/原始微调编码 |
| `s` | 请求取消网络等待、释放控制并排空取景/解码后关闭连接；OK仅表示请求已接收，任务退出以日志为准，画面保留最后一帧 |
| `p` | 做配对和同一 GUID 重连诊断。需要先停止取景 |
| `help` | 列出当前命令 |
| `wifi show` | 显示 SSID、信道、密码长度、密码显示开关和客户端；加 `password` 才输出明文密码 |

UART 只提供 Wi-Fi 查询；修改在启动维护网页完成，保存成功后设备重启。默认 SSID 为 `easycamctrl`、密码为 `00000000`、信道为 6。恢复出厂已改为启动网页操作，UART `factory` / `u` 不再提供。网页“重置热点”恢复默认网络；“全部重置”额外清除 LCD 相机配对及 UI 偏好，保留 ATOM 绑定和其他 NVS。成功后重启，失败可能已部分修改配对/偏好；实机重置及重新配对仍待验收。

同一时刻只打开一个串口程序。`serial_log.py` 退出不会停止取景，加上 `--reset` 才会复位设备。

开发版本新增 `display bench`：UART 只发送消息，基准由 UI 执行；暂停并排空相机任务，生成固定 1024×576、4:2:2、质量 80 的合成 JPEG，按当前全屏 / 设置模式显示二十帧，返回异步 token 和局部显示帧率，结束后恢复此前运行的相机任务。测试画面标有 SIM，不修改配对或显示档位 NVS。维护模式、已有 SIM 和重复基准请求会拒绝；生产版本不提供该命令。可回放 `tools/uart_scripts/display-bench.uart`，前提是已保存 full 信息、全屏、SIM 关闭；脚本经设置页进入NORMAL后开始，不能使用旧maint命令切换。该基准不包含相机 / TCP 取景耗时，不能用其 FPS 宣称真实取景达标。脚本已按当前源码更新，新固件实机回放待验证。

`JPEG phases us` 每约五秒记录一次锁等待、清零、头部准备、解码、行距搬移、叠加层和 LCD 发布耗时（微秒）。`status` 与十秒健康日志新增 `min_internal`、`min_psram`、`largest_internal`、`largest_psram`；低水位使用 IDF heap API，自启动累计，包含初始化和测试的临时分配，不能当作正常取景的最低值。

约每 5 秒记录一次显示帧率，约每 10 秒报告客户端和内存。FPS 一开始显示 0.0，大约一秒后才有统计；暂停时保留最后读数。握手和网络收发每最多 100 ms 检查停止；任务退出还要排空解码任务，完整时延需实机验证。

## 画面上的参数

详细参数来自相机属性 `0x9209`，属性变化事件后刷新，并约每 5 秒轮询保底。`--` 表示相机报告“无值”，或这一项还没读到。

尚未确认的枚举继续显示原始十六进制。曝光模式 `0x00078051` 显示为 `MOVIE A`（视频光圈优先），测光 `0x8001` 显示为 `MULTI`（多重测光）。同一个数值出现在其他属性上时，含义可能不同。

`wifi show` 的 `default_password=1` 表示当前仍用出厂密码，`status` 的同名字段反映已发布给连接页的标签状态。自定义密码为 0；SSID 或密码显示开关不会改变该判断。

`ui info` / `ui pad` 只查询本次启动加载的显示档位和手柄类型。持久化修改通过启动时进入维护网页，保存后重启生效；UART 设置参数和手柄触摸板循环写入已移除。当前 `tools/uart_scripts/ui-info.uart` 只查询并验证旧写命令被拒绝；旧持久化回放已移到 `legacy/`。

## 维护迁移状态

UART gateway 已删除 `maint on/off/status/probe`，未知命令不会触发维护或探针。旧手柄维护接口也已移除；启动网页独占维护入口和不可逆切换已接入源码，启动顺序和实机效果仍待验收。`status` 的模式来自 Core 原子模式快照。

## 录像与扩展参数诊断

`status` 的 `record known/recording/pending` 分别表示录像状态已知、实际录像中和等待确认；参数写入接受不等于回读已生效。`extra status` 显示 MORE 子菜单是否开启、选中ID，以及九项扩展参数的实际值、可写状态、status、target_valid和target。status编码：0空闲、1待确认、2已回读生效、3拒绝、4超时、5命令接受。

`status` 的 `ev actual` 是相机实际回读的曝光补偿，单位为千分之一 EV（例如 333 约为 +0.3 EV），`-2147483648` 表示未知；用于核对左右调整是否生效。

输入迁移后，LCD `status` 的 `atom` 表示当前报告的物理ATOM在线标记，`ds4` 表示所选输入已连接，`sim` 标识模拟来源。本地SIM不会伪造物理ATOM在线；诊断输出以 `source_epoch/report_id` 取代旧 `boot_id/failures/ack_id`，业务不解释协议序号。`i2c stats` 仍查询物理provider真实总线统计。SIM UART关闭/重启会取消队列及已完成HOLD留下的按键；全部模拟只在RAM。

相机命令迁移后，`s` 的 OK 仅表示 Camera 已接收停止请求；Core 的维护/重启 STOP 仍等待物理排空。`status` 并行收集消息快照，任一端点失败输出 ERR，不将缺失快照的零值打印为 OK。`extra status` 的九项参数输出语义 `property` 名称，代替厂商 code。
