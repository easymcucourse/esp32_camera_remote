# 恢复实施：BLE 输入、电量与 OTA 健康检查

日期：2026-10-03。用户恢复项目，并确认手柄为八位堂 Ultimate 2；DS4 连接排查仍按此前决定暂缓，云台运动不在当前范围。本文区分代码、主机、构建、烧录和实机证据，后续增量结果补充在文末。

## 实施内容

- ATOM 改为 Classic / BLE 双模，独立 GATTC 扫描、认证、HID 服务验证和标准电量读取；不替换 Classic HID 回调。唯一候选才连接，候选溢出或多个手柄时等待。广播可省略 HID UUID，最终必须发现 HID 服务。
- Battery Level 仅发布 Matrix 第二行；断线清未知，DS4 第一行单独读取 Classic 状态，云台第三行仍等待真实来源。LCD 保留右上相机电量 / 对焦模式，不新增手柄电量。
- 缓存并通过 `ble map` 输出报告描述。Ultimate 2 输入门禁为实测的完整 113 字节描述和唯一通知报告特征；唯一 Input ID 1 为 33 字节，Output ID 5 不参与解析。未知描述保持电量功能并拒绝输入。按钮字母与扳机方向需实体确认。
- 共享输入发布在 SIM 之外优先 DS4，离线时 BLE 接管；切换清缓存、递增输入代数并同步持键基线。BLE 断线或一秒无有效报告发布离线。I²C v2 长度 / SIM 位保持兼容现有 LCD 来源释放机制。
- OTA 待确认应用满六十秒 AP 不可用、堆检查失败或确认 API 错误时进入回退处理；另一槽 INVALID 与 ABORTED 一并显示回退诊断，属于槽状态推断。实际回退失败路径会记录并重启，不能声称所有故障恢复已验证。
- 维护操作错误 / 自动关闭提示保留三秒，提示使用工作线程生命周期，LIVE 绘制关闭提示，避免首帧覆盖。
- PTP/IP 两个 socket 建立后启用 TCP_NODELAY；选项设置失败关闭 socket。新增独立 stable 配置把 Flash / PSRAM 改为 80MHz，默认配置保留。

## 主机与构建

最终主机测试 `ctest --test-dir build/host --output-on-failure`：54/54 通过。新增 OTA 健康策略、提示计时 / 回绕、BLE 广播原子解析、Ultimate 2 报告解析和输入来源发布测试。输入发布覆盖持键接入不生成按下、来源切换、断线归零、SIM 及事件 ID 保留。

LCD debug / release 与 stable debug 已构建通过；ATOM debug / release 双模配置已构建通过，生产构建检查模拟符号不链接。stable 未烧录，最终硬件稳定性尚未验证。提取输入发布模块时发生 SDK 字段误改，构建失败；字段修正后两个 ATOM 配置重建通过。不得把失败构建产物当作部署版本。

## 实机已观察

- 初次 BLE 扫描未识别候选；用户随后确认已连接。后续 UART 状态读到 BLE 状态 3、电量 88%，十秒电量读取多次成功；再配对读取到 86%。初次扫描失败不能反推后续未连接。
- 连续窗口收到 33 字节 HID 通知，约 122 报告 / 秒；主要捕获中立输入，用户完成实体操作的时间早于该窗口，不能据此确认 A/B/X/Y 映射。
- 实机 `ble map` 成功返回完整 113 字节描述，作为输入适配依据。描述不包含设备身份。
- LCD 最新维护网页两轮基础验证通过；OTA 上传测试共 21 个 HTTP 请求，完整镜像损坏 SHA 拒绝、同会话正确镜像重传成功，切换到 ota_0。启动超过六十秒后 UART 显示 `running=ota_0 state=valid`，证明正常启动确认路径。
- LCD 应用烧录保留 NVS / otadata；ATOM 115200 烧录哈希校验通过，保留 NVS。COM8 / COM6 分别为 LCD / ATOM，未操作其他设备。

## 验证边界与待办

实体字母键 / 左右扳机、BLE 到 LCD 控制、相机动作读回、重启自动重连、DS4 / BLE 同时在线切换、输入静默释放、Matrix / LCD 物理视觉仍需实机验收。相机在部分观察窗口离线；更早单次 4.9 FPS 不证明本版性能达标。TCP_NODELAY 效果、80MHz 硬件效果、≥30 分钟稳定性、故障回退注入均未验证。对焦框、倍率 / 放大属性仍需实际协议来源；云台电量协议未知。

本轮未提交或推送。原始本机证据均在忽略目录 `build/`：`resume-ble-diagnostic-observe.log`、`resume-ultimate-input-observe.log`、`resume-ble-map-wait.log`、`resume-final-host-test.log`、`resume-ultimate-final-debug.log`、`resume-ultimate-final-release.log`、`resume-maint-web-test.log`、`resume-ota-upload-test.log`、`resume-ota-confirm-test.log`、`resume-stable-build.log`。不提交原始串口日志与设备身份。

## 最终输入首轮观察与读描述重试

`resume-final-input-atom.log` / `resume-final-input-lcd.log`：ATOM 电量 84%、BLE 状态 3，持续收到 33 字节通知，但 LCD 输入离线，未通过解析门禁。连接初始化日志不在该窗口内，具体失败状态未知，不能宣称 BLE 控制成功。随后观察 BLE 已离线，原因尚未归因。该轮临时 Python 脚本使用默认串口 DTR / RTS，可能影响连接；后续采用先置 False 再打开端口，避免把观察过程当作无干扰状态。

LCD 同窗相机重新连接，电量 93%、AF-C、短窗 4.3–4.9 FPS，显示未失效。仅证明本版取景短窗工作，不满足稳定性或性能目标验收。

新增描述读取在通知初始化之后由 worker 串行发起；失败最多三次重试，读请求十秒超时后恢复；`ble map` 额外打印门禁、通知特征数、描述句柄和尝试次数。未知 / 缺失描述继续拒绝输入。这是故障恢复改进，根因仍须连接日志确认。

## 最终验证终态

读描述重试版 ATOM debug / release 构建通过（`resume-ble-read-final-debug.log` / `resume-ble-read-final-release.log`），COM6 115200 烧录哈希校验通过（`resume-ble-read-final-flash.log`）。预先 DTR / RTS=False 的六十秒观察中 BLE 始终未连接，门禁计数均为零（`resume-ble-read-final-observe.log`）；新重试路径和实体输入尚无成功证据，需手柄重新配对。

最新 LCD stable debug 增量构建通过（`resume-final-stable-build.log`），仍未烧录 stable 配置。输入共享发布的真实 I²C 模拟回归 18 条命令通过（`resume-pad-publish-i2c-test.log`），结束 SIM0、SETTINGS0、释放归零；该回归证明共用发布 / I²C 链路未回退，不证明实体 BLE 映射和相机实际效果。最终 54/54 CTest 与文档链接 / diff 检查通过。工具会话全部结束，无串口继续占用。
