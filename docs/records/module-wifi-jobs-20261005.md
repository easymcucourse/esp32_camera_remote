# 2026-10-05 Wi-Fi 异步配置 worker / token 迁移

完整目标继续进行。本批完成配置状态与应用边界；TCP及其他计划阶段仍未完成。

- wifi_esp32持有每实例wifi_config_jobs，包含运行配置、country上限、队列2、enqueue锁和结果8。prepare复制排队但不写NVS；commit一次性释放、最多延迟10秒；cancel仅在commit前有效；已取出的未commit请求最多等待25秒。过期/取消不调用save或restart。
- 原wifi_apply_config算法及NVS格式不变。worker执行save/restart/失败回滚，只有成功才更新运行快照；失败返回统一分类，具体NVS/SDK诊断留在backend状态。启动task失败撤回radio启动，stop关闭admission、取消等待/队列并等待worker结束；超时保留对象可重试。
- freeze关闭admission并等待已有事务完成，超时自动resume；core恢复出厂改用该通用门禁。同步持久化写入仍不代表运行配置变更。原wifi_config任务4096内部RAM/priority2/未绑核与queue2保持，context改为backend私有对象；周期状态发布移到现有wifi_endpoint，无新增状态task。
- app_wifi追加CONFIG_ASYNC capability、get/apply/commit/cancel/result/freeze/resume接口及PENDING/NOT_FOUND分类。声明能力却缺ops拒绝bind；只提供AP的fake实现可独立替换。Wi-Fi与core reset共用配置token分配器；其他域仍使用各自上下文token，不用同一result查询。
- app_wifi_messages接入配置GET/PREPARE/COMMIT/CANCEL/RESULT和SELECT_CAMERA；只转换值类型，未包含NVS/driver/backend私有头。周期200ms发布完整网络/配置状态，2秒发布选中MAC的RSSI；generation变化发布NETWORK_CHANGED并由UI清空旧RSSI。订阅事件非阻塞，队列满后下个快照重试。正常Camera生产调用仍旧wifi_ap兼容函数，后续须换消息。
- main/wifi_ap删除运行配置、队列、结果池、NVS及worker，只保留兼容适配和暂时服务组合。旧API和临时bridge选择入口将在Camera/UI/维护及组合根迁移后删除；本批不据此声称正常模块跨域依赖全已收口。

验证：host64/64通过（原54保留）。新增真实wifi_config_jobs+fake RTOS回归，覆盖queue/mutex/task创建失败、重复启动、prepare不修改配置、cancel无I/O、延迟commit/重复commit/commit后cancel拒绝、25秒等待过期、queue满、freeze超时恢复/成功拒绝新请求、driver失败后回滚、save错误分类、历史8淘汰、stop超时/取消/重试及restart。bridge fake driver验证配置值/提交延迟/token/结果分类。fake scheduler未证明真实RTOS并发和硬件效果。

Default 0x354110 / Stable 0x3532f0 / Release 0x347770均编译通过、低于5MiB；Release禁止模拟/JPEG编码符号不存在，证据build/module-wifi-jobs-*.log。边界、文档链接与diff检查通过；无烧录或实机操作。

下一步实现Wi-Fi TCP不透明channel、绝对deadline/取消/network generation和lease message，迁移PTP网络；再继续输入provider、独占维护、相机组合backend、UART与完整组合根/兼容清理。完整条目见 [清单](../development/module-split-checklist.md)。
