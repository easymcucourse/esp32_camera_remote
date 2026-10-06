# 2026-10-06 网络换代事件与消息契约纠正

真实 `app_wifi_messages.c` 发布 WIFI_NETWORK_CHANGED 时曾填入 `payload.channel.generation`，Camera endpoint 则读取 `payload.network.generation`；union 内这两个字段的位置不同，因此已发布的通知会被 Camera 以 INVALID_ARG 拒绝。改为发布 network 字段，与消费者一致；channel 请求仍保留独立 channel generation。

原 host Wi-Fi bridge 测试只检查 request/reply，fake 时间不触发周期事件且 send 丢弃事件。增强原 fixture：每次接收推进250ms，直接检查真实事件发布的 source/flags/no lease、envelope及network generation；首个换代事件返回TIMEOUT，确认下一轮重试，成功后不重复同代。修复前断言失败证据 `build/module-network-event-repro.log`；修复后保留全部原断言并通过。Camera endpoint 的真实处理 fixture 和 producer 网络换代取消 fixture 同在完整回归中；它们下层仍是 fake，不冒充真实线程/网络集成。

同时纠正 `app_message.h` 过时说明：UI_PREFERENCES 只有 GET，写/reset编号仅保留ABI并拒绝；Wi-Fi菜单行仅显示信息，没有旧写worker；正常Wi-Fi配置请求仅GET，其余退休ID拒绝。补充所有envelope共用的任务调用、deadline/相关ID、代际、内联/lease所有权及错误语义；不能将运输成功当成操作成功、timeout当成撤销业务或buffer可复用。

验证：HOST **261/261**、LCD Default **0x3586c0** / Stable **0x3578a0** / Release **0x34c690** 三构建终态0；均小于5MiB。三archive直接符号边41/41/37通过；Release23禁止符号未出现。日志与JSON均在 `build/module-network-event-*`。ATOM未改，沿用此前两构建。没有烧录、实机、远端CI、提交或推送。

S1.10仍需逐操作完整审核；本批发现说明原笼统“消息路径存在”证据不能证明每个payload字段一致。真实网络换代/阻塞取消/SMP及时性与计划其余硬件要求继续待验证；设备信息尚待用户确认。
