# 2026-10-06 Core 原子模式与 UI 正常许可

Core 新增私有原子模式 STARTUP、NORMAL、ACTIVATING、MAINTENANCE、RESTART。只有 STARTUP 能申请维护；正常许可与维护申请采用同一 CAS，不能同时成功。正常许可幂等，ACTIVATING/MAINTENANCE/RESTART 拒绝正常许可；维护激活只接受 ACTIVATING，重启后无回到 STARTUP 的路径。没有固定启动窗口计时器。当前生产 System status 和 ENTER_NORMAL 使用该状态，维护申请/激活/重启策略尚未绑定完整生产切换，不能据此宣称 S3.4 完成；旧维护状态暂仍覆盖 System status。

公共纯值消息追加 ENTER_NORMAL 与原因，不改变已有枚举值。System 验证 UI 来源、request/无 lease、原因、deadline 和 endpoint generation，再决定许可。UI 在有效 JPEG 解码前、设置页打开前、Debug 显示基准启动前请求许可；获批后按 UI endpoint generation 缓存，避免逐帧 RPC，失败不缓存。JPEG 许可失败走原丢帧结果与 lease 归还路径，不触发显示恢复；设置页拒绝不改变模型。Debug 基准也保留原 System/Input/SIM preflight。当前尚无真实启动 HTTP trigger，其他直接显示/恢复路径仍需最终审计。

app_wifi 新增可选 Core-only config_start，要求此前 config_quiesce 成功且正常 producers/router 已停止。复用同一配置 worker，不重建 AP、不重新开放正常 bridge/TCP，保留 saved/current/旧 token 历史。分配失败维持 admission 关闭并允许重试，重复启动拒绝；Core 最终维护切换尚未调用此接口。已有真实 wifi_jobs fixture 验证停机期间原事务完成、失败重启闭锁、成功重启保留旧结果并可执行新的维护配置。无线/NVS 实际操作仍为 fake。

169/169 主机测试通过，原54与旧断言保留。新增真实原子策略测试包含300次两个 pthread 并发争抢；UI helper 测试真实消息构造、失败不缓存、成功缓存、epoch 更新和 reply 归还；原 Core factory/System、UI frame/menu/endpoint/bench 与网络 fixture 补充正常许可和配置 worker 重启断言。首次 host 使用 ATOMIC_VAR_INIT 在本机 GCC16/C23 不可用，改为标准 aggregate 初始化；新网络 fixture snapshot 变量错误已修正，没有降低告警或删除旧断言。

最终 LCD Default 0x35be80、Stable 0x35b060、Release 0x34feb0 构建成功，均小于5MiB/6MiB分区。三种 compile graph 中 Core mode/UI mode helper 单一 owner；三种生产 ELF 有 get/enter_normal/UI helper，Release 无 bench/SIM/encoder/维护 UART/debug fault 检查符号。维护 claim/activate 未生产绑定，链接回收不作为实际进入证据。日志 build/module-core-mode-{host-build,host,default,stable,release}.log。ATOM 本批未重建；前批双模结果不能替代本批验证。

本批构建会话全部终态，无本批后台串口/构建，其他聊天未检查。未烧录、实机、提交或推送；没有 SDK/SMP/cache-off/性能/长期稳定性验证。完整目标仍 active。下一步是独立维护 lifecycle 与任意 HTTP 首请求 trigger、Core 全局停止和固定画面绑定、配置 worker 交接，随后删除旧 maint_mode/compat/WHOLE_ARCHIVE，并完成 Web factory 与全项审计。

本批边界/文档/diff 检查通过：125文档、538本地链接、0问题。
