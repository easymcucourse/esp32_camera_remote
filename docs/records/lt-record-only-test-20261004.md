# 2026-10-04 LT 仅录像烧录与测试

用户要求删除 LT 半压对焦并同步代码、文档，随后要求烧录测试。当前映射为 RT 半压 S1、全压 S2；LT 半压无动作，全压按实际录像状态请求开始或停止。

## 烧录

烧录前 COM8 状态确认 `ota running=ota_0 state=valid`。使用 ESP-IDF Python 环境中的 esptool，以 460800 波特率仅将 `build/esp32_camera_remote.bin` 写入 `0x20000`。应用大小为 3,478,256 字节（`0x3512f0`），哈希校验通过，随后复位。未写 NVS、otadata、分区表或 bootloader。烧录后再次确认 `ota_0 valid`。

## 串口模拟输入与真实相机通信

LCD 开启 `atom sim on`，模拟手柄经现有输入处理、动作队列与真实 Sony PTP/IP 通信执行。本次脚本 12 项检查全部通过：

- 相机会话在线、录像初始停止；模拟输入已连接、扳机已松开。
- LT 半压 153 到达 LCD，半压及松开期间没有 S1 或录像命令。
- RT 半压发送 S1 按下；LT 同时半压时，RT 松开立即发送 S1 释放，LT 松开无额外 S1。
- LT 全压只发送 `0xD2C8` 启动，没有 S1；相机录像状态最终确认 `known=1 recording=1 pending=0`。
- 再次 LT 全压发送停止，没有 S1；相机确认 `known=1 recording=0 pending=0`。
- 显示 `display_failed=0`；测试结束恢复 `SIM=0` 实体输入，两扳机为 0。

本次初始曝光模式回读 `0x00058015`（当前显示映射 MACRO）；录像中回读 `0x00078054`（MOVIE AUTO），停止后恢复 `0x00058015`。测试脚本未发送曝光模式写入。该结果证实本次 MACRO 配置下 LT 录像启停成功；不能推广为照片 M 或所有模式，也不能仅靠前后对比确认半按耦合就是历史失败的唯一原因。

## 验证边界

恢复实体 DS4 后，用户回复“已完成，能录像，取景正常”。该反馈确认用户观察到实体手柄录像及持续取景。55 秒实体采集窗口内记录到 RT 全压 254 / 243、半压 112，以及 S1、S2 按下和释放均接受；该窗口没有捕获 LT 录像命令，故不能声称实体 LT 启停也有完整逐条日志回读证据。取景帧数从 373 增长到 586；拍照动作附近采样 FPS 降至约 2.16–2.33，其他采样约 4.28–4.86，未做性能调优。

RT 验证的是命令与相机接受，不是实际合焦或照片文件生成。串口模拟输入与实体操作证据分别记录。最终另一次状态查询确认会话在线、`ota_0 valid`、`SIM=0`、两扳机为 0、录像停止无 pending、`display_failed=0`；后续采样中用户继续切换模式，本测试未强制恢复初始模式。未进行 30 分钟稳定性、照片 M 专项或所有模式测试。测试产生的视频文件未删除。

## 证据

- `build/lt-record-only-build.log`：固件构建成功；相关主机测试重新构建后 3/3 通过。
- `build/lt-record-preflash-status.log`、`build/lt-record-postflash-status.log`：烧录前后 OTA 状态。
- `build/lt-record-only-flash.log`：应用写入与哈希校验。
- `build/lt_record_hardware_test.py`、`build/lt-record-hardware-test.log/.json`：模拟扳机、真实相机通信与 12 项结果。
- `build/lt-record-physical-ds4.log`：实体输入采集窗口。
- `build/lt-record-final-status.log`：实体反馈后的状态快照；用户回复为实机视觉证据。
