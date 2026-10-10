# 取景内存与帧率优化计划（TCP pbuf → PSRAM / 增大窗口 / 增加 JPEG 槽 / 提升 FPS / LCD 重启后重连）

[English](../en/design/liveview-memory-fps-plan.md) · **简体中文** · [日本語](../ja/design/liveview-memory-fps-plan.md)

日期：2026-10-07。本文主体保留用户提供的原始分析与计划；执行进度与纠正见 [阶段 0 记录](../records/liveview-baseline-20261007.md)。估算不是验收证据。

执行时已确认：当前存在 build 产物；SPIRAM_USE_MALLOC 已启用。启用 SPIRAM_TRY_ALLOCATE_WIFI_LWIP 会改变新 sdkconfig 的 Wi-Fi 默认静态 RX 数与 RX BA 窗口，因此阶段 1 须显式固定原值以隔离变量。身份持久化当前在 components/common_runtime/camera_identity_store.c，不在 app_camera；NVS 写入仍必须经内部 RAM 栈 worker。其余阶段按实测门禁推进，未通过不宣称完成。

当前 ui_jpeg_renderer.c 的全屏和设置缩略图均可走 SIMD 解码，设置页再缩图；下文“设置页 ROM 解码161ms”的历史估计不代表当前实现。纯设置页阶段0实测3.27FPS、纯全屏4.83202FPS；首次混页采样被一致性检查拒绝，已补采，混合页统计不作为阶段对比。

阶段1实测/完整Kconfig差异纠正：PSRAM优先强制静态TX（动态TX不可选），默认16个内部DMA缓冲，TX缓存32；乱序pbuf默认从4变成0不限量。首版内部低水位降至15031字节，故不通过；修正版降低静态TX数并固定乱序上限，依据主任务实测栈余量调整启动栈，见[阶段1记录](../records/liveview-stage1-20261007.md)。SDK的LWIP_TCP_OOSEQ_MAX_PBUFS范围是0..12，下表B的16/C的32不可直接配置，执行时必须在合法范围内调整并记明。门禁内存单位应记录具体字节数，不混淆十进制MB与MiB。

依据：当前源码、`sdkconfig.defaults`、`dependencies.lock`（ESP-IDF 5.5.1，esp32s3）及 `docs/records` 中的实机记录。原分析撰写时假设没有 `build/` 产物；执行阶段已确认有产物。下文未经实测的lwIP/Wi-Fi数值仍属 **原默认值推断**，第 0 阶段必须先用实际 `build/<flavour>/sdkconfig` 和 `idf.py size` 核对。

---

## 1. 当前内存占用分析

### 1.1 已有的实机数据

| 来源 | 数据 |
| --- | --- |
| `communication-recovery-test-20261006.md` | 10 行 bounce 版本启动后 `free_internal=72331`，Router 时 internal=80847；30 行 bounce 时 Router 只剩 3019 B、最大块 1984 B，导致启动失败 |
| `display-uart-test-20261003.md` | 内部 RAM 43519、PSRAM 2386504（当时缓冲配置与现在不同） |
| `display-profile-test-20261003.md` | 真实取景窗口：JPEG 178897 B、`read=1100ms`、`display=261ms`、2.81 FPS |
| 其他记录 | 全屏取景 FPS 在 2.2–4.9 之间波动，录像时最高约 6.5；设置页约 2.3–3.3 |

结论：**内部 RAM 是最紧的资源**（启动后约 70 KB 余量，且曾因 bounce buffer 多 80 KB 就启动失败）；PSRAM 还有约 2 MB 级余量，但需要重新测量。

### 1.2 内部 RAM（DRAM）主要占用（按源码 + 默认值估算）

| 占用者 | 估算大小 | 说明 |
| --- | --- | --- |
| LCD bounce buffer | 40960 B | `board_lcd.c`：`BOARD_LCD_WIDTH * 10` × 2 块，DMA 必须在内部 RAM |
| Wi-Fi 静态 RX 缓冲 | 约 16 KB | 默认 `ESP_WIFI_STATIC_RX_BUFFER_NUM=10`，每个约 1.6 KB，DMA 必须内部 |
| Wi-Fi 动态 RX / TX 缓冲、lwIP pbuf | 随流量变化，峰值可达数十 KB | 默认 **全部从内部 RAM 分配**（未启用 `SPIRAM_TRY_ALLOCATE_WIFI_LWIP`） |
| 项目内部栈任务 | 约 50 KB | health 4K、router 4K、UART 4K、input 4K、ATOM 3K、SIM 3K、camera endpoint 4K、identity 4K、Wi-Fi endpoint 4K、两条 TCP lane 8K、Wi-Fi config 4K、HTTP 6K |
| IDF 系统任务 | 约 25 KB | wifi、tcpip、esp_timer、event loop、2×IPC、2×idle 等 |
| 队列控制块、`app_message_t` 静态 / 队列 | 数 KB | router 控制块内部、envelope 存储在 PSRAM；`camera_endpoint` 结果队列 4×`app_message_t` 用 `xQueueCreate` 在内部；`ui_frames.c` 的 `pending[2]` 在 .bss |
| `SPIRAM_MALLOC_RESERVE_INTERNAL` | 32 KB（默认） | 留给 DMA / 内部专用分配的保留池 |

当前窗口很小（默认 `LWIP_TCP_WND_DEFAULT=5760`，约 4 个 MSS），所以 TCP 接收在内部 RAM 上的瞬时占用不大。**这也是窗口不能直接加大的原因**：64 KB 窗口在最坏情况下要占用约 70 KB+ 的 pbuf，内部 RAM 承受不了。必须先把 pbuf 移到 PSRAM，再加大窗口。

### 1.3 PSRAM 主要占用（8 MiB 八线 PSRAM）

| 占用者 | 大小 | 说明 |
| --- | --- | --- |
| XIP 拷贝（`SPIRAM_FETCH_INSTRUCTIONS` + `SPIRAM_RODATA`） | 约等于 .text + .rodata（镜像约 3.3–3.5 MiB） | 启动时把代码 / 只读数据拷到 PSRAM，这部分不进 heap；需用 `idf.py size` 确认 |
| LCD 两块全屏帧缓冲 | 2,457,600 B | 1024×600×2×2 |
| 相机 JPEG 包缓冲（帧槽） | 1,048,576 B | `camera_runtime.c`：`OBJECT_CAPACITY=512KiB` × 2，开机分配，保留到重启 |
| PSRAM 栈任务 | 96 KB（+ debug bench 32 KB） | camera_pair、UI endpoint、UI refresh 各 32 KB |
| 字体 / glyph cache、JPEG 工作区、router envelope、OTA chunk | 数十～数百 KB | FreeType 优先 PSRAM；JPEG work 4 KB；OTA 4 KB 仅维护时 |

PSRAM 余量大致在 2 MB 级别（需实测），足够容纳：第三个 512 KiB 帧槽 + Wi-Fi/lwIP 缓冲（窗口 64 KB 时也只需几百 KB）。

### 1.4 取景链路耗时结构（决定 FPS 的因素）

```
Camera 任务 (CPU0)                         UI endpoint (CPU1)
GetObject(0xffffc002) → TCP recv → 槽 N  ──lease──▶  解码 → 叠加 → publish(等 2 次 vsync) → 结果回传
```

- **网络读取**：真实样本 179 KB 用 1100 ms，约 160 KB/s。窗口 5760 B ÷ 每轮往返（Wi-Fi 下 10–35 ms）正好落在这个量级，说明**读取被 TCP 窗口限制**。另外窗口只有约 4 段时，一旦丢一个包，后面不够 3 个重复 ACK，相机端只能等超时重传（几百 ms），很可能就是 read 抖动到 1 s 的原因（待抓包验证，可用 `tools/capture.ps1`）。
- **显示**：全屏路径清零约 1 ms、解码约 75 ms、叠加约 6 ms、publish 约 51–96 ms。`board_lcd_publish` 要等两次帧完成；18 MHz 像素时钟下整帧约 1386×661 像素 ≈ 19.6 Hz（51 ms/帧），所以 publish 本身就占 1–2 帧时间。全屏显示上限约 5.5–7 FPS，与 bench 实测 6.5 FPS 一致。
- **双槽流水线**：camera 读槽 B 的同时 UI 显示槽 A，稳态 FPS ≈ 1 / max(读取, 显示)。**当前读取远慢于显示，所以单纯加槽不会提升平均 FPS**；加槽的作用是吸收读取抖动、掩盖 UI 结果回传延迟。真正的大头是窗口。

---

## 2. 目标与验收指标

| 指标 | 现状 | 目标 |
| --- | --- | --- |
| 全屏取景 FPS（相同 Wi-Fi 环境，10 分钟） | 2.2–4.9，波动大 | 平均 ≥ 5，最低 5 秒窗口 ≥ 4 |
| 设置页 FPS | 2.3–3.3 | 不低于现状（≥ 2.4，需求 R3.2） |
| `LIVEVIEW read=` P50 / P95 | 约 250 ms / 1100 ms | P50 ≤ 150 ms，P95 ≤ 300 ms |
| `min_internal`（取景 10 分钟后） | 未有取景期间数据 | ≥ 40 KB，且比第 0 阶段基线不降低 |
| `largest_internal` | 未知 | ≥ 16 KB |
| `min_psram` | 未知 | ≥ 1 MB |
| 稳定性 | 30 分钟在线测试仍未通过 | 30 分钟无 `NO_MEM`、无 Stream 退出（与 0x200F 问题分开判定） |

---

## 3. 分阶段计划

每一阶段单独构建、烧录、测量，**通过后再进入下一阶段**，任何阶段失败可单独回退。前两阶段只改 `sdkconfig.defaults`，不动 C 代码。

> 注意：`sdkconfig.defaults` 只在生成新 `sdkconfig` 时生效。`tools/ci_build.py` 每次用新的 `build/<flavour>/sdkconfig`，按 `sdkconfig.defaults` → `tools/ci/<flavour>.defaults` → `lcd-<flavour>.defaults` →（stable）`sdkconfig.stable.defaults` 叠加。本地需备份后移开旧 `sdkconfig` 再生成；仅 `idf.py reconfigure` 不会覆盖已有值，并用 `build/<flavour>/sdkconfig` 确认最终值。

### 阶段 0：基线测量（不改任何东西）

1. 用当前三个构建（Default / Stable / Release）跑 `idf.py size`、`idf.py size-components`，记录 DRAM、IRAM、PSRAM XIP 占用。
2. 从 `build/<flavour>/sdkconfig` 摘出实际值：`ESP_WIFI_STATIC_RX_BUFFER_NUM`、`ESP_WIFI_DYNAMIC_RX_BUFFER_NUM`、`ESP_WIFI_DYNAMIC_TX_BUFFER_NUM`、`ESP_WIFI_RX_BA_WIN`、`ESP_WIFI_TX_BA_WIN`、`LWIP_TCP_WND_DEFAULT`、`LWIP_TCP_SND_BUF_DEFAULT`、`LWIP_TCP_RECVMBOX_SIZE`、`LWIP_TCPIP_RECVMBOX_SIZE`、`LWIP_TCP_MSS`、`LWIP_TCP_OOSEQ_MAX_PBUFS`、`SPIRAM_USE_MALLOC`、`SPIRAM_MALLOC_ALWAYSINTERNAL`、`SPIRAM_MALLOC_RESERVE_INTERNAL`、`SPIRAM_TRY_ALLOCATE_WIFI_LWIP`。
3. 实机全屏取景 10 分钟（固定相机位置、相同信道），用 `tools/test_camera_connection.py` / `serial_log.py` 收集：
   - `LIVEVIEW frames fps JPEG read display` 每 5 秒一行 → 统计 FPS、read、display、JPEG 大小的 P50/P95/最大值；
   - 10 秒健康日志与 `status` → `free/min/largest` 的 internal 与 psram；
   - `JPEG phases us` → 解码与 publish 分项。
4. 可选：`tools/capture.ps1` 抓一段 TCP，确认重传 / 零窗口 / 窗口满的情况，验证“窗口限速 + 超时重传”假设。

输出：一份基线记录（`docs/records/liveview-baseline-YYYYMMDD.md`），后续各阶段都对照它。

### 阶段 1：TCP / Wi-Fi pbuf 移到 PSRAM（仅配置）

改动（`sdkconfig.defaults`）：

```
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

- `SPIRAM_TRY_ALLOCATE_WIFI_LWIP` 让 Wi-Fi 动态 RX/TX 缓冲和 lwIP 的 `mem_malloc`（PBUF_RAM、乱序段、socket 结构等）优先从 PSRAM 分配。esp-wifi 收到的数据是以 PBUF_REF 包住 Wi-Fi 动态 RX 缓冲交给 lwIP 的，所以“pbuf 进 PSRAM”实际就是这两类缓冲进 PSRAM。
- 静态 RX 缓冲仍在内部 RAM（DMA 要求），不受影响。
- 保留 `SPIRAM_MALLOC_RESERVE_INTERNAL=32768`，不调低。

预期：取景期间 `min_internal` 明显上升（因为 Wi-Fi/lwIP 不再吃内部堆），读取速度基本不变（S3 八线 PSRAM 120 MHz，`recv` 的 PSRAM→PSRAM 拷贝 180 KB 约 1–2 ms）。

风险与验证：

- PSRAM 带宽：LCD 扫描约 36 MB/s 持续读 PSRAM，再加 Wi-Fi 写 PSRAM，需要观察是否出现画面撕裂 / 扫描抖动（历史上 30/40 MHz 像素时钟出过问题），Stable 档（80 MHz PSRAM）也要测。
- Flash 写期间（NVS 配对写、维护 OTA 上传）的 Wi-Fi 行为：回归 `maint-ota-upload`、配对确认流程。
- 判据：FPS 不低于基线；`min_internal` 提升；30 分钟无 `NO_MEM`。

### 阶段 2：增大 TCP 接收窗口及配套缓冲（仅配置，分三档）

相机是发送方，ESP32 是接收方，决定吞吐的是 **ESP32 的接收窗口** 和 Wi-Fi 的 AMPDU 接收窗口。分档上调，每档单独测量：

| 配置项 | 默认（推断） | 档 A | 档 B | 档 C |
| --- | --- | --- | --- | --- |
| `CONFIG_LWIP_TCP_WND_DEFAULT` | 5760 | 16384 | 32768 | 65534 |
| `CONFIG_LWIP_TCP_RECVMBOX_SIZE` | 6 | 16 | 32 | 64 |
| `CONFIG_LWIP_TCPIP_RECVMBOX_SIZE` | 32 | 64 | 64 | 64 |
| `CONFIG_LWIP_TCP_OOSEQ_MAX_PBUFS` | 4 | 8 | 16 | 32 |
| `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM` | 32 | 32 | 48 | 64 |
| `CONFIG_ESP_WIFI_RX_BA_WIN` | 6 | 16 | 16 | 32 |
| `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM` | 10 | 10 | 16 | 16 |
| `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` | 5760 | 不变 | 不变 | 不变 |

约束与说明：

- `TCP_WND` 不超过 65535，不开窗口缩放（`LWIP_WND_SCALE`），没必要。
- `TCP_RECVMBOX_SIZE` 至少为 `TCP_WND / MSS`（MSS 1440 时：16 KB→12，32 KB→23，64 KB→46），否则窗口开了也会被邮箱卡住。
- `RX_BA_WIN` 不大于动态 RX 缓冲数，具体上限以 menuconfig 的依赖检查为准。
- 静态 RX 缓冲在内部 RAM：10→16 增加约 9.6 KB 内部占用，只在档 B 起调，并以阶段 1 腾出的内部余量为前提。
- `TCP_SND_BUF` 只影响 ESP32→相机方向（PTP 命令都很小），保持不变以节省内存。
- `TCP_WND` 是全局默认值，会作用于所有 socket（PTP 命令 lane、事件 lane、维护 HTTP 的 3 个 socket）。最坏情况 PSRAM 占用 ≈ 活动 socket 数 × 窗口 × 约 1.15（pbuf 开销）；档 C 时也只有几百 KB，在 PSRAM 余量内。事件 lane 和 HTTP 实际流量小，不会真占满。
- 可选的 IRAM 优化（`CONFIG_LWIP_IRAM_OPTIMIZATION`）会额外占约 10 KB IRAM（S3 上 IRAM 与 DRAM 共享），先不开；如果档 C 后 CPU0 成为瓶颈再评估。

理论估算：179 KB 帧，若吞吐提升到 1.5–2 MB/s，读取约 90–120 ms，加上相机响应延迟，预计 read 降到 150 ms 左右，全屏 FPS 由读取限制转为由显示限制（约 5.5–7 FPS）。

判据：read P50/P95 达到第 2 节目标；FPS 提升；`min_internal`、`min_psram` 在目标内。档 B 达标就停在档 B，只有收益明显才上档 C。

回退：删除对应行即可恢复默认。

### 阶段 3：JPEG 缓存槽 2 → 3（需要改代码，本文只列出改动点）

**目的**：读取速度提升后，读取和显示耗时接近，抖动会让任一方空等；第三个槽让相机在 UI 解码 + 发布期间还能继续读下一帧，同时掩盖 UI 结果回传到 camera 任务的延迟。**它不会单独提高平均 FPS**，必须在阶段 2 之后做。

内存代价：PSRAM +512 KiB（共 1.5 MiB）；内部 RAM 增加约 1～2 个 `app_message_t`（UI `pending` 环、可能的结果队列）。

需改动的位置：

| 文件 | 改动 |
| --- | --- |
| `components/app_camera/private/camera_frames.h` | `CAMERA_FRAME_SLOTS 2` → 3；`camera_frames_init` 改为接收缓冲数组 + 数量 |
| `components/app_camera/camera_frames.c` | `init` 写死了 `slots[0]/slots[1]` 和两缓冲间距检查，改为循环赋值 + 两两不重叠检查；其他函数已按 `CAMERA_FRAME_SLOTS` 循环，逻辑不变 |
| `components/app_camera/private/camera_stream.h`、`camera_stream.c` | `camera_stream_begin(first, second, ...)` 改为数组参数 |
| `components/app_camera/camera_runtime.c` | `packet_buffers[2]`、`app_camera_init` 的 `i<2` 分配 / 释放循环、`liveview(session, first, second)`、`pair_task` 传参、两处 “2x512KiB / 2 x 512 KiB” 日志文字 |
| `components/app_ui/ui_frames.c` | `pending[2]`、`count<2`、`%2`、`count==2` 改为与槽数一致的常量（UI 最多要同时持有 N 个待回传结果） |
| `components/app_ui/ui_message_endpoint.c` | UI endpoint `{16, 2}` 的 bulk 深度改为 ≥ 3，否则第三帧在 router 被拒 |
| `components/app_camera/camera_endpoint.c` | 结果队列 `xQueueCreate(4, …)` 改为 ≥ 槽数 + 1（建议 6），并更新 “Two slots mean…” 注释 |
| 主机测试 | `tests/host/test_camera_frames.c`、`test_ui_frames.c`、`test_frame_bus_integration.c`、`test_camera_stream.c`、`test_camera_producer.c`：加 3 槽乱序完成、全部占满、结果按序应用、排空等用例 |
| 文档 | `docs/design/module-resource-ownership.md`（JPEG frame slots、Router inbox 表）、`architecture-design.md`、`docs/development/serial-log.md` 的日志文本 |

保持的不变量：

- 每个槽复用仍需 **lease 归还 + UI 结果** 两个条件都满足；
- 结果仍按 token 顺序应用（`apply_completed`）；
- 停止时 `camera_frames_drained` 必须等所有槽归还。

可选项（建议做成独立开关，默认关闭）：

- **只显示最新帧**：UI 发现 bulk 队列里已有更新的帧时，旧帧直接以 `ESP_ERR_NOT_FINISHED` 回传（计入 dropped），避免 3 槽带来的额外 1 帧显示延迟。
- **槽容量**：基线中若 JPEG 最大值远小于 512 KiB（例如 P99.9 < 300 KB），可评估 3 × 384 KiB（总量 1.125 MiB，基本不增加 PSRAM）。超出容量的帧由现有 PTP 容量检查拒绝，不会越界，但会掉帧，所以要以实测最大值为准。

判据：FPS 不低于阶段 2；FPS 波动（5 秒窗口最低值）改善；显示延迟主观无明显增加；停止 / 重连 / 维护切换时排空正常。

### 阶段 4：其他提升 FPS 的手段（按预期收益排序，均需改代码，逐项评估）

1. **LCD 异步发布**（预期收益最大，显示路径每帧省约 50–100 ms）：`board_lcd_publish` 目前在 UI endpoint 线程里同步等待两次帧完成。可改为提交后立即返回，在下一次获取后台缓冲前再等待完成。必须保持现有“`ready` 为假期间不碰像素内存”的所有权规则；LCD 显示异常问题尚未定位，此项要在那之后做，并单独做视觉验收。完成后全屏显示上限可从约 6.5 FPS 提高到约 9 FPS。
2. **Wi-Fi 带宽 / 信道**：检查 AP 当前带宽（`esp_wifi_get_bandwidth(WIFI_IF_AP)`），环境干扰小时评估 HT40；记录信道与 RSSI，便于对比测试。受环境影响大，只作为可选项。
3. **设置页路径**：ROM 解码 + 缩小约 161 + 27 ms，是设置页 FPS 低的主因，与本计划的网络优化无关，单独立项。
4. **不建议改动**：LCD 像素时钟 18 MHz（30/40 MHz 实测异常）、bounce buffer 行数（内部 RAM 不够）、相机循环每轮 `vTaskDelay(1)`（只有 1 ms，收益可以忽略）。

---

## 4. 预期效果汇总

| 阶段 | 内部 RAM | PSRAM | 预期全屏 FPS |
| --- | --- | --- | --- |
| 基线 | 启动后约 70 KB 空闲 | 约 2 MB 级空闲（待测） | 2.2–4.9 |
| 1：pbuf → PSRAM | 取景时余量增加 | 多占几十 KB | 不变 |
| 2：窗口 16/32/64 KB | 档 B 起静态 RX 多约 9.6 KB | 多占约 100–300 KB | 约 5–6.5（转为显示限速） |
| 3：3 个 JPEG 槽 | 多 1～2 个消息结构 | +512 KiB | 平均基本不变，波动减小 |
| 4.1：LCD 异步发布 | 不变 | 不变 | 约 7–9（取决于读取） |

这些数值是估算，以每阶段的实测为准。

## 5. 测试与记录要求

- 每阶段：Host 全部通过、三档构建（Default / Stable / Release）通过、Release 禁模拟符号与模块边界检查通过、实机全屏 10 分钟 + 设置页 2 分钟、停止 / 重启取景、维护模式进出各一次。
- 最终：30 分钟在线稳定性测试，记录最低 FPS、最低 internal / PSRAM、重连次数（需求 `sony-ptpip-request.md` R3.2 / 30 分钟项）。
- 0x200F 拒绝取景和 LCD 显示异常是已知未解决问题，测试时要分开记录，不能把它们算作本计划的回归，也不能把本计划的通过当成这两个问题已解决。
- 每阶段写一份 `docs/records/` 记录，写明构建大小、实际 sdkconfig 值、日志位置和结论。

---

## 6. LCD 重启后需要重启相机才能重连：原因分析与对策

现象：LCD 重启（OTA、网页保存、显示故障软重启、断电）后，相机不会自动恢复取景，要重启相机才行。与上面的性能阶段相互独立，可以单独排期，建议优先于阶段 3。

### 6.1 可能原因

| 编号 | 原因 | 依据 | LCD 侧能否解决 |
| --- | --- | --- | --- |
| A | **相机已重新连上热点，但沿用旧 IP、没有重新走 DHCP**，LCD 的 DHCP 租约表是空的，发现阶段直接把它过滤掉 | `wifi_esp32.c` 的 `clients()` 只返回 `esp_netif_dhcps_get_clients_by_mac` 查到 IP 的站点（`if (pairs[i].ip.addr)`）；`camera_discovery.h` 写明“只用 DHCP 快照，不扫描 IP 段”。LCD 重启后租约表清空 | 能，最可能也最好修 |
| B | **相机没有重新连上热点** | `protocol-analysis.md` 2026-09-27：AP 重启后 126 秒内客户端数始终为 0 | 只能缓解（相机自身行为） |
| C | **相机连上了，但拒绝新会话**：非正常重启时没有发 CloseSession，相机认为旧会话仍在 | 正常重启路径会排空相机并发 CloseSession（`camera_backend_sony.c` 第 192 行 `orderly`），断电 / panic / 看门狗复位不会 | 部分能（重试策略） |

### 6.2 先诊断（不改代码）

复现时**先不要重启相机**，用串口记录：

1. Wi-Fi 状态里的已关联客户端数，以及 `Associated DHCP clients: N`（`wifi_console.c`）。
   - 已关联 ≥ 1 而 DHCP 客户端 = 0 → 原因 A。
   - 已关联 = 0 → 原因 B。
2. 相机 phase 文字（`Waiting for paired camera...` / `Reconnecting to paired camera...`），以及 `ptpip` 日志中是否有 `INIT_FAIL` / `initialization_reason`、connect 失败 → 原因 C。
3. 分别测试**正常重启**（网页保存 / OTA）和**断电重启**两种情况，记录是否都需要重启相机。
4. 可选：手机连同一热点抓包，或在 LCD 侧临时打印 ARP 表，确认相机重连后的 IP 和有无 DHCP 报文。

### 6.3 对策 A：已配对相机无 DHCP 租约时按记住的 IP 连接（建议优先实现）

思路：配对成功时把相机 IP 和 MAC 一起保存；发现阶段如果某个**已关联**站点的 MAC 与配对 MAC 相同但没有租约，就用保存的 IP（或 ARP 表中该 MAC 对应的 IP）去探测 15740 端口。配对校验不变：仍然只接受已配对的 MAC，并在 InitCommandAck 中校验 GUID。

改动点：

| 文件 | 改动 |
| --- | --- |
| `components/wifi_esp32/wifi_esp32.c` `clients()` | 已关联但无租约的站点不再丢弃，而是以 `ip` 为空、带 MAC 返回；或在 LCD 侧用 lwIP `etharp_find_addr` / `etharp_get_entry`（需持有 tcpip core lock）按 MAC 反查 IP 并填入 |
| `components/app_wifi/include/app_wifi.h` | 更新契约注释（目前写着 “Only associated peers with DHCP leases”），增加“IP 来源：DHCP / ARP / 无”标志 |
| `components/app_camera/camera_discovery.c` / `.h` | 已配对且 MAC 匹配、IP 为空时，使用身份中保存的 IP 探测；未配对时保持原规则（必须有 DHCP 租约），避免误连其他设备 |
| `components/app_camera/camera_identity_store.c` 及身份结构 | `sony_remote` 命名空间新增“上次 IP”字段；配对确认 / 每次会话验证成功后更新（只在变化时写 NVS，避免频繁写 Flash） |
| `components/app_camera/camera_runtime.c` | 把 discovery 得到的 IP 来源写入日志与 phase，便于区分 |
| 主机测试 | 发现：有租约、无租约 + MAC 匹配、无租约 + MAC 不匹配、未配对无租约、多候选；身份存储：IP 字段读写与旧数据兼容（旧 blob 没有 IP 时视为未知） |

风险：相机若换了 IP 而 ARP 表也没有记录，探测会失败，此时回到原有退避重试，不比现状差。

### 6.4 对策 B：缩短热点消失时间，减少相机放弃重连的概率

- 启动顺序中尽早启动 AP（目前要等 Core 启动流程），让相机断开到热点恢复的时间尽量短；记录实际“复位 → `WIFI_EVENT_AP_START`”耗时。
- SSID、密码、信道、认证方式保持不变（已是如此）；确认 BSSID（AP MAC）重启前后一致。
- 正常重启前（`app_health.c` 的重启路径）在 CloseSession 之后主动 `esp_wifi_deauth_sta` 断开相机，让相机收到明确的断开而不是信标丢失，比较两种方式下相机是否更愿意自动重连。需实机对比，效果取决于机型。
- 如果实测确认相机断开后完全不重试，只能在 LCD 提示用户在相机上重新选择连接，而不必关机；把这一点写进 `docs/user-guide/troubleshooting.md`。

### 6.5 对策 C：会话被拒时的重试策略

- 记录 `INIT_FAIL` 的 `initialization_reason`，区分“忙 / 旧会话未释放”和“拒绝配对”；前者不当作配对失败，按现有 `camera_retry_delay`（1→2→4→8→16→30 s）继续重试，不提示重新配对。
- 连接 15740 被拒或超时同样继续退避重试；观察相机多久会自行释放旧会话。
- 非正常重启后，相机若在旧 TCP 连接上发送任何数据（如事件通道的 ProbeRequest），LCD 新的协议栈会回 RST 让相机清掉旧连接；如果实测相机长时间不释放，再评估在发现阶段先对相机 15740 端口做一次短连接 / 立即关闭来促使其清理。

### 6.6 验收

| 场景 | 判据 |
| --- | --- |
| 网页保存触发的正常重启 | 不操作相机，60 秒内自动恢复取景 |
| OTA 后重启 | 同上 |
| 显示故障软重启 | 同上 |
| LCD 断电重启 | 不操作相机，120 秒内自动恢复取景；若相机机型确实不重连，记录为相机限制 |
| 相机先断电再开机（回归） | 仍能自动重连，不需要重新配对 |
| 未配对 / 换相机（回归） | 仍只连接有 DHCP 租约的单一候选，多候选仍提示断开多余设备 |

每个场景记录：复位到 AP 启动耗时、相机关联时间、DHCP 有无、发现到的 IP 来源、会话建立时间。结果写入 `docs/records/`。
