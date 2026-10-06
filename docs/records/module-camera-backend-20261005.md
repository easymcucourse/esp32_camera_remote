# 2026-10-05 通用 Camera backend 契约

本批完成阶段4.1的接口定义与绑定校验。接口尚未接入生产Camera，Sony backend factory/ops和完整Camera迁移仍未实现；完整目标继续进行。

- camera_backend独立component只依赖C标准类型；不依赖PTP/Sony/SDK/Console/Wi-Fi/UI。定义统一结果、语义setting/action、带类型的值、借用属性/choice、通用能力、peer、JPEG view和生命周期ops。
- 接口包含版本、能力、不透明context和static const ops；绑定只接受fresh zeroed storage，拒绝版本/必需能力不符、未知能力、缺少必需/广告ops与重复绑定。失败不修改已有对象，不创建第二层纯转发wrapper，不保存协议会话/事务/取消状态。
- 明确单owner同步调用、cancel/network_changed跨task无阻塞、整个操作正值超时、cleanup失败保留所有权、destroy成功失效完整handle、属性全量校验后回调、choice只在callback有效、frame借用caller scratch、返回前不撤回buffer，以及协议接受不等于物理完成。实现层需要履行这些契约，当前测试不证明完整Sony实现。
- 边界脚本对camera_backend仅允许自身头及stdbool/stddef/stdint，防止厂商/业务依赖。

主机70/70通过，原54测试保留。新fake backend独立链接camera_backend，无PTP/Sony/app_wifi：验证错误版本/能力、每个必需ops缺失、无副作用失败、可选能力实现、重复绑定保留原对象、单context生命周期与cleanup状态。证据build/host/Testing/Temporary/LastTest.log。

Default 0x3559d0 / Stable 0x354bb0 / Release 0x349040编译通过，日志含新component真实camera_backend.c编译，均小于5MiB；Release禁用符号检查通过。首次直接CMake重新配置因shell缺少编译器PATH失败，补足现有工具链PATH后配置及构建成功，未安装工具。build/module-camera-backend-{config-default,config-stable,config-release,default,stable,release,symbols}.log。边界/doclinks/diff通过；无烧录/提交/推送。

下一步实现camera_backend_sony组合实例及同fake Console契约测试，再迁移生产Camera，删除legacy fd/外部计数器；继续完整计划的输入/UI/维护/UART/组合根和兼容清理。见[逐项清单](../development/module-split-checklist.md)。
