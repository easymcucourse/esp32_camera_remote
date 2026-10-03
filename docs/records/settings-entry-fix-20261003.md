# 设置模式进入失败修复

2026-10-03：用户反馈“无法进入，或进入后自动退出”。COM8 实机进入设置模式后复现 `JPEG allocate memory failed`、`Scaled JPEG setup failed`、`LIVEVIEW display failed: ESP_ERR_NO_MEM`，取景 worker 排空并重连。此时 SETTINGS 标志可以为 1，但设置页无法成功发布，不能只检查该标志判定切页成功。

首次判断全屏解码器未释放不准确：原有代码已经有释放。首次冗余调整烧录后，21 次切页检查仍失败，原始失败保留在本机 `build/settings-fix-switch-test.log`。最终取消 JPEG 库内的缩放实例，避免它在完整取景流水线开启时申请额外的大块内存。

LIVE / SETTINGS 共用全尺寸 SIMD RGB565 解码器。设置模式将 1024×576 图像直接解码到现有帧缓冲顶部，再使用 `image_shrink` 原地最近邻缩小至 768×432，保留 LCD 的 1024 像素行距；从上到下、从左到右写入，防止覆盖尚未采样的源像素。右侧和底部清零后绘制参数。没有新增图像缓冲。缩略图采样方式改变，物理视觉需要观察。

`test_image_stride` 增加所有小尺寸 / 缩放比例的独立原图采样对照、填充行、首尾保护及非法参数；54/54 CTest 通过，LCD debug / release 构建通过。应用仅写入已确认运行的 ota_0（0x20000），不修改 NVS、分区表或 otadata。最终实机结果补充在下方。

本机证据：`settings-entry-probe.log`、`settings-fix-switch-test.log`、`settings-shrink-host-test.log`、`settings-shrink-debug-build.log`、`settings-shrink-release-build.log`、`settings-shrink-flash.log`，均在忽略目录 `build/`，不提交原始设备日志。

## 最终烧录与本地渲染验证

最终应用 COM8 / 460800 写入 ota_0，哈希校验通过。在线测试等待相机六十秒未连接，未进行切页，不能记作在线验证成功；测试 finally 恢复 ATOM SIM0，真实 DS4 输入在线。该等待结果见 `build/settings-final-lcd.log` / `settings-final-atom.log`。

本地 JPEG 回归 `settings-local-render.uart` 九条命令通过：全屏→设置页→全屏，每次连续显示20帧，各次 `error=ESP_OK`，设置页没有内存错误。证据 `build/settings-local-render.log`。该基准暂停相机、使用合成 JPEG，不代表在线取景负载和物理视觉验收；当前相机离线，已请用户重新连接热点，在线切页复测待完成。终态 SETTINGS0 / SIM0、ota_0 valid；没有新提交或推送。

## 后续在线复测（同日）

用户提出手柄选择与电动变焦需求时，相机已经在线。最终缩放修复版本连续切页21次全部通过：session=1、切页状态正确、fps>0、display_failed=0；没有 ESP_ERR_NO_MEM、JPEG worker drained 或取景显示失败。终态SIM0，真实DS4输入恢复，设置页维持开启。在线窗口为短测，未代替30分钟稳定性或屏幕视觉验收。证据 build/settings-online-check.log；原始 settings-final-* 日志被此次复测更新。其后新功能版本已烧录，当前状态以新记录为准。
