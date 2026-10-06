# 2026-10-05 恢复出厂协调迁入 app_core

完整目标仍在进行。本批迁移恢复出厂事务与异步协调，不把 Wi-Fi token/TCP 和其他剩余阶段算作完成。

- main/factory_reset.* 迁入 app_core 的 app_factory_transaction.c / app_core_factory.h；原事务逻辑及失败类别保留，另增加空参数/缺失callback拒绝。成功保留相机独占至重启；失败只尝试Wi-Fi持久化回滚，不声称恢复身份/UI已发生的修改。
- app_factory_service 复制组合根提供的ops，独立worker先冻结配置入口、等当前配置任务排空、重新读取运行配置，然后执行相机排空与多域持久化。失败恢复配置入口；成功保留配置/相机独占，结果可观察500ms后重启。
- 非阻塞请求队列容量1，结果历史8；仅允许一个pending事务；token分配器由组合根注入，使用当前全局原子async ID，避免与Wi-Fi结果混淆。队列满、部分初始化失败、未知/未完成/过期结果均明确处理。
- 新factory_all任务4096内部RAM、priority2、未绑核；原wifi_config4096/priority2及队列2保持。新任务资源须随最终资源表验收；本批无实际RTOS并发/硬件时序证据。
- main/wifi_ap删去reset-all事务分支与相机身份/UI偏好实现include，只保留现有Wi-Fi配置worker及临时core请求/结果适配。服务组合的相机/UI持久化callbacks放在app_main；后续迁移完整app_core_start时一并移动。wifi_ap_service/config_freeze/config_resume明确为临时组合接口，最终须清理。
- 配置冻结的生产实现借用现有enqueue mutex关闭新请求，观察8个结果项等已有事务完成；5秒超时自动恢复入口。该额外排空门禁为跨域reset独立化所需，主机service测试使用fake freeze，真实配置并发/时序仍需后续backend worker回归。

验证：host63/63通过，原54保留。原factory_reset测试只更新source/include位置，原失败与回滚断言全部保持；新增fake service验证无效ops、队列/task创建失败清理、send失败、重复request、pending结果、冻结失败不执行相机步骤、相机忙/事务失败恢复入口、八槽历史淘汰、成功后拒绝新请求及重启门禁。LCD default 0x353d40 / stable 0x352f20 / release 0x347420均编译通过，均低于5MiB，Release禁止模拟/JPEG编码符号检查通过；boundary、文档链接及diff检查通过。证据build/module-core-factory-*.log。

没有烧录或硬件操作。下一步继续把Wi-Fi异步配置worker/token/current snapshot迁入wifi_esp32及app_wifi/message接口，再完成TCP/PTP网络、输入providers、维护独占、相机backend与UART清理；完整验收见 [计划清单](../development/module-split-checklist.md)。
