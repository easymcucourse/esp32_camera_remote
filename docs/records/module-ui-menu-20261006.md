# 2026-10-06 UI 菜单消息与语义参数

UI_MENU_ACTION 的生产处理器已接入 UI endpoint。设置页切换、主菜单循环、MORE 打开/关闭/九行/EXIT 导航由 app_ui/ui_menu_messages.c 完成；UI_STATUS 与菜单回复共用私有 snapshot。菜单 STEP 回复 semantic Camera property，不向输入公开 Sony 属性号，也不在 UI 直接执行 Camera 动作。

main/atom_link 的输入回调实际发送 Input→UI REQUEST，读取菜单 route/property/state 后发 Input→Camera SETTING_ADJUST；Camera safety generation 保留在原 action.generation 和 command.token 中。删除原直接 UI toggle/menu/extra mutations 与自行 selected 行号换算。Wi-Fi 页仍通过旧 wifi_menu_ui adapter，正常维护入口仍旧兼容策略；这些不是最终模块边界。生产轮询 caps/settings、偏好 pad 缓存和 Camera action 尚待完整 input service 消息化。

消息验证包括 type、target、来源 Input/UART、REQUEST-only、无 lease、非零 lifecycle、允许的 action、MOVE/STEP ±1、绝对 deadline；修改前比对源 endpoint generation 与目标 delivery epoch，拒绝停止/重启前的旧命令。UI 消费端只返回值；输入请求失败继续反馈原 gamepad safety cancellation。菜单 REQUEST 使用 500ms 上限，实机 UI/输入时序尚未验证。现有 UI endpoint 与任务/栈政策保持，没有新增任务、像素缓冲或显示 writer。

新增 host test_ui_menu_messages 使用真实 UI model + menu handler：隐藏菜单不移动；原主菜单顺序 {0,1,2,3,4,6,9,7,8,5}；七行 semantic 映射；MORE 九行/EXIT/返回/切页关闭；Wi-Fi active route；非法方向/来源/flags/lease/action/零 generation、过期、源 generation/目标 epoch stale 均不修改模型。原 54 项保留，总 91/91 通过。fake critical/timer 非 SMP/RTOS/硬件稳定性证明。

构建 Default 0x3592b0、Stable 0x358490、LCD Release 0x34c850，均 <5MiB；ATOM 双模 Debug 0x1042a0、Release 0x101690 通过，BLE/BTDM/GATTC 配置与 Release 禁止符号门禁通过。三个 LCD ELF 都实际链接 ui_menu_message_apply/ui_menu_snapshot。日志 build/module-ui-menu-{host-build,host,default,stable,release,atom-debug,atom-release,symbols}.log。boundary/doclinks/diff 通过。没有提交、推送、烧录或实机验证。

完整计划仍 active：Wi-Fi 菜单归 UI/typed 配置与工厂恢复，Input service/provider 生产接入/生命周期/全部动作和状态消息，UART gateway，启动 :80 无认证独占维护，Core 组合根/停止排空和全清单最终验收均未完成。见[全清单](../development/module-split-checklist.md)。
