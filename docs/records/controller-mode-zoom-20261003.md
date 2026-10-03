# 2026-10-03 手柄类型与电动变焦

用户要求 OTA 菜单选择 DS / Xbox 兼容手柄，暂时按 DS 开发，并确认当前是电动变焦镜头。

维护页 OTA 区增加选择，默认 DS，NVS 保存及恢复出厂默认 DS。DS 仅发布经典 DS4，Xbox 兼容只发布已适配 BLE；切换释放旧动作、重新 HELLO、清旧事件和增加来源代数。两端 HELLO 增加输入类型参数 / 能力位，旧 ATOM 缺少该能力时 LCD 不进入 POLL，必须两端升级。

当前镜头使用用户声明 POWER_ZOOM，并非自动识别。R1 请求 Tele(+1)，L1 请求 Wide(−1)，松开发送0；同按、断连、输入来源改变保持停止与重新按下门禁。已确认 PZ 时无需 ZoomEnableStatus 为1才提交；相机接受响应与镜头实际运动分别验证，更换镜头需重新确认声明。

验证：
- 54/54 CTest 通过；新增类型协商 / 缺失能力拒绝、来源过滤、已确认PZ无状态及状态0时变焦、停止回归。
- 维护网页 JavaScript Node --check 通过。
- LCD / ATOM debug、release 共四项构建通过，release 模拟符号排除检查通过。
- COM8 应用仅写 ota_0 0x20000，COM6 应用仅写0x10000，两端哈希通过；保留 NVS / otadata。
- 首轮 HTTP 探测在23请求时被测试驱动提前关闭维护中断；随后等待探测完成，24项实机 TCP 回环检查通过（认证、非法值 / 字段 / 重复、DS / Xbox 保存与GET读回），恢复原DS。该探测不代表手机页面视觉或 Wi-Fi 空口验收。
- 新固件启动：ota_0 valid、I²C v2握手成功、DS / POWER_ZOOM / zoom_available=1、SIM0。重启后相机、DS4暂离线，已请用户重连及实体键验证；暂不宣称相机变焦命令接受或镜头运动通过。

本机证据 build/controller-host-test.log、controller-*-debug.log、controller-*-release.log、controller-*-flash.log、controller-probe-lcd.log / atom.log。日志均忽略，不提交原始设备身份。

无新增 Git 提交或推送。项目其余未闭环功能见实施状态。

后续用户更正：R1拉近、L1拉远；已反转变焦肩键方向，条件MF方向保留。相关4项回归（gamepad_input / camera_actions / sony_write / doc_links）及LCD debug/release通过。烧录前状态相机和真实DS4已重新在线，不能沿用此前离线快照。新构建与烧录日志为build/zoom-direction-*.log。
