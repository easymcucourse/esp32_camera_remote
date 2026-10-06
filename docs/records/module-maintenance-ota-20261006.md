# 2026-10-06 独立维护OTA与Core启动健康检查

本批建立components/app_maintenance并将实际OTA HTTP check/upload/status、boot状态查询与ota_header校验移入该组件。main/maint_ota.c、maint_ota.h、ota_header.c/.h已删除；旧Web改用app_maintenance_ota.h，main CMake不再注册OTA源。完整维护Web、trigger、认证与旧maint_ctl仍在main，main WHOLE_ARCHIVE仍为迁移期依赖，不是S3.1全部完成。

app_maintenance不包含相机/UI/Input/Console/Core/旧维护头，也不直接调用这些正常功能。上传许可及prepare/cancel/commit reboot由Core注入ops，初始化校验所有回调、只允许一次成功初始化。Core app_core_ota_compat当前仍连接旧控制器的上传许可，此临时adapter须随不可逆Core维护mode一起删除；不能据此宣称原相机预约控制器已消失。module boundary新增组件依赖/header/call guard；三个实际OTA对象未引用功能模块符号。

上传保留原镜像头/芯片/项目/大小/时间门禁、4KiB PSRAM流式块、完整esp_ota_end校验及boot partition提交。新组件自己的atomic单上传gate拒绝并发和重入。shutdown请求在读取循环检查；失败路径如已begin则abort，释放chunk、取消重启reservation并释放已取得的上传许可；end或boot提交失败不安排重启。成功提交镜像后，即使最终HTTP响应丢失仍commit1500ms reboot，与旧实现一致。OTA不再调用UI进度/Camera reservation/touch，状态仅供Web读取；无新任务、队列或画布。旧maint_ctl仍有PIN/menu显示，最终固定MAINTENANCE尚未Core绑定，不能把删除OTA进度绘制当整个固定画面验收。

启动健康检查迁为Core app_core_ota_health，ota_health纯决策头归Core private。保留60秒pending镜像确认、heap失败回滚、display fatal由现有health链处理、确认失败回滚。Core通过app_wifi公共status确认started/online及可用AP地址，避免依赖SDK netif；rollback调用Core Camera生命周期而非旧camera_pair，并在rollback API异常返回后执行esp_restart，内部RAM health task归属保持。此路径仍临时关闭旧维护控制器，最终全局模式/失败cleanup尚未替换。

147/147主机测试通过，原54注册/旧断言保留。新增14个实际maintenance_ota源码场景，独立目标只链接OTA源码、prefix校验和SDK/HTTP/JSON fakes，不链接正常应用；覆盖success、最终响应丢失、PSRAM分配/OTA begin/prefix写/body写/end/boot提交失败、shutdown打断、并发重入、restart/admission拒绝、坏头、初始关闭、状态及许可/内存归还。另9个真实Core健康检查场景覆盖时间边界、heap、AP离线/错误/地址缺失、确认失败、display、非pending、关闭失败；Core启动新增OTA初始化失败用例，原启动断言保持并延长顺序。JSON fake只记录字段，不证明真实HTTP/cJSON/flash/RTOS/SMP/硬件行为。

最初固件编译暴露公共OTA头包含HTTP类型却只声明private HTTP依赖，以及Core生命周期声明遗漏，已补public SDK依赖与app_core.h。新lost_reply fixture最初误使所有HTTP回复失败（包括未初始化拒绝），修为只丢最终reboot ACK，原生产语义与核心断言未放宽。最终host/build日志build/module-maintenance-ota-{host-build,host,default,stable,release}.log；早期默认失败日志另存module-maintenance-ota-default-initial.log。

当前源码LCD Default0x35c320、Stable0x35b500、Release0x350330全部构建成功，均<5MiB/6MiB分区。三compile graph中OTA/header/Corehealth/compat各单一owner、无main旧OTA；ELF新OTA与Corehealth/SoftAP hook实际可达，旧OTA/维护UART符号无，Release无SIM/bench/fault/encoder。主机实际独立link及SDK OTA对象未引入Core/Camera/Input/UI/Console功能引用。ATOM共享源码未改，本批未重建；上一批ATOM证据保留。

所有本批构建会话终态，无本批后台串口/构建，其他聊天未检查。没有烧录、实机、提交或推送；OTA真实flash/失败回滚/关闭竞争、全局维护与长期稳定仍待验证。完整目标active，下一步迁移无认证Web与配置持久化/工厂、启动:80 trigger和Core不可逆切换，消除旧控制器/临时adapter/WHOLE_ARCHIVE，完成全项审计。

最终边界/文档/diff门禁通过：123文档、529本地链接、0问题。
