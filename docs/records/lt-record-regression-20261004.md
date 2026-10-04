# 2026-10-04 LT 录像失败复查

用户报告 LT 出现对焦且不录像。当前源码 S1 已仅由 RT 控制，实体采集的 LT255/RT0 两次操作只发送录像属性 `0xD2C8`，收到 `0x2001`，没有 `0xD2C1` / `0xD2C2` 对焦或快门命令；照片 P 模式下录像状态仍停止。命令接受不能作为录像成功证据。

同一 ESP32 远程会话中，用户确认机身 MOVIE 按钮也不能录像。串口 `s` 停止相机任务、关闭套接字后，用户确认同一照片 P 模式下机身按钮可以录像。随后串口 `j` 恢复会话，查询 session1、SIM0、display_failed0；用户进一步确认“现在lcd可以录像了”。这说明恢复会话后功能恢复，但不能证明唯一根因是未释放对焦，亦不能据本次失败认定照片 P 不支持录像。

用户要求完整去掉 LT 对焦。进一步移除输入结构的 LT 半压阶段，仅保留独立的录像全压布尔状态；RT 保留半压 S1、全压 S2。LT 全压阈值与原实现一致：230 进入，低于 204 离开。LT 按压、保持及松开均不请求 S1，亦不释放仍由 RT 按住的 S1。

验证：重新构建主机测试后，`gamepad_input` / `camera_actions` / `sony_write` 3/3 通过，包含 LT 全压阈值抖动及 LT 保持全压时 RT 松开的释放回归。LCD 默认固件构建通过，应用大小 `0x3515d0`；`git diff --check` 通过。未烧录此修改，设备仍运行之前的版本。

本机证据：`build/lt-record-regression-physical.log`、`build/lt-record-regression-body-movie.json`、`build/lt-record-regression-stop.log`、`build/lt-record-regression-resume.log`、`build/lt-record-regression-restored-status.log`。原始日志仅本机保存；用户反馈与日志观察分别记录。

## 新版烧录

用户随后要求烧录。烧录前确认 COM8 运行 `ota_0 valid`、录像停止且无 pending；重新构建确认应用大小 `0x3515d0`。仅写入应用地址 `0x20000`，保留 NVS / otadata / 分区表 / 引导程序；esptool 写入 3,478,992 字节，hash 校验通过并复位。

复位后查询 `ota_0 valid`、`SIM0`、`display_failed0`，ATOM / DS4 在线，手柄电量档位 8。相机尚未重连（session0、Waiting for paired camera、热点客户端0），本次未重新验证录像启停和持续取景。证据 `build/lt-cleanup-preflash.log`、`build/lt-cleanup-flash.log`、`build/lt-cleanup-postflash.log`。
