# 2026-10-06 Core配置任务与正常网络排空

新增 app_core_factory_quiesce：关闭恢复出厂请求 admission，原单槽队列中的未开始任务取消并写入原八槽 token history。用私有 wake sentinel 唤醒原 portMAX_DELAY worker；已开始的配置冻结/Camera预约/持久化事务仍完成原失败回滚与释放，成功仍500ms后重启。发送者计数保护从slot预约到queue send的持有期，worker在admission归还后排空queue再退出；Core成功停止后删除queue。超时不释放资源、不强删task且不重开请求；重复完成stop成功，之后可以start新worker。原4096/internal/priority2、queue1、结果8和事务逻辑保持。

Core重启现在先等factory停止，再允许Camera物理排空与UI偏好worker关闭；超时不主动删除仍被factory使用的Camera/UI/config资源。当前旧健康重启策略仍最终执行重启，所以不宣称维护激活成功。正常UART恢复出厂入口仍有过渡实现，最终计划要求Web-only，后续必须删除。

app_wifi新增可选 config_quiesce backend操作，wifi_esp32直接复用既有wifi_jobs_stop，只停止正常配置worker，不停SoftAP、不改network generation、不删除配置快照/token history或saved storage。stop取消队列中的任务，正在save/reconfigure/rollback的事务完成后worker退出；timeout关闭admission并保留owners，resume不能重新打开stopping worker。实现factory之后、所有正常config producers停止后由Core调用。维护写入下一次启动使用的版本化saved配置仍可用；完整Web配置事务尚未实现。旧无该可选operation的backend返回UNSUPPORTED，不冒充成功。

新增Core network owner，启动时绑定一次并核对Wi-Fi version/AP/store/config/TCP能力；main暂负责注入同一个wifi_ap_service，最终组合根迁移未完成。app_core_network_quiesce用同一等待budget先配置worker后app_wifi_messages_stop，后者取消/归还channel I/O lease并等两个TCP workers及endpoint worker结束，AP对象保留。当前Core重启只有UART/Input/providers/Camera/bench/menu均已停止且factory已停止才进入这条网络停止路径；网络停止成功和prefs停止成功后才关UI消息和renderer，避免网络状态继续发布到已关闭UI。

新增真实factory service fixture验证idle wake/重复stop、queued任务无写入取消、发送者尚持queue时close、超时资源保留/拒restart，以及UI持久化中close后原事务rollback/release/resume仍完成。旧factory成功/失败/八结果测试断言保持。fixture发现退出后再次发送wake的问题，已改为仅running时发送；初次失败日志保留。新fixture初次QueueHandle tag不一致及Core network fixture漏async_token链接均已修正，不降低-Werror或删除旧断言。

新增Core network+真实app_wifi facade fixture验证version/capability、重复bind、optional unsupported、config失败时bridge不关闭、bridge timeout重试、共享100ms budget扣除配置40ms后剩60ms、AP/status generation不变、token history/saved写仍可访问。backend配置停止使用fake，SDK Wi-Fi未在host运行。原真实wifi_jobs fixture扩展save回调内关闭，证明stop timeout后该物理apply仍完成、结果保留、worker确认退出，随后resume仍不接受新写。主机共115/115通过，原54注册和断言保留；fake不是SMP/NVS/cache-off/真实HTTP/硬件等待期限证明。健康任务整体停止交错目前仅源码观察，不以这些窄测试证明完整独占切换。

当前源码LCD Default `0x35bea0`、Stable `0x35b080`、Release `0x34fea0`均构建成功，小于5MiB/6MiB分区。三个compile graph均只有一份Core network/factory source，ELF实际包含factory/network/config/bridge stop生产调用；Release无SIM/bench/Debugfault/encoder/维护UART探针符号，Release无显示bench/UIbench/lcdsim源。模块边界/diff通过；文档最终检查补充如下。日志build/module-network-stop-{host-build,host,default,stable,release}.log；factory早期验证/失败日志build/module-factory-stop-*也保留，不作为最终源码构建证据。

ATOM本批未改未重建；无烧录、实机验证、提交或推送。全部本批build sessions终态，无本批后台串口/构建，其他聊天未检查。完整计划active；下一步System endpoint/router全局停止与启动组合根、独立无认证app_maintenance、SoftAP:80启动trigger/不可逆状态机、Web-only写入与旧认证/菜单/compat清理，最终逐项审计仍未完成。

最终文档检查119文档/520链接/0问题，diff通过。
