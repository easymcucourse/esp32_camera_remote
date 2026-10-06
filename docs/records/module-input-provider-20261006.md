# 2026-10-06 统一输入 provider API / report owner

本批新增 app_input/include/input_provider.h，版本1，仅导出来源枚举、不可解引用的32位注册句柄、值类型report、register/publish/disconnect/unregister。没有暴露gamepad状态、RTOS队列、设备句柄或业务callback。输入轴保留原int8有效范围，电量保留原0..10/255，不改变原协议的电量单位。report包含独立source_epoch/report_id及cached event validity/buttons；不把ATOM缓存事件号误作每次周期快照号。

input_provider.c 使用内部静态16项复制环队列与两个provider槽，短临界区保护API，非阻塞且不分配、不创建任务。每种来源只注册一次；句柄单调分配不回绕，注销后的旧句柄不再投递；待断开通知未消费时不能复用来源注册。报告复制，不引用provider栈。队列满时独立保留安全断开通知，并用内部64位serial失效该来源所有旧排队报告，其他来源报告仍可消费。显式disconnect/注销/close同样让断开先于旧报告；close关闭报告与注册准入。内部序号耗尽时关闭准入并给所有注册来源STOP通知，不回绕。

新增private input_owner消费registry，默认ATOM、显式唯一来源选择；报告转同一input_reports/gamepad状态机。新注册句柄重置该来源epoch游标并释放/建立baseline；来源切换释放失败阻止新press，旧来源晚report不改变当前状态。断开/overflow清online/battery；quiesce关闭registry准入并持续重试release。公共report和private kernel frame类型分离，避免同名类型冲突。

新增test_input_provider覆盖初始化前/重复注册、非法范围、复制存储、满队列仍优先断开、只失效受影响来源、显式断开、注销/重注册/旧句柄、close与drain。test_input_owner链接真实registry/owner/report/gamepad，覆盖两来源、释放投递失败、旧来源拒绝、断开、新句柄允许epoch从1重启、overflow不执行旧press和quiesce拒新报告。原gamepad回归断言不改；host88/88通过，原54保留。host critical宏为fake，非SMP/RTOS调度或硬件稳定性证明。

三构建 Default/Stable/Release {"default": "0x358cc0", "stable": "0x357ea0", "release": "0x34c280"}，均<5MiB；三个app_input归档含所有public provider和private owner符号。生产ATOM尚未调用这些新API，因此image会裁剪新registry/owner；仅原gamepad component已生产使用。不能把归档构建和host集成冒充生产provider/service完成。Release禁止符号及旧fd API无。日志 build/module-input-provider-{host-build,host,default,stable,release,symbols}.log。

仍缺Core-owned input_service公共生命周期/消息consumer、真实ATOM与UART provider接入、capabilities/UI状态/动作全部typed消息及main旧旁路删除。registry当前一次启动后close，完整停止/失败回滚重启须随最终service生命周期实现。原ATOM任务3072/prio4未变，没有新增task；维护/完整Core组装/UART目标仍未完成。未提交、推送或烧录。

下一步优先实现真实消息service和UI菜单/偏好消息消费，将ATOM业务处理拆出provider，再删除main对gamepad private头、Camera/UI/Wi-Fi/维护直接调用。完整范围见[验收清单](../development/module-split-checklist.md)。
