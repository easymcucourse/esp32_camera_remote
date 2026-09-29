# 首轮实测：2026-09-27

原始文件：`captures/remote-20260927-193841.pcapng`。电脑 192.168.110.70，相机 192.168.110.46。持续 120 秒，94,183 包，dumpcap 报告接口丢包 0。未执行任何自制控制命令。

## 已确认

- 相机描述 XML 报告 ZV-E10、固件 2.00、PTP 版本 3.00、配对 Necessary、RemoteControlSupport Enable。
- TCP 15740：PTP/IP 命令连接及独立事件连接；InitCommandRequest/Ack 后使用返回的 connection number 建立 InitEventRequest/Ack。
- TCP 64321：HTTP GET `/DigitalImagingDesc.xml` 返回设备信息。它不是已证实的 HTTP 遥控接口。
- 抓到两组会话初始化，可用于比较重连行为；既有电脑配对记录可能被复用，本次握手不等同于首次配对流程。

## 初始化顺序（首个会话）

所有下面的参数按 uint32 小端解释，原始字节保存在 CSV。

| 操作码 | 参数 | 事务 ID | 证据/解释 |
|---|---|---|---|
| 0x1002 | 1 | 2 | OpenSession |
| 0x9201 | 1,0,0 | 3 | 扩展初始化，语义待核对 |
| 0x9201 | 2,0,0 | 4 | 同上 |
| 0x1001 | 0 | 5 | GetDeviceInfo |
| 0x9202 | 300 | 6 | 扩展请求 |
| 0x9201 | 3,0,0 | 7 | 扩展初始化 |
| 0x9202 | 300 | 8 | 扩展请求 |
| 0x9209 | 0 | 9 | 随后周期性出现，疑似属性查询 |
| 0x1008 | 0xFFFFC002 | 10 | GetObjectInfo |
| 0x1009 | 0xFFFFC002 | 11 起 | 高频 GetObject，疑似实时取景对象 |

两会话共 979 次 0x1009、92 次 0x9209、3 次 0x9205。响应 0x2001（OK）共 1090 次；事件包括 0xC203、0xC207、0xC20C，具体语义尚未确认。

## 已捕获的设置操作

相对抓包开始 58.384、61.124、65.023 秒出现 0x9205，参数均为 0xD21E，事务 ID 分别为 251、283、320。需与用户实际操作对照后命名属性。

第一笔（帧 35216–35220）为：

1. OperationRequest：phase=2，opcode=0x9205，transaction=251，param=0xD21E。
2. StartData：transaction=251，total_length=4。
3. Data：`f4 01 00 00`，按 uint32 小端为 500。
4. EndData：transaction=251，无额外数据。

因此 ESP32 不能把设置简化为“发送一个操作码”：必须实现请求、数据阶段、响应和事务关联。暂不能仅凭数值 500 认定这是 ISO。

## 解析和限制

`tools/analyze.py` 调用 tshark Follow TCP 重组每个方向，然后依应用层 uint32 length/type 拆包，导出 `.ptpip.csv`。direction=0/1 为 tshark Node0/Node1，本次分别为电脑/相机。CSV 按方向排列，并非时间线；时序仍以原始抓包为准。

末尾 stream 5 的相机方向停在一个未收完的包（偏移 74097751，声明长度 72716）；采集在连接仍活动时结束，尾部不完整不代表网络丢包。此前完整包已输出。Wireshark 的部分 Malformed 标记不能直接视为相机错误。

下一步：核实三次设置的操作名称和数值；另抓明确标记的对焦、拍照、录像；解析 GetDeviceInfo 与属性数据集；验证首次配对和身份字段，再实现 PC 最小客户端及 ESP-IDF 状态机。

## ESP32 配对尝试（2026-09-27）

用户确认相机地址为 `192.168.4.2` 并进入配对模式。实现并烧录 `main/camera_pair.c`：独立随机 GUID 持久化至 NVS，以 `ESP32-Camera-Remote` 发起 InitCommandRequest，随后计划验证事件通道、OpenSession/CloseSession 以及相同 GUID 重连。报文结构根据上述原始抓包实现。

本轮结果：更新固件导致 AP 重启后，相机未重新关联；约 126 秒内 12 次 TCP 连接全部失败，热点客户端数始终为 0。没有发送 InitCommandRequest，没有收到配对确认，因此不能宣称配对成功。日志为 `build/camera-pair.log`。需要相机重新加入热点后重试。

### 后续实测：配对成功

增加等待相机 MAC 关联及 UART `p` 重试入口。相机重新连入 `192.168.4.2` 后，首次 InitCommandRequest 收到 type=5、reason=1；用户重新进入相机配对等待画面后，经串口 `p` 重试成功。该轮未重启 AP。

`build/camera-pair-confirm.log` 的关键时序（ESP32 启动后毫秒）：

| 时间 | 结果 |
|---|---|
| 72402 | 发送 InitCommandRequest，名称 ESP32-Camera-Remote |
| 88493 | 收到 InitCommandAck，type=2，46 bytes，名称 ZV-E10 |
| 88502 | 收到 InitEventAck |
| 88584 | OpenSession / transaction 2 返回 0x2001 |
| 88826 | CloseSession / transaction 3 返回 0x2001 |
| 91857 | 使用 NVS 中相同 GUID 再次发送 InitCommandRequest |
| 91860 | 3ms 后收到 InitCommandAck |
| 91870 | 第二次 InitEventAck |
| 92107 | 第二次 OpenSession 返回 0x2001 |
| 92338 | 第二次 CloseSession 返回 0x2001 |

结论：ESP32 独立身份配对、命令/事件通道、标准 PTP 会话及即时重连已实机验证。GUID 已持久化到 NVS；断电后重连尚未单独测试。诊断结束主动关闭 socket，没有执行 Sony 扩展初始化、拍照、录像或参数修改。

## 实时取景 JPEG 读取与 LCD 显示成功

`tools/extract_liveview_sample.py` 从原始抓包 TCP stream 0 的事务 11 提取对象：62,344 bytes，JPEG 从偏移 136 开始，共 62,124 bytes。图像已检查为 1024×576 实时取景画面。对象头第一个 uint32 小端字段与 JPEG 起始位置一致。

ESP32 实机按抓包顺序执行 OpenSession → 0x9201(1,0,0) → 0x9201(2,0,0) → GetDeviceInfo → 0x9202(300) → 0x9201(3,0,0) → 0x9202(300) → 0x9209(0) → GetObjectInfo(0xFFFFC002) → GetObject(0xFFFFC002)。每个事务均返回 0x2001。

| 项目 | 首次开机读取 | UART j 刷新 |
|---|---:|---:|
| 对象大小 | 96,544 bytes | 127,008 bytes |
| JPEG 偏移 | 160 | 160 |
| JPEG 大小 | 96,333 bytes | 126,743 bytes |
| 图像尺寸 | 1024×576 | 1024×576 |
| 解码及 LCD 提交 | 623ms | 623ms |

因此不能硬编码最初抓包里的 136 字节头部长度。当前读取偏移字段后核对 SOI/EOI，并完整验证 PTP/IP StartData、Data、EndData 的事务编号和累计长度；最大对象限制为 512KiB。

LCD 使用 RGB565，完整 JPEG 先解码到 PSRAM 暂存帧，再提交显示，上下黑边各 12 像素。用户已确认画面、颜色及方向正常。两次读取均正常 CloseSession，没有拍照、录像或设置属性的操作。NVS 中的配对 GUID 在本次固件更新后仍可直接握手。

证据：`build/camera-jpeg.log`、`build/camera-jpeg-refresh.log`。当前是一次一帧，支持 UART `j` 刷新，尚未实现连续取景和帧率优化。

## 连续取景及显示稳定性修正

将初始化与帧读取分离：每次会话只执行一次 Sony 初始化，随后循环 GetObject(0xFFFFC002)，事务编号持续递增。帧间消费事件连接的数据并响应 PTP/IP ProbeRequest；网络或协议失败后关闭连接、延迟重连。UART `s` 停止，`j` 恢复；仅允许一个相机任务运行。

显示初版在单帧缓冲上覆盖写，用户报告撕裂/花屏。改为两个驱动帧缓冲，在后台解码，发布完成的帧，并等待两次帧完成通知后再复用旧前台缓冲，避免通知与切换时序竞争。DMA bounce buffer 增至 20 行。用户随后具体描述“刷新起点上下乱动”；将像素时钟从 30MHz 降至 18MHz 后，用户确认问题消失、持续更新稳定。该结果支持并发取景时扫描带宽余量不足的判断，但未测量硬件 DMA 欠载计数。

当前 JPEG 仍以 1024×576 原分辨率显示，典型帧率约 1.2fps，软件解码/帧切换约 600–690ms/帧。像素时钟降低的是 LCD 扫描速率，不是 JPEG 源图尺寸。帧缓冲、JPEG 工作区与会话内收包缓冲复用，CPU1 运行相机任务；没有引入持续增长的帧队列。

日志：`build/liveview-18m.log`。未进行真正的相机断电断网恢复测试，自动重连逻辑的支持与验证范围应分别表述。

18MHz 版本连续记录到 123 帧，PSRAM 剩余约 4.62MB，未见持续下降。随后 UART `s` 成功结束会话（CloseSession 返回 0x2001）；再次输入 `j`，复用持久化身份完成握手和 Sony 初始化，恢复连续取景，记录到 25 帧、约 1.1–1.2fps。停止/恢复日志分别为 `build/liveview-18m-stop.log`、`build/liveview-18m-restart.log`。串口记录器退出后，设备继续运行取景。

## 扩大缓冲及快速 JPEG 解码

按用户要求将 PSRAM 对象接收缓冲从 512KiB 增至 1MiB；LCD 每个 bounce buffer 从 20 行增至 30 行，两块内部 RAM 缓冲合计由 80KiB 增至 120KiB。保持双帧缓冲和已验证的 18MHz 像素时钟，提升目标是实际取景更新帧率。

引入固定版本 `espressif/esp_new_jpeg` 1.0.2。1024×576 图像直接解码为 RGB565_LE，输出到后台帧缓冲第 12 行（16 字节对齐）；检查头部尺寸和所需输出长度后才解码。其他尺寸仍使用 ROM TJpgDec 路径。组件说明见 [Espressif 官方文档](https://components.espressif.com/components/espressif/esp_new_jpeg/versions/1.0.2/readme)。

编译、烧录哈希校验通过，沿用 NVS 身份自动重连。实机日志 `build/liveview-fast.log` 测得约 2.2–2.5fps，单帧解码及显示约 214–244ms；PSRAM 剩余约 4.03MB，内部 RAM 约 63–66KB。用户移动相机后确认画面正常且稳定。该帧率为本次 Wi-Fi 和画面条件下的测量值，不是固定保证。

## 独立 JPEG 任务与 SIMD 核验

相机任务固定 CPU0，新增 CPU1 `jpeg_decode` 任务（8KiB 栈、优先级 4）。两块 1MiB PSRAM 对象缓冲分别通过空闲槽队列和待解码队列转移所有权；通信任务只写自己持有的槽，解码任务完成显示后才归还槽。队列均有界；停止或失败时发送结束项，等待解码任务排空后释放资源。LCD 仍由单个解码任务操作，保留双帧缓冲与帧完成同步。

固定版本 esp_new_jpeg 1.0.2 已自动链接 `lib/esp32s3/libesp_new_jpeg.a`。映射文件确认引用 `jpeg_dec_idct.S.obj` 中 `IDCT_BLOCK_8_8_128ASM`，库反汇编包含 `ee.vld.128.ip`、`ee.vmul.s16` 等 ESP32-S3 SIMD 指令，记录在 `build/jpeg-simd-disassembly.txt`。本轮新增的是双核流水线，SIMD 解码实现上一版本已使用。

编译及烧录校验通过。`build/liveview-pipeline.log` 实测持续约 3.85–4.01fps，显示阶段约 254ms，解码任务栈余量至少 5832 字节，PSRAM 剩余约 2.98MB。用户确认画面正常且稳定。UART 停止测试中解码任务完成 182 帧后退出，CloseSession 返回 0x2001（`build/pipeline-stop.log`）；再次启动成功创建解码任务并恢复取景（`build/pipeline-restart.log`）。未进行断网故障注入测试。

## FPS 叠加与全局 O2

屏幕右上角加入黑底白字 `FPS x.x`，采用内置 5×7 点阵字体放大三倍，在后台帧缓冲完成 JPEG 解码后绘制。以成功完成 LCD 帧提交同步的时间间隔统计，约每秒更新，显示的是取景更新帧率而非 LCD 扫描频率；暂停时保留最后的读数，恢复后重新统计。

`sdkconfig.defaults` 和本机 `sdkconfig` 均启用 `CONFIG_COMPILER_OPTIMIZATION_PERF=y`，重新编译整个工程。`build/compile_commands.json` 确认 app_main、camera_pair、board_7b 使用 `-O2`；预编译库不重新优化。编译与烧录哈希校验通过，日志 `build/fps-o2-run.log` 测得约 3.5–3.7fps；用户确认 FPS 显示正常、画面稳定。

烧录前在旧固件执行停止时捕获到 pair_console 栈溢出（`build/before-fps-o2-stop.log`），将其栈从 2048 增至 4096 字节。新版停止时解码任务排空 108 帧，CloseSession 返回 0x2001，恢复取景成功，本次复测未见栈溢出。证据：`build/fps-o2-stop.log`、`build/fps-o2-restart.log`。

## 0x9209 属性数据与详细屏幕

原始抓包的首个 `0x9209` 返回 2811 字节，开头为 93 个条目和 4 字节保留值。每个 Sony 属性条目布局为：`code:u16`、`type:u16`、`getset:u8`、`enabled:u8`、默认值、当前值、表单及约束。它比标准 PTP DevicePropDesc 多一个 `enabled` 字节；把标准格式直接套用会得到看似合理但错误的数值。

抓包首个快照中，白平衡 `0x5005` 当前值为 2，F-number `0x5007` 为 `0xFFFE`（无可显示值），对焦 `0x500A` 为 1，测光 `0x500B` 为 `0x8001`，闪光 `0x500C` 为 2，曝光模式 `0x500E` 为 `0x00078051`，EV `0x5010` 为 0，快门 `0xD20D` 为 `0xFFFFFFFF`（无可显示值），ISO `0xD21E` 为 `0x00FFFFFF`（AUTO）。`0xD20F` 是色温，不作为光圈回退值。

第三方实现与 Sony Camera Control PTP 参考表均确认：`0x00078051` 表示 **Movie Recording (A)**，即视频模式下的光圈优先；`0x8001` 在属性 `0x500B` 中表示 **Multi**，即多重测光。固件据此显示 `MODE MOVIE A` 和 `METER MULTI`。Sony 的枚举值必须结合属性代码解释，同一个 `0x8001` 在白平衡或闪光属性中有不同含义。

交叉验证来源：

- [libgphoto2 Sony 32-bit exposure program table](https://github.com/gphoto/libgphoto2/blob/master/camlibs/ptp2/config.c)
- [pysonycam Sony property enumerations](https://github.com/olkham/pysonycam/blob/main/pysonycam/constants.py)
- [Sony Camera Image Edge replacement property reference](https://github.com/Fireflaker/Sony-Camera-Image-Edge-REPLACEMENT/blob/main/ptp_property_reference.md)

固件现在读取每个条目的当前值而非默认值，并每 5 秒重新请求 `0x9209`。详细屏幕将已知 Sony 编码转换为 ISO、光圈、快门、EV、拍摄模式、白平衡、对焦和测光名称；尚未确认的扩展枚举继续显示原始十六进制值。
