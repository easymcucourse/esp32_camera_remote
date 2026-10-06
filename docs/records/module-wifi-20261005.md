# 2026-10-05 Wi-Fi 接口、驱动与消息 bridge 第一批迁移

完整目标仍为“完成完整计划并编译通过”。本记录为阶段2的部分落地，不能作为完整计划验收。

## 实际修改

- app_wifi 提供不透明对象、API version / capability、只读 ops factory contract、生命周期、网络状态 / generation 和 DHCP 客户端快照。错误统一分类，实现诊断仅作观察。创建失败不接管 factory context；重复启动拒绝、停止失败保留对象可重试、运行中不得销毁。
- 通用配置 / 校验 / 编码 / apply 回滚算法从 main 原样移到 app_wifi，100字节 NVS 记录格式未改。所有既有 host 测试保留原逻辑，只同步源文件与 include 路径。
- wifi_esp32 独占 AP driver / country / netif / DHCP / event 实现；factory 拒绝同时创建第二个 ESP32 radio 对象，部分初始化失败清理 event、netif、driver、锁和对象。同步 SDK stop 的 timeout 目前只限制锁等待，完整有界取消 / TCP 排空待下一批实现。
- 原 wifi_ap 配置 worker 已使用 app_wifi 生命周期与快照，不再 include esp_wifi/esp_netif。其 NVS / token / factory-reset / camera 租约仍在 main，后续须拆解，不能靠移动整个文件掩盖业务依赖。旧 camera_controller 中未使用的 esp_wifi include 已删除。
- app_wifi_messages 注册 WIFI endpoint，独立 task 回复发现 / 指定MAC RSSI / 网络状态。只依赖通用 app_wifi / app_console；其余 TCP/config operations 明确回复 NOT_SUPPORTED，尚未完成接入。
- Wi-Fi 状态及选中相机 RSSI 使用消息事件交给 UI endpoint；订阅处理更新 model 与连接页。发布失败不阻塞 worker，周期快照会再次发布。真实相机发现还使用旧 wifi_ap_get_clients，待 Camera endpoint / PTP 基类迁移。
- 新增 wifi_endpoint task：4096内部RAM、priority2、未绑核；原 wifi_config task 的4096/priority2及队列2保持。网络初始generation由后端启动置非零，restart/stop前递增，包括失败尝试。
- 建立 app_wifi / app_wifi_messages 的 SDK和跨业务 include 扫描。NVS、维护 netif及 PTP bare fd 边界还未收口，扫描仅约束已迁移 component。

## 验证

- host 61/61 通过（原54保留）。fake driver 覆盖版本 / 必需ops / 能力拒绝、启动失败、重复启动、容量不足不返回半份客户端快照、实现异常count、配置状态错误、停止超时后重试和运行中销毁拒绝。fake scheduler / endpoint 覆盖 bridge 注册与task失败、发现 / RSSI / 状态回复、错误分类、未实现操作、stop超时 / 重启和消息释放。
- cmake --build build、build/stable、build/ci-lcd-release -j4 均通过；对应日志 build/module-wifi-*.log。应用大小分别 0x353750 / 0x352930 / 0x346e10，均低于5MiB。
- Release sdkconfig 确认模拟 / 故障注入关闭；nm --defined-only 确认模拟与JPEG encoder禁止符号不存在，证据 build/module-wifi-symbols.log。
- 模块边界、文档链接与 git diff --check 通过。未烧录、未访问硬件；无 Wi-Fi 重启 / 实机相机 / LCD 冒烟 / 长期稳定性结论。

## 后续

下一步：把配置NVS / prepare-commit-cancel-result迁入 backend，跨域恢复出厂放入 app_core；实现带绝对deadline、取消、network generation和不透明channel的TCP API及Wi-Fi message租约交接；迁移PTP网络调用。其后继续输入provider、独占维护、相机backend/PTP基类、UART和最终清理。完整清单见 [逐项计划](../development/module-split-checklist.md)。
