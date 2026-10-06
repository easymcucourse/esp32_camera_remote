# 2026-10-06 公共接口与五配置核对

本批按计划 3.3/3.6 核对 public API 版本、生命周期和实际构建，完整目标仍进行中。

## 修改

- ATOM、SIM、Maintenance/Web/OTA public 头补齐版本常量；现有签名和行为未改变。boundary 门禁检查全部功能 public contract 的正整数版本。
- Input/ATOM/SIM 明确 Core 串行调用、重复 start、partial start 清理、stop 超时保留 owner、重试及 provider registration 返回要求。ATOM prepare 后启动失败仍保留设备供 stop/retry，不误称所有资源自动释放。
- 维护明确一次初始化、不可逆 phase、复制 callback table、context 生命周期和状态快照不代表 admission。
- 五配置复核发现 ATOM Debug/Release 缺 `i2c_monitor_result_name` 链接：前批纯 formatter 从 monitor 拆到 common 后未加入 ATOM 构建。补 `m5_atom_matrix/main/CMakeLists.txt` 的 `i2c_monitor_format.c`，两配置重建成功。
- 开发 build/serial 文档删除旧 `u`/正常配置写说明，修正 BLE 双模实际配置及已编译 Ultimate 2 parser；implementation-status 明确下方历史入口已被当前计划取代。验收清单更新有直接证据的行，不把硬件项目标为通过。

## 验证

`cmake --build build/host -j4` 后 `ctest --test-dir build/host --output-on-failure --timeout 20`：255/255，原 54 保留。

| 配置 | 目录 | binary size | 构建结果 |
| --- | --- | --- | --- |
| LCD Default | build | 0x358780 | 0 |
| LCD Stable | build/stable | 0x357970 | 0 |
| LCD Release | build/ci-lcd-release | 0x34c770 | 0 |
| ATOM Debug | build/ci-atom-debug | 0x1043f0 | 0 |
| ATOM Release | build/ci-atom-release | 0x1017f0 | 0 |

五个终态日志 `build/module-final-{default,stable,release,atom-debug,atom-release}.log`。首次 ATOM 链接失败已修复并重跑，最终表仅代表修复后的结果。LCD 三图各 20 project/bind nodes、92 declared edges，无项目环；实际 archive direct edges Default/Stable 41、Release 37 全通过，见 `build/module-final-{graph,symbols}-*.json`。SDK 内部图和间接 callback/ops 不在这个证明范围。

Release 按 `tools/ci_build.py` 当前禁止集合检查：LCD 23、ATOM 5 均不存在；关闭 REMOTE_DBG_SIM，LCD rollback 启用及镜像 <5MiB、ATOM BTDM/BLE/GATTC 启用。结果 `build/module-final-release-check.json`。这是本地检查，未执行远程 CI。

## 新发现的剩余偏差

`components/app_maintenance/maintenance_web.c` 仍 include `lwip/sockets.h`，stop 用 `httpd_get_client_list()` 取得 SDK 客户端后直接 `shutdown(..., SHUT_RDWR)` 唤醒阻塞 handler。计划要求维护无裸 socket/lwIP 旁路，因此 A18/V26 不能标为全通过。不能只隐藏 include 或将这个调用移成无语义 wrapper；下一步需要明确 HTTP session 退出所有权，并验证阻塞接收、失败 stop 保留 owner 和 shutdown/OTA 竞争。

本地 SDK 5.5.1 的 `httpd_sess_trigger_close()` 只是向 HTTP task 排队，并不立即中断当前 handler；`httpd_stop()` 同步等待线程结束，也没有项目级 deadline。直接替换之前必须考虑原唤醒行为及 10s recv/send timeout。原资源表已有此 join 限制，本批不增加严格有界声明。

无本轮烧录、实机冒烟、SMP/cache-off、HTTP STA 隔离、OTA 断电或稳定性证据；无提交/推送。其余完整清单、全部工具/文档旧路径、PTP/Sony 无调用删除依据仍待收尾。
