# 串口命令

LCD 和 ATOM 串口波特率均为 115200。输入命令后按 Enter（包括单字符命令）；最多 255 字节，支持双引号、单引号和反斜杠转义。用脚本发送并保存日志的方法见 [串口日志工具](../development/serial-log.md)。两端共用 `help`、`version`、`status`、`log <tag|*> <none|error|warn|info|debug|verbose>`；可在命令前加 `#123`，同步答复带相同请求号。LCD 保留下面的相机、热点命令；ATOM 的 `status` 提供 LCD 心跳、手柄、事件队列与灯阵状态。

开发构建的 LCD 支持 `display fault off|once|persistent`：一次回调丢失验证扫描恢复，持续丢失验证三次恢复失败后的排空和重启。仅改变 RAM 状态；生产构建可关闭 `CONFIG_APP_DEBUG_FAULT_INJECTION`，命令随后返回不可用。

可重复测试：`python tools/uart_script.py --port COM8 --script tools/uart_scripts/console-smoke.uart --log build/console-test.log`。ATOM 使用 COM6。支持 `--port lcd=COM8 --port atom=COM6` 与脚本中的 `@lcd` / `@atom`；`!` 期待错误，`expect` / `expect-any` 等待新输出，`wait` 等待毫秒数。工具自动添加请求号，等待热点 / 手柄异步操作的匹配 token 终态，按动作时长延长超时。`lcd-display-recovery.uart` 包含受控重启。

ATOM 开发构建支持 `pad sim on|off`、`pad connect|disconnect`、`pad battery 0..10|none`、`pad overflow`。先开启模拟并连接，再发送 `tap start 100`、`hold a+b`、`release all`、`stick r -128 127`、`trigger rt half|full|off` 或 `seq hold rt; wait 300; release all`。名称接受 Xbox / DS4 别名，不区分大小写；最多 32 个展开动作、四项工作队列；动作由独立任务执行。`shoot` / `record` 会向在线相机发送真实动作，执行前输出 SIM 警告。关闭模拟时取消排队动作、释放当前输入并恢复真实报告；状态与显示标注 SIM。生产可关闭 `CONFIG_REMOTE_DBG_SIM`，相关命令不注册，解析器和播放器不编译。

`pair-pad-sim.uart` 验证真实 I²C 上的 Start 切页、模拟量与溢出保护。两端 `i2c log on|off|changes` 开关逐帧监视，默认 off；changes 忽略序号及其 CRC，只输出语义变化与失败帧。`i2c stats` 查询总数 / 分类 / 最长耗时 / 日志丢弃，`i2c stats reset` 只清统计。监视队列最多 32 条，满时丢日志不阻塞总线。`pair-i2c-monitor.uart` 提供双端验证。LCD 本地模拟命令见下方。

ATOM 开发构建另支持 `i2c drop <0..10000>`、`i2c corrupt <0..10000>`、`i2c delay <0..200>`：丢弃后续合法请求的响应、翻转响应 CRC、延后发送毫秒数。0 取消；计数 / 延时重启清除，生产关闭模拟时不可用。会使 LCD 重试及断开，适用于开发验收。`pair-i2c-fault.uart` 验证两次 CRC 不断开、三次故障断开及恢复。LCD 本地提供 atom fail / crc / timeout（见下方）。

ATOM 开发构建还支持 `i2c req <9 个十六进制字节>`，可连写或逐字节空格分隔。例如 `i2c req a5 02 01 01 02 02 00 00 86`。请求排入 I²C 任务，先返回 `OK SIM i2c req queued token=N`，随后 `DONE SIM i2c req token=N status=OK reply=...`；错误 CRC 返回 `BAD_CRC`，无完整帧返回 `NO_RESPONSE`，格式错误直接 `ERR`。同一时刻仅允许一个请求；回放工具等待对应 token。复用实际协议解析 / 响应与 POLL ACK，虚拟请求不写真实 TX FIFO、不刷新 LCD 心跳、不计真实 I²C 统计、不消费故障计数。LCD 在线时会警告 ACK 可能确认真实事件；开发测试须考虑这一影响。生产关闭模拟时不提供此命令。脚本见 `tools/uart_scripts/atom-i2c-req.uart`。

ATOM 开发构建支持 `led test`：切换四角校准模式，每秒按左上红、右上绿、右下蓝、左下白循环。再次执行恢复正常渲染；`led fault bt|i2c|overflow on|off` 设置独立强制图案，优先级仍为蓝牙 → I²C → 溢出，真实故障保持。校准优先于故障图案。`led off` 清除校准与全部强制图案，真实故障不清除。设置只保存在 RAM，重启清除，生产关闭模拟后命令不可用。`status` 的 `led calibration=... forced=... SIM` 显示本地调试状态；它只改变 Matrix 显示，不向 LCD 伪报链路故障。回放脚本为 `tools/uart_scripts/atom-led.uart`，物理方向需观察灯阵确认。

LCD 开发构建支持 `atom sim on|off`、`atom online|offline|reboot`、`atom version 0..255`、`atom fail|crc|timeout 0..10000`。启用后停止访问物理 ATOM，经同一 CRC / HELLO / POLL 客户端处理；主动离线或切换来源唤醒通信任务释放旧输入。版本故障和重启经过原有判定路径；关闭后恢复真实链路。状态与界面保留 SIM 标记，即使模拟 ATOM 离线。设置只在 RAM，重启后关闭；关闭 `CONFIG_REMOTE_DBG_SIM` 不提供模拟命令。

LCD 模拟开启后还支持 `pad connect|disconnect`、`pad battery 0..10|none`、`pad gap|overflow`、`gimbal state off|search|connecting|connected`，以及同 ATOM 的 tap / hold / release / stick / trigger / shoot / record / seq。动作时序仍由 10ms 播放器执行；事件使用已有 ACK / gap 队列。左摇杆保留在模拟快照但现有协议不转发，云台仅模拟链路状态，运动不在范围。模拟拍照 / 录像仍会操作在线相机。默认本地回放脚本先要求 session=0，避免其中 RT 全压实际拍摄。当前 I²C 监视统计只记录实际总线事务，不混入本地模拟；脚本 `lcd-local-sim.uart` 包含两端检查，需已接 ATOM 验证启用期间心跳停止、退出后恢复。`lcd-local-sim-reset.uart` 在模拟开启时验证既有显示恢复和重启清除模拟，需要相机 session=0，包含受控重启且保留 NVS。


| 命令 | 功能 |
| --- | --- |
| `j` | 开始或恢复取景。已经在取景时忽略重复请求 |
| `S` | 切换设置显示：左侧 768×432 缩略图，右侧显示模式、ISO、快门、光圈、EV、白平衡、对焦、测光和闪光；下方增加画幅、驱动、效果、DRO、对焦区域、无线闪光及白平衡色温/原始微调编码 |
| `s` | 取消网络等待、清除控制请求、排空解码任务并关闭连接，画面停在最后一帧 |
| `p` | 做配对和同一 GUID 重连诊断。需要先停止取景 |
| `u` | 空闲时清除相机身份；先 `s` 并等任务结束，再清除和重新配对 |
| `help` | 列出当前命令 |
| `wifi show` | 显示 SSID、信道、密码长度、密码显示开关和客户端；加 `password` 才输出明文密码 |
| `wifi set ssid "My Camera" password "12345678" channel 6` | 设置指定字段，可只设置其中一个；校验通过后异步保存并重启热点 |
| `wifi display on` / `wifi display off` | 保存连接页密码显示开关，不重启热点 |
| `wifi newpass` | 生成 12 字符密码，保存并重启热点；成功后输出新密码 |
| `factory wifi`、`factory confirm` | 先申请，再在 10 秒内确认，只恢复热点配置，保留相机身份 |
| `factory all`、`factory confirm` | 10 秒内确认；停止相机并排空解码，恢复热点、清除 LCD 相机身份，成功后重启 |

Wi-Fi 修改先返回 `OK ... queued token=...`，随后 `DONE` 或 `FAIL` 表示执行结果。网络参数变化会断开相机 Wi-Fi，需要相机重新连接。默认 SSID 为 `easycamctrl`、密码为 `00000000`、信道为 6。`factory all` 处理 LCD 的 `wifi_ap/cfg`、`sony_remote` 和显示档位 `ui_prefs/info`，不清除 ATOM 手柄绑定或其他 NVS 数据；失败不重启，身份清除失败可能已部分修改记录，需查看日志。该路径已有主机测试，实机重置及重新配对仍待验收。

同一时刻只打开一个串口程序。`serial_log.py` 退出不会停止取景，加上 `--reset` 才会复位设备。

开发版本新增 `display bench`：暂停并排空相机任务，生成固定 1024×576、4:2:2、质量 80 的合成 JPEG，按当前全屏 / 设置模式显示二十帧，返回异步 token 和局部显示帧率，结束后恢复此前运行的相机任务。测试画面标有 SIM，不修改配对或显示档位 NVS。维护模式、已有 SIM 和重复基准请求会拒绝；生产版本不提供该命令。可回放 `tools/uart_scripts/display-bench.uart`，前提是 full 信息、全屏、SIM 关闭及维护关闭。该基准不包含相机 / TCP 取景耗时，不能用其 FPS 宣称真实取景达标。

`JPEG phases us` 每约五秒记录一次锁等待、清零、头部准备、解码、行距搬移、叠加层和 LCD 发布耗时（微秒）。`status` 与十秒健康日志新增 `min_internal`、`min_psram`、`largest_internal`、`largest_psram`；低水位使用 IDF heap API，自启动累计，包含初始化和测试的临时分配，不能当作正常取景的最低值。

约每 5 秒记录一次显示帧率，约每 10 秒报告客户端和内存。FPS 一开始显示 0.0，大约一秒后才有统计；暂停时保留最后读数。握手和网络收发每最多 100 ms 检查停止；任务退出还要排空解码任务，完整时延需实机验证。

## 画面上的参数

详细参数来自相机属性 `0x9209`，属性变化事件后刷新，并约每 5 秒轮询保底。`--` 表示相机报告“无值”，或这一项还没读到。

尚未确认的枚举继续显示原始十六进制。曝光模式 `0x00078051` 显示为 `MOVIE A`（视频光圈优先），测光 `0x8001` 显示为 `MULTI`（多重测光）。同一个数值出现在其他属性上时，含义可能不同。

`wifi show` 的 `default_password=1` 表示当前仍用出厂密码，`status` 的同名字段反映已发布给连接页的标签状态。自定义密码为 0；SSID 或密码显示开关不会改变该判断。

`ui info` 查询 LIVE 信息档位，`ui info full|compact|hidden|next` 异步保存到 NVS 后生效，DONE / FAIL 使用共享异步 token。DS4 触摸板按下也循环三档，按住不重复；X/Y 映射保留。设置菜单始终显示，隐藏档录制红点和模拟 SIM 仍可见。`ui-info.uart`、`pair-ui-info.uart`、`ui-info-persistence.uart` 分别验证本地输入、真实 I²C 输入与受控重启持久化；这些脚本要求初始为全显，结束恢复全显。

## 网页维护基础入口

maint on 在相机无会话时开启，允许相机继续连接；相机会话建立后自动关闭网页。取景中使用 maint on stop 显式停止 / 暂停相机；maint off 关闭并恢复自动连接。请求返回 token，等待 DONE / FAIL。maint status 输出模式、是否暂停、剩余时间和当前 PIN（不要复制 PIN 到公开记录）。

手柄也可开启：连接页长按 Select / Share 两秒切换；设置菜单最后的 MAINTENANCE 按 A，三秒内再按一次 A 确认停止取景。左右不会确认；B、移动光标、切页或输入缺口会取消。开启后回连接页读取地址 / PIN；维护中也可用 Share 长按、菜单 A、串口或网页关闭。`status` 的 `menu selected` 与 `maint` 不包含 PIN，可用于回放检查。

连接页显示维护地址与 PIN，网页可登录查看设备信息、修改热点 SSID / 密码 / 信道、生成随机密码、上传 LCD 固件、确认重启设备或退出。留空新密码会保留当前密码；保存返回排队结果，约 1.5 秒后应用并重启热点，重新连接后用原 PIN 登录。重启设备会关闭维护并保留 NVS。OTA 使用本项目应用 `.bin`，先检查版本 / 芯片 / 项目，确认后上传；网页发送进度满后还需等待设备校验。LCD 显示收到比例。成功后重启，新固件连续健康运行六十秒后确认，确认前复位会回退。

开发版本的 `maint probe` 用设备内部真实 TCP / HTTP 检查基础接口；`maint probe wifi` 临时修改热点并恢复，`maint probe reboot` 实际重启，`maint probe ota` 会把损坏和有效两份当前运行镜像依次写入另一 OTA 分区，验证失败后重试并重启。后者要求双 OTA 分区且当前固件已确认，不可用于只有 factory 的旧固件。测试期间会断开外部客户端。探测不打印 PIN / token / 密码；设备回环不证明手机 Wi-Fi 访问通过。

## 录像与扩展参数诊断

`status` 的 `record known/recording/pending` 分别表示录像状态已知、实际录像中和等待确认；参数写入接受不等于回读已生效。`extra status` 显示 ASPECT / MORE 子菜单是否开启、选中ID，以及九项扩展参数的实际值、可写状态、status、target_valid和target。status编码：0空闲、1待确认、2已回读生效、3拒绝、4超时、5命令接受。
