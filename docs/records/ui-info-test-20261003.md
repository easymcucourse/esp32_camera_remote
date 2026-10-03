# 2026-10-03 三档信息与持久化回归

## 实现

LIVE 支持 full / compact / hidden，触摸板单次事件循环，按住 / 快照不重复，gap 不切换，X/Y 映射保持。按键选择在异步偏好询问后未收到回复，按事先说明使用推荐的触摸板映射，尚非用户确认；Xbox 对应键待 BLE 适配。串口 `ui info [full|compact|hidden|next]` 可查询 / 修改。

精简保留录像计时、相机低电量（≤20%）和命令拒绝 / 超时，隐藏保留红色录像点；SIM 在所有档位保留，SETTINGS 不隐藏菜单。对焦框尚未实现，不能以档位完成替代该目标。档位在下一帧成功解码时应用。

ui_preferences 的独立优先级 2 / 3072 栈任务保存 NVS `ui_prefs/info`，成功后发布给 board，失败保留旧值；四项请求队列、八项总预留约束结果队列不会溢出。全部重置新增 UI 偏好阶段，与持久化 mutex 串行并拒绝新请求直至成功重启；失败不重启，只尝试 Wi-Fi 回滚，相机 / UI 存储可能已部分改变。

所有 UART 异步模块统一使用 debug_async_token，避免 UI 保存与手柄动作等模块复用同一 token 使回放工具提前完成等待。

## 验证

41 / 41 CTest 通过：新增 overlay 策略（所有档位录像点保持、隐藏录像文字、低电量 / 未知值、无效档位回退），扩展输入（单次 touch、按住 / gap 不切换、X/Y 无信息动作）与全部重置（UI 失败释放占用 / Wi-Fi 回滚 / 不重启）回归。

LCD 与 ATOM 开发 / 关闭模拟构建均成功。COM8 / 460800 与 COM6 / 115200 烧录校验成功，LCD 最终包含 help 命令说明，未擦除 NVS；发布构建未烧录。

- ui-info.uart：26 条命令通过，本地输入循环、保持、gap、X/Y、设置页保持，以及退出模拟后 full。
- ui-info-persistence.uart：5 条命令通过，保存 hidden，持续显示故障三次失败后 camera_drained=1、NVS preserved、重启 READY，再读 hidden，最终恢复 full。
- pair-ui-info.uart：最终固件 11 条命令通过，经真实 I²C touch 到 compact，overflow / gap 后保持，再恢复 full、退出模拟。
- 最终 console-smoke 6 条与 ATOM 原始请求 12 条通过。两端模拟关闭，设置页关闭，信息档位为 full。

异步等待实测：LCD 本地 tap touch 使用 token=1，UI 保存先于动作完成使用 token=2；工具等到 token=1 的 pad DONE / elapsed=100ms 后才查询状态，没有被提前的 UI DONE 结束等待。

证据：`build/info-host-final-test.log`、`info-lcd-final-build.log`、`info-help-build.log`、`info-atom-build.log`、`info-lcd-release-final.log`、`info-atom-release-build.log`、`flash-info-final.log`、`m5_atom_matrix/build/flash-async-token.log`、`build/ui-info-test.log`、`ui-info-persistence-test.log`、`pair-ui-info-test.log`、`info-console-final.log`、`info-atom-req-test.log`。

## 边界

相机离线，未观察实际取景 overlay、录像红点或物理触摸板；输入来自串口模拟，真实 I²C 传输有实测。NVS 正常持久化及跨重启已测，NVS 错误注入 / 偏好损坏恢复尚未实机验收。全部重置新增阶段仅主机回归，未清除相机身份做实机测试。实际相机控制、MF 框、BLE Xbox、维护 OTA、CI / 长期稳定性等全需求继续推进。
