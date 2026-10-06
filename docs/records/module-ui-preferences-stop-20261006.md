# 2026-10-06 UI 偏好任务停止与资源归还

UI偏好保存增加关闭入队/有界quiesce，Core重启在Input/providers与Camera排空后调用 app_ui_preferences_quiesce。新请求拒绝，已入队但未执行的写取消；正在提交的NVS操作完成并归还句柄后才允许资源释放。同步请求仍由worker给原caller semaphore通知，caller持有自己的栈上下文和binary semaphore，shutdown不撤销它。

使用原内部3072字节/优先级2 worker、requests4/results8/mutex和NVS ui_prefs/info,pad。正常运行保留portMAX_DELAY队列阻塞，关闭用private wake唤醒，队列满时已有请求会唤醒；关闭期等已进入admission临界路径归还。队列result消费者也纳入admission计数，避免释放与legacy结果读取交错。worker停止后才删除两个队列/锁；超时保留所有资源并拒绝重新start，下一次quiesce可继续等待。UART拥塞完成事件循环检查关闭标记，legacy未读取结果在最终清理归还outstanding。完整停下后可再次启动并重载NVS。

扩展原ui_preferences回归，原断言保持。覆盖关闭超时资源保留、重新start拒绝、取消排队legacy/typed写、入队与关闭交错、提交中关闭（同步caller semaphore仍存活）、未读取legacy结果、UART满时取消事件等待、重复quiesce与完整restart。初次新增提交场景错误预期只有一个semaphore，实际还有caller-owned sync semaphore；纠正新断言为两个后通过。该fake不证明真实NVS cache-off/SMP/RTOS行为。

主机111/111通过；固件最终尺寸补充见下方。日志build/module-ui-preferences-stop-{host-build,host,default,stable,release}.log。ATOM本批未改未重建，上一批双模成功证据保留；未烧录、实机、提交或推送。Core目前仍在超时记录后重启；最终独占维护须等待各模块真实quiesce完成，不能依据当前重启日志判断进入维护成功。

这只完成UI偏好任务停止。菜单/endpoint/连接页refresh/renderer完整停止和固定MAINTENANCE锁，Core全局资源编排与不可逆独占维护仍缺；完整计划active。

最终LCD Default `0x35b120` / Stable `0x35a300` / Release `0x34f0b0` 构建成功，均小于5MiB；三ELF含生产preferences quiesce路径。boundary/docs/diff通过（115文档/512链接/0问题）。全部本批构建会话终态，无本批后台串口/构建，其他聊天未检查。
