# 2026-10-06 维护 HTTP 退出边界

修复[上一批审计](module-api-build-audit-20261006.md)发现的 A18/V26 偏差：维护应用不再 include lwIP，也不直接 shutdown HTTP 客户端。`app_maintenance` 的 CMake 删除直接 lwip 依赖。

## 所有权与停止语义

外部 Core 先关闭业务模式，再调用 Web stop。Web 的 atomic stopping gate 在连接关闭前发布；dispatch、404/405 handler 和 available 均拒绝新业务，JSON 接收在每次 recv 前后检查，停止后不再提交刚收到的配置。SDK client-list 返回的 session 标识只用于 `httpd_sess_trigger_close()`，socket 生命周期和 transport 操作仍由 SDK 负责。逐连接关闭请求失败或客户端列表失败仍继续 `httpd_stop()`；join 失败保留原 server 与关闭的 admission，重复 start 拒绝，stop 可以重试。成功 stop 消费 server；重复 stop 不重复关闭。

SDK 5.5.1 的 session-close 是排队操作，**不保证立即唤醒当前 recv/send**。原 10s I/O timeout、HTTP 6144内部栈/priority3/3 clients/15 routes 均保留。Core 在活动维护退出时先进入 RESTART，注入 OTA 的 shutting_down callback 在接收之间返回 true；进入 NORMAL 时还没有启用维护上传。JSON 停止检查覆盖当前段是最后一段的情况。已经开始执行的持久写/OTA commit 不会被粗暴撤销，其原终态清理与重启约定保留。

`httpd_stop()` 的 SDK 同步 join 无项目 deadline；本批不声称解决严格有界停止。真实阻塞网络/SMP/Flash/cache-off 下的停止时延仍待实测。

## 验证

- 完整 CTest 259/259（原54保留），日志 `build/module-http-stop-host.log`。
- 最后加强 fake SDK close 时的新请求拒绝断言后，维护相关53/53重跑，日志 `build/module-http-stop-maintenance.log`。新增中途/最后一段 JSON 接收遇 stop 均无 save/restart；client-list 失败、逐session-close失败仍join；failed stop 保留owner/closed gate且重复stop再次请求关闭。fixture模拟外部owner事件，不证明真实线程调度或网络时延。
- LCD Default `0x358760`、Stable `0x357950`、Release `0x34c730` 均构建终态0、<5MiB；日志 `build/module-http-stop-{default,stable,release}.log`。ATOM源未改，沿用上一批 Debug/Release 构建。
- 三实际图各20 project/bind nodes、91显式边（删除维护直接SDK lwip边）；archive direct symbols Default/Stable41、Release37通过。机器记录 `build/module-http-stop-{graph,symbols}-*.json`。
- boundary 新增维护 raw socket/lwIP include/调用拒绝门禁；生产源扫描通过。无更改 SDK、无新增 HTTP task。
- 最新 LCD Release 23 禁符号均不存在，记录 `build/module-http-stop-release-check.json`；boundary、149份文档/584本地链接零问题及 diff whitespace 检查通过。

无烧录/实机/远端CI/提交/推送。完整计划剩余全项审计、文档/工具旧路径和实机验收继续保留在[清单](../development/module-split-checklist.md)。
