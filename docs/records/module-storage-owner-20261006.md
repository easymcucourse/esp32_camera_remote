# 2026-10-06 事务／显示／NVS 所有权核对

按A28读取Camera producer/session、Sony内嵌PTP、Wi-Fi lane/backend/storage/jobs、display_surface和Core维护停止/写入callback。资源表补齐每种owner和防重叠范围，区分namespace唯一primitive与实际执行任务/生命周期阶段，不将“common文件只有一份”当并发证明。

LCD直接NVS写入13处集中于wifi_saved_config.c、camera_identity_store.c、preferences_store.c。调用站点记录 `build/module-storage-owner-call-sites.json`。Wi-Fi saved read可能修复无效blob，因此既是reader也可能是writer，仍走storage mutex；正常Camera内部RAM worker保存GUID/peer，生产者等待完成；维护forget只能在正常owner全部停止后。ui_prefs普通只读，维护单HTTP task串行写入；更新头/源注释，删除不存在的正常偏好writer说明。三个HTTP clients不是三个handler任务。

新boundary门禁扫描LCD main/components/common C源，拒绝primitive之外nvs_set/erase/commit和任意全局NVS erase；ATOM是独立固件，不纳入LCD namespace规则。新增storage_owner_gate正反例，涵盖三个允许owner、UI旁路、comment排除、namespace erase旁路、允许文件也不能全局erase、common旁路。词法检查不证明macro/函数指针或SDK内部操作，阶段锁/线程行为仍依据实际source与对应fixtures。

host build终态0，完整 **262/262**（原54保留）；日志 `build/module-storage-owner-host-build.log`、`build/module-storage-owner-host.log`。本批只改工具/测试注册与注释/文档，无生产行为修改，沿用前批LCD D3586c0/S3578a0/R34c690与ATOM此前两配置，不重复固件构建。没有烧录、实机、远端CI、提交或推送。

A28由无证据更新源码/host范围的部分证明；真实SMP/Flash/cache-off、网络owner取消及时性及维护切换整体仍待硬件。factory跨namespace不原子，失败可能保留部分Camera/UI修改，未因新增门禁改变该行为。设备信息仍待用户确认，完整目标进行中。
