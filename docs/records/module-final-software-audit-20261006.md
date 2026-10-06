# 2026-10-06 最终软件验收范围核对

以原计划阶段条目、验证矩阵和38项验收标准逐项核对当前清单。build/module-final-software-audit-checklist.json保存各编号的要求/状态/证据分类；它不是运行pass报告，清单缺证据/部分项不能判定计划完成。

本批纠正范围：S1.5要求创建/迁移独立Core，真实component/health/restart/编排和fixtures已证明该实现要求，真实生命周期另由V18/A31验收；S3.2要求删除认证并说明策略，生产源与当前README/user-guide/design已满足，历史auth记录/test-only保留有范围说明，真实热点可达性仍由V21/V22验；S4.1纯backend契约/fake ops与实际Sony绑定已证明；S5.8要求CI增加检查，ci.yml与ci_build.py已注册source/private、lease regression/sanitizer、actual graph/direct archive门禁，正反例本机通过，远程Actions没有执行，动态/SMP路径不扩大门禁结论。

原计划局部条目与最终独立维护规则存在差异，不能恢复被要求删除的入口：V23的UI fake Wi-Fi编辑器只在历史fixture，当前真实UI网络行信息展示独立构建；Camera discovery/PTP/Sony目标继续无app_wifi/lwIP并共享fake Console。V24的普通bridge配置写已按S3.6拒绝，仅Web直接isolated app_wifi的prepare/commit/cancel有效。test_app_wifi_messages真实bridge+facade+fake driver验证四退休消息NOT_SUPPORTED；test_maintenance_web独立目标验证ACK成功才commit、ACK失败cancel、commit失败cancel。V35原可逆系统lease要求与最终不可逆MAINT差异已在清单注明；不恢复正常模式以满足旧release表达。

本次9条相关CTest（app_wifi_messages/camera_discovery/PTP client/protocol/Sony/UI menu及3Web）通过，build/module-final-audit-contract-host.log；最近完整262通过与LCD三配置重建见Sony exposure收尾，ATOM两构建源未变。最终清单仍保留真实RGB/DMA/cache-off、SMP owner组合、启动I²C/AP安全、HTTP接口隔离、独占维护、保存/重启/OTA竞争及新Sony实机冒烟待验。不能把源码/host证明当硬件验收。

只读串口枚举当前仍COM11、COM101，历史LCD COM8/ATOM COM6未出现；没有打开端口、读取原始设备身份或烧录，设备对应信息等待用户。全部当前文档例子语义/真实脚本回放仍另列S6.3/S6.4/A38。无新生产修改、remote CI、提交或推送。
