# 2026-10-06 UI Wi-Fi 菜单停止

Wi-Fi菜单新增关闭admission、有界quiesce与cooperative任务退出。Core重启在Input/provider/Camera排空后调用 app_ui_menu_quiesce，再停止偏好worker。菜单原4096内部RAM/优先级2、queue16、50ms周期、48字节内job metadata保持。新菜单动作拒绝，已进入admission路径先归还再清除page状态；超时保留任务与队列并拒重新start，后续quiesce可继续等待。任务结束才删除队列，完整stop可restart。

菜单普通RPC改用router cancelable request，关闭先检查predicate，阻止后续COMMIT，已等待请求按<=25ms取消片返回。已知Wi-Fi staged token发送取消，控制队列满时保留worker、10ms重试；source/target endpoint生命周期变化则退休旧token，不把旧取消发给新域。正常结果查询也核对两端生命周期。已COMMIT的配置及System恢复出厂仍归其域，不因菜单退出撤销；最终Core必须另行freeze/drain Wi-Fi/System jobs才能进入独占维护。菜单quiesce成功不能充当配置排空证明。

扩展原ui_wifi_menu真实kernel/model+fake router回归，保留59阶段旧脚本与原断言。覆盖停止超时/queue保留/start拒绝、关闭后动作拒绝、重复quiesce/restart、入队与关闭交错、PREPARE返回时关闭不COMMIT、cancel拥塞重试、Wi-Fi endpoint退休不发旧token取消。主机111/111通过，-Werror保留；fake调度非RTOS/SMP/实机证明。

最终固件构建数值补充见下方。日志build/module-ui-menu-stop-{host-build,host,default,stable,release}.log。ATOM本批未改未重建，前批双模成功证据保留；未烧录、提交、推送或实机测试。UI endpoint/连接页refresh/renderer完整停止与固定MAINTENANCE画面仍缺，Core完整组合根、启动无认证不可逆独占维护、legacy API清理继续，完整目标active。

最终LCD Default `0x35b490` / Stable `0x35a670` / Release `0x34f410` 构建成功，均<5MiB，ELF含菜单quiesce/可取消RPC路径。boundary/docs/diff通过（116文档/514链接/0问题）。创建阶段未发布admission的交错新增回归通过，防止创建失败与UI请求并发读取队列。全部本批构建会话终态，无本批后台串口/构建；其他聊天未检查。
