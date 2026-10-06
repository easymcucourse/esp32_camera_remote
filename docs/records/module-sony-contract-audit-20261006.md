# 2026-10-06 Sony/PTP 当前契约与文档核对

sony-ptpip-design当前节更正Web-only配置/无u/POWER_ZOOM声明、255测试及当前版未烧录；新增真实对象/资源图，明确Sony只一个embedded ptpip_client_t，Camera sole protocol顺序与Wi-Fi lane真实socket所有权不同。旧第1–14节fd/transport/session/独立Sony/UI草案保留历史协议参考及原anchors，但明确被最终拆分计划替代，不再暗示它们是迁移待实施路径。

源码证据：camera_backend_sony.c私有sony_backend_t含generic interface/单ptp/properties/capabilities；ptpip_client.h集中两channel tokens、session/next_transaction/deadline/parents/cancel/network状态。camera_session.c保留generic backend/factory/状态，camera_backend_binding.c是Camera唯一具体factory组合点；app_camera CMake PRIVATE backend/Sony，Sony PRIVATE PTP，PTP不依赖Wi-Fi实现/lwIP。搜索生产三目录无ptp_session_t/ptpip_transport_t/裸socket/lwIP，socket字样仅资源说明，不当实现引用。

test_sony_backend真实Sony+PTP+Console契约fixtures断言vendor/standard连续transaction及同owner CloseSession、network/cancel拒绝、disconnect失败保留token且destroy拒绝；producer/debug用fake generic backend不链接PTP/Sony。重跑PTP client/protocol/Sony/session/discovery/producer及Debug/partial相关9/9通过，日志build/module-sony-contract-audit-host.log。补checklist S4.9/S4.11/A20/22/23/25/27源/主机证据，A24保留第二真实实现未验范围。

纯文档审计未修改生产实现，无额外构建、硬件、remote CI、提交或推送，无livehandles；最新255全host与三LCD见前批记录。对象/依赖代码证明不代表真实相机/网络、SMP/缓存禁用及30min稳定性。完整目标active，继续全S/V/A38、remaining development docs、public/ops contracts/owner错误退出和最终五配置。

最终boundary/doclinks147文档581链接0issues及diff通过。
