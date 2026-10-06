# 2026-10-06 正常 Wi-Fi 只读与 Web 保存后重启

按计划3.9/S3.6，正常配置写入口全部迁出：UART wifi只提供show/password查询，set/display/newpass不发送消息；正常Wi-Fi endpoint删除PREPARE/COMMIT/CANCEL/RESULT执行分支，CONFIG_GET/STATUS/发现/RSSI及相机选择保留。Core未引用的request/prepare/commit/cancel/result/freeze/resume兼容函数删除。LCD删除wifi_menu纯核与4096内部RAM/pr2/16项队列编辑worker及启动/停机接口，SETTINGS网络行只显示信息，普通菜单不再路由到Wi-Fi编辑器；其他相机/MORE导航保留。

原编辑器、无reset版本纯核、UART配置编码和其菜单路由归tests/support/legacy保留回归目标/断言，生产无这些源。真正生产Wi-Fi endpoint fixture改验所有旧写消息NOT_SUPPORTED；UART新增真实编码fixture只stub查询RPC，没有配置writer，验证旧参数无RPC。生产UI菜单fixture改验Wi-Fi行HANDLED且wifi_menu=false；UI启动失败清理现在只有preferences，旧双owner计数改为1。UART gateway release旧断言依赖已删wifi poll，改验保留poll至少1且旧wifi poll被调用即assert失败；Debug其他poll不变。Core停止删除不存在的menuowner阶段/失败场景，保持其他所有失败路径。

Web热点保存此前仅重启AP，不满足成功后设备重启。新增注入wifi_committed(token)：handler在changed情况下预约重启、staged prepare，ACK成功后commit交给Core。原health的maintenance_poll在MAINTENANCE查询唯一token，PENDING不动、结果成功才提交1500ms设备重启、失败或未知结果取消预约；不要求浏览器继续请求，保留AP配置worker原应用/回滚行为。ACK失败或COMMIT拒绝时取消staged请求与重启预约，不写配置。所有设置变更（包括密码显示偏好）完成后重启；no-change不预约。页面新增reboot_after_apply说明，原restart_in_ms仍表示AP应用延迟。没有新任务/队列或提前重启中断NVS写入。

HOST231/231，含Core异步配置success/failed/pending重启、Web成功通知/ACK与COMMIT失败不通知、正常写入口拒绝及保留原54基线。Node --check通过。LCD Default0x3589f0、Stable0x357bd0、Release0x34c9e0三构建终态0，<5MiB/6MiB分区。实际compile graph无Wi-Fi编辑器源，三ELF无旧编辑器/stop/poll/compat写符号，Core completion watcher/shared Web commit仍在。日志build/module-wifi-readonly-{host-build,host,default,stable,release,graph}.log。ATOM未涉及未重建，无烧录、实机、提交或推送。

本批完成普通Wi-Fi配置写路径删除，不能宣称完整计划完成。UI旧wifi view/model/renderer死分支尚待下一轮清理；A35基础UI/Wi-Fi→trigger→正常准备顺序/boot barrier/partialinit、UI模型清空/freeze、最终S/V/A38逐项审计和实机证明仍未完成。真实AP变化/Flash/cache-off/重启时序没有由fake证明。

最终boundary/doclinks/diff通过，131文档549links0问题。controller热点标题变更后保留旧热点页anchor，修复README链接兼容。全部本批构建handles终态。
