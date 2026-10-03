# 2026-10-03 Matrix 调试回归

## 实现

开发构建新增 `led test` / `led off` / `led fault bt|i2c|overflow on|off`。四角按左上红、右上绿、右下蓝、左下白，每秒一步；再次 test 退出校准。独立 forced 掩码叠加到渲染快照，真实故障与定时保持；校准最高显示优先级，off 清除所有覆盖。设置仅 RAM，status 显示 calibration / forced / SIM，生产关闭模拟不注册命令且不编译 matrix_debug_frame。

渲染任务仍独占 RMT，UART 仅更新短锁保护的设置。覆盖不改实际连接 / I²C 协议故障报告，BLE 手柄和云台电量来源仍待实现。

## 验证

39 / 39 CTest 通过，新增覆盖撤销、真实故障保留、故障优先级、四角顺序及时间回绕回归。ATOM 开发和关闭模拟构建通过；关闭模拟 ELF 无 matrix_debug_frame 符号。

仅 COM6 / 115200 烧录，LCD COM8 保留原固件，未擦除 NVS。`atom-led.uart` 18 条命令通过：校准开关、三种掩码组合与逐项撤销、真实 faults=0 保持、非法参数拒绝、off 完全清除覆盖。随后原始请求 12 条与真实 I²C 手柄模拟 18 条回归通过。完成后 calibration=0、forced=0、SIM=0、SETTINGS=0。

证据：`build/led-host-test.log`、`build/led-atom-build.log`、`build/led-release-build.log`、`m5_atom_matrix/build/flash-led-debug.log`、`build/atom-led-test.log`、`build/led-req-test.log`、`build/led-pair-pad-test.log`。

## 边界

串口回放确认命令及状态，主机测试确认逻辑图案；未观察物理灯阵，不能据此确认安装方向、颜色或故障视觉。RMT 发送 / 恢复故障注入、活动相机负载及 30 分钟稳定性仍待验收。
