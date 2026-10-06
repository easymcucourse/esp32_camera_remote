# 2026-10-06 输入状态机与 report 仲裁内核

现有 gamepad_input.c 和头迁入 components/app_input（头为 private），迁移时比较源文件 SHA256 一致；原 gamepad_input 回归断言不改。main 的 ATOM 路由暂显式包含 private 头并依赖 app_input，这是尚未接入 provider 的过渡边界，不能称完成输入消息化。生产仍使用同一状态机、同一3072栈/prio4 ATOM任务，没有新任务或业务动作映射变更。

新增 input_reports.c 为 owner-only 纯 C 内核：显式唯一来源选择、各来源 epoch/id 游标、重复/旧 epoch 拒绝、ID 回退安全释放并隔离该 epoch（新 epoch 才恢复）、断开/gap/切换先释放。首次报告仅建立 baseline，不合成缓存 press；持有的扳机/肩键需释放后重新按下。RELEASE_ALL 和 MF_CANCEL 投递失败各自保留 pending，重试成功前阻止普通动作/新来源 press。序号为正值，每 provider lifetime 内不回绕；耗尽须更换 epoch。sink true 仅代表接受安全动作所有权，不代表相机已完成执行；最终消息 service 的执行确认与 provider 句柄/注册、队列及生命周期尚未实现。

独立 test_input_reports 覆盖两来源切换与迟到报告、重复不重复动作、回退隔离、epoch恢复、gap释放/重按、断开以及 release/MF handoff拒绝。与原 gamepad_input 使用同一纯状态机，不链接 Sony/Wi-Fi/UI/驱动。边界检查拒绝 app_input 依赖业务实现或 driver。

主机86/86通过，原54保持。三构建 Default/Stable/Release {"default": "0x358cc0", "stable": "0x357ea0", "release": "0x34c280"}，全部<5MiB；ELF确认生产 gamepad state machine 链接；新 report kernel 尚无生产调用，会由链接器裁剪，不把仅归档编译当作provider集成。Release禁止符号与旧fdAPI无。日志 build/module-input-reports-{host-build,host,default,stable,release,symbols}.log。

未提交/推送/烧录；无新硬件结果。下一步实现 input_service/public provider handles 与统一 report 队列，ATOM/模拟仅提供报告，capabilities/UI state/动作状态走 typed Console message，删除main对gamepad private头及直接Camera/UI/维护旁路。随后独占维护、UART、完整Core组装和全清单验收。完整计划仍 active，S2.4仅部分。
