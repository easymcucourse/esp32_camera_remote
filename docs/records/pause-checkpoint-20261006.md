# 2026-10-06 阶段整理与暂停交接

用户要求“今天暂停，整理提交推送”。本次保存当前模块拆分、通信修复、测试和文档工作区；项目尚未通过完整实机验收。

## 保存范围

- `main` 收口为 Core 启动入口；Camera、UI、Input、Wi-Fi、维护、Console/消息路由独立 component，底层显示、网络及 Sony/PTP 接口分层。依赖图、资源所有权、消息契约与验收清单同步维护。
- 正常 owners 的启动屏障、停止排空及维护切换已实现；配置写入与 OTA 归维护 Web。历史实现仅为仍需回归的测试保留，旧 UART 维护脚本移入 legacy。
- Camera 启动分配两槽 512KiB 图片包，UI 工作缓冲开机分配；Router 队列载荷使用 PSRAM。LCD bounce 改为 10 行，解决已复现的启动内部内存不足。
- 修复 Router 请求／回复锁竞争、Input 新报告时间戳被误判过期，以及 UI 菜单等待当前 JPEG 时截止过早；补充失败阶段日志和主机回归。

## 验证边界

- 提交前本机 Host 263/263 与模块边界检查通过；172 篇文档的 630 个本地链接无错误，最终暂存内容 diff 检查通过。
- 本批最终 LCD Default/Stable/Release 构建通过，Release 禁模拟符号检查通过；仅 Default 烧录实测。ATOM 使用双模 Debug 构建烧录，旧本地 BR/EDR-only 配置曾构建失败。
- LCD/ATOM 仅烧录应用，保留 NVS、otadata、bootloader 与分区表。在线真实 I²C 模拟菜单 10 次往返、在线 I²C 故障恢复和单次 LCD 恢复短测通过。
- 最后 30 分钟在线测试失败：约 123 秒窗口出现 5 次会话退出，取景响应 0x200F、backend=12，未获得有效帧率窗口；同窗两端 I²C 各 2411 次事务无错误。
- 用户反馈 LCD 显示不正常，实际视觉异常尚未定位；display_failed0 不能作为视觉通过证据。

暂停时串口脚本已退出，没有仍运行的本会话串口测试进程。本机日志、备份、配置和 agent memory 保持 Git 忽略；原始设备身份与配置不写入此交接。

## 恢复入口

先复查当前硬件、串口占用和 Git 状态；核查相机取景拒绝与 LCD 异常的关系，再评估 RGB 扫描、bounce 缓冲和渲染行为。修复后重做在线菜单、故障恢复、30 分钟连续取景及实体视觉验收。不要将阶段提交解释为硬件问题已解决。

详细证据见[通信修复记录](communication-recovery-test-20261006.md)，完整迁移待办见[模块拆分进度](../development/module-split-status.md)及[验收清单](../development/module-split-checklist.md)。
