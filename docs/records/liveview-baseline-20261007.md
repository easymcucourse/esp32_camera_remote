# 取景优化阶段 0：基线（2026-10-07）

状态：进行中。来源为用户要求完整执行 [优化计划](../design/liveview-memory-fps-plan.md)；工作起点 668d84e，跟踪工作区最初 clean。本记录区分新构建与设备现有镜像，不以编译通过代替实机验收。

## 软件基线

本机 ESP-IDF 5.5.1，调用 tools/ci_build.py 的 LCD debug / debug --profile stable / release，完成实际构建、component graph 与静态库符号边界；Release 禁模拟符号检查通过。三档应用均低于 5 MiB OTA 限额。既有 sdkconfig 的实际值逐项核对并归档，不只读取 defaults。

| 字节 | Default | Stable | Release |
| --- | ---: | ---: | ---: |
| 应用 bin | 3509952 | 3506352 | 3460720 |
| IRAM vectors + text | 16384 | 16384 | 16384 |
| D/IRAM data | 23044 | 22096 | 22596 |
| D/IRAM bss | 51336 | 51328 | 45320 |
| D/IRAM text | 94367 | 91731 | 93463 |
| D/IRAM 合计 | 168747 | 165155 | 161379 |
| XIP instruction linker span | 1159888 | 1159888 | 1116824 |
| XIP rodata linker span | 2216128 | 2216104 | 2211312 |

ESP32-S3 的静态 D/IRAM 由 size 工具合并报告；表中 XIP 为链接器保留区跨度，不等同于运行时 PSRAM heap 或映射页对齐后的保留量。size/size-components 使用与 idf.py 相同的 esp_idf_size 模块从新 MAP 生成。

三档共同实际配置：

| 配置（省略 CONFIG_） | 值 |
| --- | --- |
| ESP_WIFI_STATIC_RX_BUFFER_NUM | 10 |
| ESP_WIFI_DYNAMIC_RX_BUFFER_NUM / DYNAMIC_TX_BUFFER_NUM | 32 / 32 |
| ESP_WIFI_RX_BA_WIN / TX_BA_WIN | 6 / 6 |
| LWIP_TCP_WND_DEFAULT / TCP_SND_BUF_DEFAULT | 5760 / 5760 |
| LWIP_TCP_RECVMBOX_SIZE / TCPIP_RECVMBOX_SIZE | 6 / 32 |
| LWIP_TCP_MSS / TCP_OOSEQ_MAX_PBUFS | 1440 / 4 |
| LWIP_WND_SCALE | 未启用 |
| SPIRAM_USE_MALLOC | y |
| SPIRAM_MALLOC_ALWAYSINTERNAL / MALLOC_RESERVE_INTERNAL | 16384 / 32768 |
| SPIRAM_TRY_ALLOCATE_WIFI_LWIP | 未启用 |
| SPIRAM_FETCH_INSTRUCTIONS / SPIRAM_RODATA | y / y |

Default/Release 为 120 MHz PSRAM 配置，Stable 为 80 MHz。构建日志 build/liveview-baseline-{default,stable,release}-20261007.log；完整 sdkconfig、size-summary、size-components、图与符号门禁结果、bin hash 快照在 build/liveview-baseline-20261007/。未经重新烧录的构建不代表设备当前镜像。

原有 Host 263/263 通过；新增日志分析工具的七个用例通过，注册后全套 Host 264/264 通过。最终日志 build/liveview-stage0-final-host-test-20261007.log。

## 实机采样

起初 Win32_SerialPort 只列出 COM11/COM101；后续 pyserial 实际枚举到 LCD COM8 / ATOM COM6。LCD preflight 显示 session1 / settings0 / SIM0 / display_failed0，真实 ATOM 和 DS4 在线，运行 ota_0、state valid，相机非录像且回读已知。用户确认当前画面正常、持续更新；该确认只证明当前观测，不证明历史显示异常根因已修复。

初始短窗口：4.91 FPS，read 约 139–151 ms，display 203 ms；min_internal=32463、min_psram=1125840、largest_internal=23552 字节。原计划的“PSRAM 约 2 MB”估计与此相差较大，第三个 512 KiB 槽不能直接假定满足 1 MiB 余量门禁。

第一次 615 秒采样已完成，120 个完整窗口连续覆盖 612.342 秒，无跨会话或复位；最大报告间隔 5.344 秒。新增页面一致性检查发现 120 个 settings0 分项和 1 个 settings1 分项（设备时间729053ms），**不是合格的纯全屏基线**。下表只保留其混合页面参考值，不用于阶段间全屏对比。日志 build/liveview-baseline-full-20261007.log 与完整 summary JSON。未重启相机或更改拍摄参数；设备已有固件来源仅有前一日烧录记录与行为吻合，尚未读回 hash 作本轮镜像比对。

| 首次混合页面参考指标（非全屏验收） | 结果 |
| --- | ---: |
| 按时间加权平均 FPS / 帧数差分 FPS | 4.85675 / 4.85840 |
| 最低报告窗口 FPS | 4.32 |
| 抽样 read P50 / P95 / 最大 ms | 158 / 206 / 259 |
| 抽样 display P50 / P95 / 最大 ms | 203 / 203 / 305 |
| 抽样 JPEG P50 / P95 / 最大字节 | 102308 / 113596 / 115570 |
| free_internal 观察最低 / 累计 min_internal 字节 | 59979 / 32463 |
| largest_internal 最低字节 | 23552 |
| 累计 min_psram 最低字节 | 1121964 |
| 抽样 decode P50 / P95 微秒 | 122789.5 / 130667 |
| 抽样 publish P50 / P95 微秒 | 72277.5 / 94119 |
| 可见 NO_MEM / panic / Stream 退出 / 显示失败行 | 0 / 0 / 0 / 0 |

参考平均 FPS、read P50 和累计内部堆低水位未达到最终目标。没有记录到 0x200F 行；此处只表示日志没有可见行，并不证明相机未发送过被当前 backend 消化的临时拒绝响应。第一次135秒设置页采样也在约35秒后回到全屏，用户确认期间操作过并承诺后续保持不动；该段同样不用于设置页基线。新的纯设置页135秒采样 build/liveview-baseline-settings-clean-20261007.log 正在进行，随后重采纯全屏615秒。停止/恢复与重启诊断待执行。

纯设置页采样已完成：25个完整窗口覆盖129.787秒，26个JPEG分项全部settings1；加权FPS3.27、最低3.27，帧数差分3.27460；read抽样P50/P95/最大178/198/201ms，display305/306/306ms，JPEG最大109470字节。未观察到断流/NO_MEM/panic/显示失败，满足设置页≥2.4的基线要求。

纯全屏重采完成：120个完整窗口覆盖611.986秒，121个JPEG分项全部settings0；加权FPS4.83202、帧数差分4.83344、最低窗口4.43，最大报告间隔5.344秒；read抽样P50/P95/最大163/198/237ms，display203/203/237ms，JPEG106835/118245/122217字节。free_internal观察最低59783、累计min_internal32463、largest_internal23552，累计min_psram1121964、largest_psram1114112字节。无可见断流/NO_MEM/panic/显示失败行。日志 build/liveview-baseline-full-clean-20261007.log，页面门禁通过，完整统计见同名前缀summary JSON。基线平均FPS与内部堆低水位未达到最终目标。

停止命令后在途读取取消，9591帧已显示租约排空并退出任务；重新启动完成身份验证，随后121帧、4.86fps。日志 build/liveview-baseline-{stop,resume}-20261007.log。取消后不盲目发送CloseSession，正常协议边界才允许该操作。

第一次UART硬复位（非断电）没有操作相机：AP/DHCP服务约2508ms启动、相机3963ms关联、35502ms主动离开、42837ms再次关联、43095ms取得DHCP地址、45843ms完整会话验证、45853ms开始取景。125秒观察中后续取景持续；此次恢复满足60秒，但不能代替正常网页/OTA/断电三类场景。日志 build/liveview-baseline-reset-20261007.log，未复现永久关联/租约/会话故障。

UART版本报告01d6b4e-dirty、Oct 5 2026 19:10:18，新基线构建project_version为668d84e。版本字符串由增量配置保留，不能仅凭它判定运行源码。已读回ota_0的3509952字节，和新基线bin只有80字节不同，全部位于app descriptor的版本/时间/日期/ELF摘要，以及尾部镜像校验/摘要；六个段的实际载荷（首段排除256字节app descriptor）逐字节相同，其他descriptor字段也相同。因此上述实机基线对应相同代码/数据/配置，避免不必要的重复烧录。比对JSON、原应用和系统区域备份保存在build/liveview-baseline-20261007/（ignored）。该目录含本机NVS数据，不能发布或提交。频道6、默认密码标志1，Wi-Fi查询有一个DHCP客户端。

工具 tools/analyze_liveview.py 对 IDF 时间戳计算加权 FPS；第一次短报告和跨复位/断流区间排除，报告最长连续会话、日志间隔、heap 最低值和错误行。read/display/JPEG 每 5 秒仅取最后一帧，所以 P50/P95 为日志抽样分位数，不能表述为所有帧分位数。heap API 低水位自启动累计，不能误称仅本轮取景最低值。全屏与设置页须用独立日志统计。

## 推进门禁

阶段0完整实机采样及相同代码/配置载荷读回核验已完成。阶段1仅配置已应用并开始独立tag三档构建，未烧录；阶段2 A/B/C、第三帧槽、异步LCD发布及最终30分钟稳定性未开始。重连诊断继续按实际场景验证，不以一次硬复位通过替代全部场景。

本机 Kconfig 已验证：SPIRAM_TRY_ALLOCATE_WIFI_LWIP 改变静态 RX 默认 10→16、RX BA 默认 6→16。阶段 1 保留静态 RX=10、RX BA=6，并保留内部预留池 32768；阶段 2 才按窗口档位上调。lwIP mem_clib_malloc/calloc 是 PSRAM 优先、内部 RAM 后备，因此不能描述为绝不占内部 RAM。
