# 2026-10-03 维护模式手柄入口

连接页无相机会话且非 SETTINGS 时，Select / Share 按下边沿开始两秒计时，触发一次维护开 / 关；按住不重复。相机会话、切页、gap、断连或释放取消计时；重连时已按住的键和无边沿快照不能开启维护。

设置菜单第九项为 MAINTENANCE，第一次 A 确认显示停止提示，三秒内第二次 A 才取得相机 lease、排空并暂停自动连接，再开启网页。左右重复不会确认。导航、B、切页、gap / 断连取消确认及旧代数排队请求；维护已开启时菜单一次 A 关闭。UI 保留全部原行，参数面板调整为 17 行 / 30 像素。请求队列满会触发输入安全取消，不落入其他菜单分发。

## 构建和主机测试

46 / 46 CTest 通过：`build/maint-entry-host-test.log`。新增纯确认测试验证三秒边界、代数取消与回绕；扩展实际 gamepad_input 测试验证长按截止、短按、单次触发、释放 / gap / 切页 / 相机会话取消、回绕和已按住的首次快照。LCD 开发 / 关闭模拟最终构建见 `build/maint-entry-dispatch-build.log`、`build/maint-entry-dispatch-release.log`。COM8 最终烧录记录 `build/flash-maint-entry-final.log`，未擦 NVS；ATOM 未重新烧录。

## 设备证据和边界

本地协议模拟首个通过版本：`build/maint-gamepad-settled-test.log`，41 条命令。验证短按 / 长按开关、八次导航到维护项、A 双确认、右键长按不确认、B / 超时 / gap / 切页取消、停止占用成功、回连接页、HTTP 可用与退出后相机自动连接恢复；两轮各 14 个设备 TCP / HTTP 请求通过。真实 I²C 初次通过 `build/pair-maint-gamepad-final.log`，8 条命令，ATOM 模拟手柄长按经真实 I²C 使 LCD 开启维护，14 个 HTTP 请求及退出恢复通过。

最终分发修正版本的本地回归 `build/maint-gamepad-complete.log`，44 条命令通过，另补充菜单一次 A 关闭维护并保持 SETTINGS 页，随后仍可重新开始双确认。前一轮扩展脚本假定关闭会强制回连接页，多按一次 Start 后在非设置页期待确认而超时；按实际页面保留规则修正。该轮 `maint-gamepad-release-check.log` 名称虽含 release，实际仍为开发固件，不作为生产烧录证据。

最终固件真实 I²C 再测 `build/pair-maint-gamepad-complete.log`，8 条命令与 14 个 HTTP 请求通过。最终维护关闭、SIM0 / SETTINGS0、full，自动连接开启并等待已配对相机。本地脚本通过 `uart_script.py --reset lcd` 执行，保证初始菜单光标为零；双端脚本不会复位 ATOM。

首轮 gap 脚本期待缺口所在事件触发确认而超时；第二轮 gap 恰好落在上一条动作尚未传递的释放事件，后续 A 已是有效新边沿。加入释放传递等待后，明确验证缺口事件不触发、下一边沿开始新的第一次确认。真实 I²C 首轮误把 `@lcd expect` 写成串口命令，修正为 `expect-any` 后通过。这些是脚本时序 / 语法修正，没有相应固件崩溃。

UART 回放证明固件输入和控制路径，不代表用户实际 DS4 按键、物理屏幕视觉或真实在线相机效果。相机会话始终为 0，STOP 测试排空等待任务而非真实取景；未验证实际 held S1 / S2 / zoom 释放、在线恢复时延、满队列整机注入、六十秒锁定 / 十分钟超时。自动关闭提示三秒消失、OTA / 网页重启、BLE Xbox、电量真实来源及全系统稳定性仍待完成。
