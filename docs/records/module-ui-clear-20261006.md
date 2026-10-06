# 2026-10-06 UI 死页面清理与维护 model 清空

按计划 3.10/S3.6 清理当前 UI：删除 Wi-Fi 编辑 view、绘制分支和 setter；删除旧维护文字、页面请求、菜单状态及 JPEG 上的维护提示叠加。SETTINGS 共 17 行、9 个导航项；七个相机属性及 MORE 的 ID 保持不变，网络行只显示 `WI-FI (Web settings)`。不再提供 LCD 维护入口。

`app_ui_enter_maintenance()` 在正常 owners 已停止、renderer readers 排空后，永久冻结 model，再清空正常字符串、菜单、属性、录制、控制器及网络状态。未知值恢复原启动 sentinel；保留 display_failed 健康信号、同步锁及单调 generation，后者仅递增一次。取得显示锁后也清空 renderer 的旧连接文字。没有解除冻结的入口；后续正常 setter 不写入、不通知。固定 MAINTENANCE 画面的恢复/发布失败仍交由 Core 重启，禁止恢复正常画面。冻结不是正在执行 setter 的通用并发屏障，依赖 Core 先停止所有写入 owner 的生命周期约束。

原 54 项基线保持：旧菜单导航与旧 Wi-Fi editor model/header 归 `tests/support/legacy`，仅供历史回归目标编译。生产 model/menu/renderer fixture 验证新导航、正常数据清空、全部 setter 拒绝、generation 幂等、健康失败保留及固定画面后不重绘。fake canvas/pixel 检查不能代替真实字体、LCD/SMP 或 cache-off 验证。

HOST 231/231 通过。LCD Default `0x358550`、Stable `0x357740`、Release `0x34c560`，三构建终态 0。实际 compile graph 各有唯一生产 model/renderer；三 ELF 有终态清空函数，无退休维护/Wi-Fi view API 或 notice 绘制符号。新增边界门禁防止这些旧接口回到 app_ui。日志 `build/module-ui-clear-{host-build,host,default,stable,release,graph}.log`。ATOM 未涉及、未重建；无烧录、实机、提交或推送。

本批完成 UI 清空/冻结及死页面删除。完整计划仍进行中：A35 启动 trigger 提前、boot barrier/部分初始化、最终 S/V/A38 审计和全部配置构建尚待完成；硬件行为仍待验证。
