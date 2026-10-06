# 2026-10-06 独立 ATOM / SIM provider 与资源停止

main/atom_link.c/.h删除，物理provider迁components/app_input_atom。public input_atom.h只供Core start/stop；Core新增input providers composition，main通过Core启动，保留物理ATOM任务创建先于Wi-Fi的顺序。ATOM独占I²C device句柄，不删除board-owned bus；3072/prio4任务、100kHz、15ms写后延时、约50msPOLL、三失败阈值、1/5s退避保持。初始化/task创建失败归还registration/endpoint；stop检查<=25ms等待片，当前I/O最多原100+15+100ms，归还device失败则sole worker继续重试，超时不强删或覆盖资源。

SIM不再借ATOM transport。app_input_sim只Debug编译，独立原3072/prio3播放器任务，10ms player tick、50ms协议报告；private sim/client/player、4jobs/8completion。共用atom_protocol组件唯一编译common协议/客户端；每provider独立协议状态属于不同物理来源。raw报告仍归唯一Input状态机；normalized epoch/id、cached ACK/edge/gap/connected/电量/类型门禁一致。SIM client首两次失败不发布假断开，第三次断开，version/reboot走真实纯协议；报告不伪造物理atom_online。选择SIM不会停止真实I²C心跳，业务只消费selected source的报告。

Input向两个provider发布pad型命令，分别跟踪endpoint epoch补发；首次GET成功才发送。SIM UART控制为typed INPUT_SIM_COMMAND，SourceUART/current lifetime/targetepoch/deadline/flags/标量范围校验，Input独占PAD_KIND。SEQUENCE是readonly BULK lease sizeof(pad_sequence_t)，验证动作kind/方向/范围/等待/总时长后复制入4jobs，再归还lease；无需借用caller栈或NVS。结果8槽保留，UART拥塞重试；重复token或槽满拒绝。停止取消player及held raw state，未归还完成结果时保留worker直到UART admission成功或旧UART代次消失。UART关闭/重启会取消已完成HOLD留下的按键/trigger，不能只靠pending completion判断。

main/lcd_sim.c只剩UART encoder/结果poll；无sim/player/transport/provider状态。配置与sequence经Console，物理和模拟状态通过Input STATUS；旧ATOM诊断对象API删除，UART status改semantic source_epoch/report_id，bench暂通过消息查询。旧维护HTTP移除ATOM状态字段，最终无认证独占维护整体仍待迁。I²C monitor32归物理provider唯一task；UART LOG_MODE/STATS/LOG_READ只取copied values，main/i2c_console.c只格式化，不再provider→main/debug_printf反向调用。ATOM独立工程仍用原common/i2c_debug，源码未调整。

Core health先Input quiesce、再provider stop、再Camera drain。provider stop共用绝对预算，即使SIM超时仍尝试ATOM停止。服务Release拒绝SIM选择且不发SIM配置；SIM component在ESP-IDF提前解析阶段声明依赖/头，在关闭配置时为无代码interface，没有link archive/code/state。编译清单证明Debug协议/client/monitor/ATOM/SIM源各1份，Release无input_sim/atom_sim/pad_player/lcd_sim；ELF无SIM生命周期/命令桩及旧ATOM/lcd transport函数。

主机99/99（原54名称全部保留，未删原断言/降低告警）。新增真实物理provider+fake I²C测试首HELLO恢复pad型、RT同样S1/S2、首两失败保持、三失败释放、monitor消息/reset、初始化/创建失败/stop/device返回失败重试。真实SIM+协议/registry/owner/gamepad测试连接/类型/电量/云台、cached报告、RT/LT映射、CRC2/失败3恢复、readonly lease/copy/duplicate token、4jobs满/8completion满/满UART保留、离线/版本/stop/restart及UART代数取消completed HOLD。Core debug/release验证部分启动失败unwind、单一stop预算、SIM失败仍停ATOM；服务Release测试使用未定义关闭Kconfig，拒选/拒发SIM命令。fake调度/critical不是SMP/cache-off/实机证据。

最终五构建成功终态、尺寸{"default": "0x35af50", "stable": "0x35a130", "release": "0x34e480", "atom-debug": "0x1042a0", "atom-release": "0x101690"}，LCD<5MiB，Stable80MHz、ATOM双模BTDM/BLE/GATTC与Release符号通过。日志build/module-input-providers-{host-build,host,default,stable,release,atom-debug,atom-release,symbols,test-preservation}.log，sourcegraph/sizes在module-input-providers-graph.json。Debug提前依赖解析缺SIM头、Release关闭宏未定义的中间失败已修复，failure logs保留。硬件脚本lcd-local-sim/pair-i2c-fault/reset更新对应semantic状态、provider日志及持续物理心跳，不宣称已在硬件运行。无活跃build/串口，其他聊天未检查；未提交/推送/烧录。

完整目标active：UART独立gateway/所有Camera/UI/Wi-Fi命令、bench/fault归UI、UI/prefs/menu全stop、启动:80无认证独占维护、纯kernel旧MAINT动作删除、完整Core组合根/frozen订阅、legacy/死代码审计和完整资源/依赖图及[全清单](../development/module-split-checklist.md)仍待完成。

最终静态检查：module boundaries通过；109文档/500本地链接/0问题；git diff --check通过。
