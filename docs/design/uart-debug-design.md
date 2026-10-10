# UART 调试控制台设计

[English](../en/design/uart-debug-design.md) · **简体中文** · [日本語](../ja/design/uart-debug-design.md)

本文按当前源码描述LCD消息gateway与ATOM独立串口；命令用法集中于 [串口手册](../user-guide/serial.md)。原 [UART需求](../request/uart-debug-request.md) 保留行输入、请求号、脚本与调试能力，配置保存/维护入口按最终 [拆分计划](../../main-module-split-plan.md) 替代。

## 1. 模块与依赖

LCD app_console同时拥有router与UART gateway。app_console_uart.c组合命令encoders；camera_commands、uart_status、wifi_console、i2c_console、ui_preferences_console及Debug lcd_sim/display_bench只包含message contract/common值逻辑，不能调用功能门面或include私有头。不存在app_diagnostics组件。维护Web不依赖Console，独占时Core停止普通UART/router。

共用common/debug_console、debug_line、debug_args、async_token提供UART读取、解析/输出与token；common_runtime负责LCD基础编译，纯I²C结果formatter独立于物理ATOM provider。ATOM的atom_dbg_cmds仍归m5_atom_matrix/main，独立固件直接操作其本端所有者；不能把LCD跨模块门禁套到另一固件的组合接口。

## 2. UART owner

两端UART0/115200，RX缓冲512、debug_console task4096-byte内部栈/priority2，read20ms。单消费者，poll在读取间执行。行容量256（最多255内容字节），CR/LF结束，CRLF只输出一次；退格删除最后字节。无效控制字符/非ASCII/超长行丢弃到下一结束符，不能部分执行命令。参数使用debug_args纯C解析单双引号和反斜杠，不使用esp_console注册/linenoise；无echo on实现。

可选#uint32请求号由debug_request_id解析，答复保留编号；debug_printf序列化一行输出，不能把它当跨task业务同步。异步token由公共async_token分配非零值；DONE/FAIL属于具体endpoint完成消息，不将queued当业务完成。

Core启动gateway前注册UART endpoint8 control/1 bulk与必要订阅，正常启动结束freeze。停止先关闭endpoint admission/取消业务request；reader退场callback只retire自己的endpoint，不能在reader中join自己或停止业务owners。外部quiesce等待worker退出并uart_driver_delete，失败保留worker/cleanup_pending；重试不能创建重复reader。UART读取失败同样只影响gateway，Core停止顺序保留其他owners到安全排空完成。

## 3. LCD 命令路由

| 类别 | 当前行为 |
| --- | --- |
| help/version/status/log | 保留生产命令；status并行收集typed快照，缺任一快照输出ERR，不打印零值OK |
| j/s/S/p | Camera start/stop/diagnostic或UI菜单message；s的OK仅请求接受，Core维护stop等待physical drain |
| wifi show [password] | 只读配置与客户端查询，password显式输出明文；无set/newpass/display保存 |
| ui info / ui pad | 查询本次启动加载偏好；参数修改及触摸板循环动作已删（Input服务忽略该action），改由Web重启生效 |
| i2c log/stats | 查询/设置物理provider诊断，仅message，不操作device |
| Debug SIM | raw typed命令/readonly sequence lease交独立SIM provider，4jobs/8completion；经统一Input report API |
| Debug display fault/bench | typed UI请求；benchmark在UI复用真实JPEG renderer，经Camera消息预约停止/恢复，gateway不取得画布 |
| factory/u/maint/probe | 退休入口，不执行；配置/恢复/维护仅启动页Web |

Debug实现由REMOTE_DBG_SIM与APP_DEBUG_FAULT_INJECTION编译门控；Release不链接模拟/player/benchmark/encoder或故障实现，生产help/status/control保留。真实metadata/SIM空组件注册与ELF禁符号检查分别证明不同范围，见 [模块图](module-dependency-graph.md)。

## 4. 模拟与脚本

LCD SIM生成normalized report，物理ATOM仍轮询。Input只选择一个source，切换完整release后baseline；停止SIM取消排队动作和已完成HOLD残留。模拟快照/SIM标记只在RAM，重启清除；拍照/录像动作仍可操作在线相机。ATOM模拟经过本端协议/快照处理，不刷新真实LCD心跳的虚拟i2c req有独立语义；详细故障/LED用法见手册。

pad_cmd纯C解析动作，pad_player10ms按绝对deadline推进；half/full取值落入 [扳机阈值](gamepad-design.md#5-扳机状态机)。不让UART task等待完整动作时长。uart_script.py负责请求号、目标端、expect与token终态，serial_log.py负责记录。旧wifi-edit、factory、ui-info保存及maint脚本只作为历史证据，不能直接用于当前smoke验收。

## 5. 验证

真实common reader fixtures覆盖UART失败/driver清理/单worker/退出callback；Debug/Release gateway fixtures覆盖订阅失败、启动失败、未知维护命令、freeze及quiesce。独立encoders使用fake Console/Camera/UI/Wi-Fi端点，无功能实现链接。provider和report equivalence测试核对同一normalized report时间线全部动作字段；不代替真实按键、串口时序和I²C端到端验收。

当前边界、真实静态库直接符号边与Release裁剪由CI门禁检查；远程CI尚未执行。主机/编译及剩余硬件范围见 [清单](../development/module-split-checklist.md)、[资源表](module-resource-ownership.md)。
