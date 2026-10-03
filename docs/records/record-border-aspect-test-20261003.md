# 2026-10-03 录像红框与 Aspect 子菜单实测

本次用户要求烧录并测试；测试设备为 LCD COM8、ATOM/DS4、Sony ZV-E10（相机回读固件 2.03）。保留此前开机标题、LIVE/SETTINGS 信息顺序和参数首尾循环修改，未提交或推送。

## 构建与烧录

先烧录已构建的红框/子菜单版；为确认实际结果，随后补充串口 `extra status`（九项实际值、可写、状态、目标）及 `status` 中的录像 known/recording/pending 诊断，再构建并烧录最终版。

两次烧录前均核对 `ota_0 valid`，只写 COM8 的应用地址 `0x20000`，哈希校验通过并自动复位。最终应用 3,478,240 字节（`0x3512e0`）；未写 NVS、otadata、分区表或 bootloader。增量构建保留的版本描述编译时间不能用来区分两次固件，写入校验和构建日志为证据。

## Aspect 子菜单和参数回读

串口模拟手柄经过实际 LCD 输入处理、相机通信及绘制流程，检查了主菜单上下完整循环、A 进入 ASPECT / MORE、子菜单首尾导航、EXIT+A 返回、B 返回及 Start 关闭子菜单并返回 LIVE。Wi-Fi 和维护菜单未误开启。

ASPECT `0xD211` 实际值从 2（16:9）改为 4（1:1），相机写入返回 `0x2001`，随后状态 APPLIED 与实际值 4 一致；再按左恢复值 2，同样经过实际回读确认。改为 1:1 时观察到取景 JPEG 从 1024×576 变为 1024×1024并采用缩小解码，恢复后回到 1024×576。

本次属性快照允许编辑 Aspect、Drive、Effect、AF Area、Wireless Flash；DRO、WB Temp、WB AB RAW、WB GM RAW 显示不可编辑。可写状态随相机当前模式改变，不能推广为所有模式的永久能力。未逐项改变其他八项参数。

## 录像、红框与连续取景

首次在照片 M（`0x00000001`）模式测试，模拟 LT 和实体 LT=255 都发出了 `0xD2C8` 启动请求，相机接受，但 `0xD21D` 回读仍为未录像，约十秒后待确认结束；该次录像测试失败。不能将命令接受报告为录像成功。用户曾报告红框显示但画面不更新，该条件未在对应采样窗口复现，不据此更改取景协议。

用户随后通过机身切换到视频模式；后续回读为 MOVIE P（`0x00078050`）。模拟 LT 启动和停止都由录像状态回读确认。在约30秒录像窗口，连续六次取景报告帧数为 2320、2353、2386、2415、2447、2480，FPS 为 6.03、6.48、6.42、5.64、6.35、6.48。用户同时确认“红框显示，取景画面持续更新”。

随后恢复真实 DS4，用户两次全压 LT。UART 捕获 LT=255，启动与停止写入均接受，录像回读依次为 1、0；用户确认“LT 已成功开始并停止录像”。这证明本次视频模式下真实 DS4 输入、相机录像启停、红框与持续取景通过，不证明照片模式远程录像或所有录制配置都支持。

## 收尾与验证边界

最终：`ota_0 valid`、相机会话在线、录像已停止且无 pending、LIVE、SIM=0、ATOM/DS4在线、LT/RT=0、display_failed=0、FPS约6.5。Aspect恢复16:9，视频模式保留为用户手动选择；未删除测试生成的视频文件。

相关主机测试此前4/4通过，涵盖红框边缘像素、主/子导航首尾及参数能力门禁。本轮增补诊断后 LCD 构建通过；设备测试未见所检查的 panic/assert/watchdog。菜单与 Aspect 检查为串口模拟输入，实际 LT 启停为实体手柄；两者不能混为全部操作均已人工逐项验收。未进行30分钟稳定性、所有扩展参数或所有信息档位的实机视觉测试。

## 本地证据

- `build/record-border-aspect-diagnostics-build.log`、`record-border-aspect-diagnostics-flash.log`：最终构建与烧录。
- `build/record-border-aspect-hardware-attempt1.log/.json`：40项检查通过，照片模式录像回读超时导致整套结果失败，已保留失败。
- `build/record-lt-real-test2.log`：照片模式下实体LT全压、请求接受但未录像。
- `build/record-video-test.log/.json`：视频模式录像启停与30秒连续取景，六项检查全部通过。
- `build/record-lt-video-test.log/.json`：实体LT录像启停回读。
- `build/record-aspect-final-status.log`：最终状态。

协议核对参考 [libgphoto2 Sony Movie 实现](https://github.com/gphoto/libgphoto2/blob/master/camlibs/ptp2/config.c)：0xD2C8 的开始/停止使用 u16 2/1，与当前实现一致。该参考只支持线格式；本次效果结论来自设备日志和用户确认。
