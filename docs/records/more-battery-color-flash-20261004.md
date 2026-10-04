# 2026-10-04 MORE 与电量颜色版烧录

用户要求烧录最新版本：设置入口及子菜单标题改为 MORE，删除独立 DS4 CONNECTED / DISCONNECTED 行，保留手柄电量；相机和手柄电量 >50% 绿色、21–50% 黄色、≤20% 红色，未知灰色。

烧录前 COM8 确认运行 `ota_0 valid`，录像停止无 pending。核对镜像大小 `0x3514e0` 且生成时间晚于最终源码后，以 esptool 460800 波特率仅写应用地址 `0x20000`。写入 3,478,752 字节，哈希校验通过并复位；未写 NVS、otadata、分区表或 bootloader。

重启后查询：`ota_0 valid`、`display_failed=0`、`SIM=0`、ATOM / DS4 在线、真实手柄电量 `battery=8`、两扳机为 0。相机尚未重新连接，`session=0`、热点客户端 0，因此本次未验证新 MORE、十八行布局及颜色的实机画面。此前电量版视觉验收不能替代本版验收。未提交或推送。

本地证据：`build/more-battery-color-build.log`、`build/more-battery-color-preflash-status.log`、`build/more-battery-color-flash.log`、`build/more-battery-color-postflash-status.log`。
