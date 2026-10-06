# 2026-10-05 Wi-Fi NVS 边界迁移

完整目标仍在进行。本批落实 Wi-Fi 持久化边界和初始化分离，异步 token、TCP、输入、维护、相机与UART仍待完整迁移。

- app_wifi 增加显式 init、CONFIG_STORE能力以及同步保存配置读写。没有该能力的实现返回 UNSUPPORTED；声明能力但缺失ops的factory被拒绝。同步写入只改变持久化，不重启网络或冒充运行配置；后续运行时prepare/commit/cancel/result由backend串行worker统一拥有。
- wifi_esp32 factory只创建对象和锁，NVS读取可在LCD初始化前执行；radio/netif/event/country在app_wifi_init中初始化，保持原ATOM之后的硬件启动位置。初始化失败释放已创建radio资源，保留对象供重试或销毁；原任务创建参数没有改动。
- wifi_saved_config私有实现接管wifi_ap/cfg读写；100字节编码与默认配置相同，默认配置用定点erase_key，不抹去其他namespace或键。无记录用默认；损坏/过长记录按原政策修复；修复失败明确返回错误，启动组合继续用默认。
- main/wifi_ap不再调用NVS API；配置worker的保存callback改用app_wifi接口。NVS算法没有下沉相机身份/UI偏好或重置产品策略。factory-reset协调和async token仍在main，本批不宣称其已完成。
- 模块扫描禁止wifi_esp32外打开wifi_ap NVS namespace。

验证：host62/62通过，原54项保留。新fake NVS验证缺失namespace/key、有效记录、坏版本/过长记录修复、erase/set/commit/open失败、失败后旧记录保持、默认值定点删除、格式不变和其他键不受影响；通用对象测试补充init失败/重试及能力拒绝。三种LCD构建通过：default 0x3539b0，stable 0x352b90，release 0x3470a0；均低于5MiB，Release禁止模拟/JPEG编码符号检查通过。证据build/module-wifi-store-*.log。boundary/doclinks/diff检查通过。

本批没有烧录或硬件访问，实机无线重启、LCD/相机冒烟和长期稳定性仍待验证。下一批迁移backend异步配置worker与core恢复出厂协调，然后TCP deadline/cancel/channel、bridge及PTP调用；完整目标不缩减，见 [逐项清单](../development/module-split-checklist.md)。
