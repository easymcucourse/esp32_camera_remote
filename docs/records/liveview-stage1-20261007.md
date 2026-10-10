# 取景优化阶段1：Wi-Fi/lwIP优先PSRAM（2026-10-07）

状态：首版构建/烧录通过，启动内存门禁失败；修正版三档构建/UART发布通过，实机长测进行中。对照[阶段0基线](liveview-baseline-20261007.md)，来源为用户要求完整执行[优化计划](../design/liveview-memory-fps-plan.md)，最新要求使用UART烧录。本记录不把构建或短测当作完整验收。

## 配置与构建

仅修改sdkconfig.defaults，无固件C源码改变。SPIRAM_USE_MALLOC原已启用，显式保留；新增SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y，内部reserve固定32768、静态RX固定10、RX BA固定6，防止启用PSRAM时Kconfig默认漂移。TCP窗口/发送缓存5760、接收邮箱6/tcpip邮箱32、动态RX32、TX BA6、MSS1440保持基线。未开启窗口缩放和lwIP IRAM优化。

首版实际sdkconfig的完整差异比较发现另外两项SDK联动：动态TX不可选，强制静态TX16（约25.6KB内部DMA内存）与PSRAM TX缓存32；乱序pbuf上限从4默认变成0（不限量）。因此原计划“动态RX/TX都移到PSRAM、其他配置不变”的描述不准确。上述表述替代本记录首版中未完整核对的动态TX/乱序上限陈述。

用新的liveview-stage1 tag生成实际sdkconfig，三档逐项断言配置。Default/Stable/Release构建、20节点91边无环图、项目符号边界通过；Release禁模拟符号通过，三档均低于5MiB OTA预算。

| 档位 | 应用字节 | PSRAM时钟 | 符号边数 |
| --- | ---: | ---: | ---: |
| Default | 3510048 | 120MHz | 41 |
| Stable | 3506432 | 80MHz | 41 |
| Release | 3460816 | 120MHz | 37 |

首次Default编译的一个子进程失败且没有编译诊断；ninja -j4重试及完整CI复查通过，不归因于源码。Stable/Release首轮通过。日志build/liveview-stage1-{default,stable,release}-20261007.log、default-retry及default-check。sdkconfig/bin/ELF/MAP/size/门禁/hash快照在build/liveview-stage1-20261007/。最新Host264/264通过，包含日志分析7个用例；本阶段配置不影响Host源码。

## UART发布与实机门禁

烧录前status确认ota_0 valid、SIM0/settings0/session1。设备分区读回确认ota_0地址0x20000、容量6MiB；旧应用与系统区域已在本机ignored目录备份。先停止相机并排空，UART仅写ota_0应用，保留bootloader、分区表、NVS和otadata；3510048字节写入59.6秒、hash验证通过。日志build/liveview-stage1-uart-flash-20261007.log。

首版重启完整会话验证约24.85秒，随后取景正常；但累计min_internal=15031，比基线32463下降17432字节，free_internal约49647，largest_internal21504。累计min_psram1045748，largest_psram1048576。未达到内存目标，**首版不通过，不进入窗口阶段**。证据build/liveview-stage1-boot-20261007.log和stage1-r1-status。

修正版stage1-r2：静态TX16→4（SDK允许1..64），保留PSRAM TX缓存32；显式固定乱序pbuf上限4。首版UI初始化主任务栈最低余量14316字节，启动最高使用不足19KiB，因此主任务栈32→24KiB（FreeType局部池16KiB仍有额外余量），降低启动堆峰值；主任务返回后IDF会删除该栈，此项不虚报为运行期内存节省。该调整超出原计划的两行配置，依据是首版实测失败。三档fresh tag重建与门禁均通过，应用Default3510176/Stable3506560/Release3460944字节，实际配置与hash快照build/liveview-stage1-r2-20261007/。Default UART仅应用烧录/hash通过，启动UI主栈headroom6124字节，累计min_internal45067、largest_internal31744、累计min_psram1049096、largest_psram1048576字节；约50.6秒会话验证恢复。用户确认修正版画面正常、持续更新。日志stage1-r2-uart-flash/boot；COM8正在连续全屏1815秒，随后设置135秒并恢复全屏。12分钟临时平均4.801fps略低于基线，无incident行，不能提前宣称性能通过。全屏≥10分钟/设置≥2分钟/30分钟稳定性及Stable/维护/NVS回归待补。

阶段2窗口A/B/C、第三槽、异步LCD尚未发布。本阶段未做OTA上传；用户要求的固件发布使用UART，维护Web/NVS回归须与发布方式分别记录。

## 修正版30分钟结果与下一轮

r2完整全屏采样发生一次真实Stream退出：设备时间1855517ms，liveview backend=6（TIMEOUT），elapsed=5003ms，frame_failed=0/stop=0；帧租约排空后1858646ms恢复取景，约3.13秒。该事件没有NO_MEM/panic/0x200F/display_failure，不能归因于已知0x200F。它使本轮30分钟无Stream退出门禁失败，尚不能进入阶段2。完整JSON/日志为build/liveview-stage1-r2-full-summary-20261007.*，设置页仍在采集。

准备r3独立配置实验：staticTX4→6，其余r2配置不变。目的为比较较大的内部DMA TX池是否改善波动，并非认定4池导致上述超时。预计增加约3.2KiB内部占用；是否仍满足min_internal≥40000只能由重启实测判断。若内存/FPS/稳定性不通过则回退，不放宽门槛。三个新build tag构建中，未烧录。

## r2完整结果

Default全屏1815秒采集，有效FPS区间1803.230秒（跨重新建会话的区间排除），加权平均4.76130、最低窗口3.71、read抽样P50/P95/max171/214/945ms，display203/224/244ms，JPEG最大123224字节；最长连续会话1736.250秒。累计min_internal45067/min_psram1049096、largest_internal31744字节。相较Default基线4.83202fps下降约1.46%，且有一次5秒读取超时退出/自动恢复，r2性能与30分钟门禁均不通过。

设置页有效129.279秒，平均3.26788、最低3.14fps，read197.5/244/255ms、display305/306/306ms；无NO_MEM/panic/Stream退出/显示失败。对照基线3.27，基本持平；按原文严格“不低于现状”不声称存在正向收益。采样已恢复全屏。

r2 Stable3506560字节已UART仅应用烧录/hash通过，启动实测PSRAM80MHz，UI headroom6124，min_internal48587、largest_internal31744、min_psram1045784字节；用户确认画面正常持续更新，无花屏/撕裂。此PSRAM值超过十进制1MB但略低于1MiB（1048576），必须保留单位边界，不静默放宽门槛。session52586正在全屏615秒→设置135秒→恢复全屏，后续统计待完成。

## Stable r2结果与同期对照

Stable full有效609.135秒/平均4.4970/min窗口3.77fps；read抽样171.5/213/256ms、display203/254/255ms，JPEG41666..122989字节。Settings有效127.650秒/平均2.77565/min2.48fps，read206/222/243ms、display356/356/356ms。两页均无NO_MEM/panic/Stream退出/0x200F/显示失败；min_internal48587/min_psram1045784/largest_internal31744字节。用户确认Stable画面正常持续更新，视觉与短时稳定性通过，但没有原Stable实机FPS对照，不能宣称80MHz相对于自身基线无回归；也不满足平均5fps/最低4fps整体目标。

staticTX6的r3三档fresh build/graph/symbol/Release禁SIM通过；镜像Default3510176、Stable3506560、Release3460944字节，完整sdkconfig/hash/bin/elf/map快照build/liveview-stage1-r3-20261007/。Host264/264通过（日志分析现8个单元用例）。先UART重刷已读回验证等效的Default基线，采集同期615秒全屏对照（session55809），随后才测r3，避免只凭不同时段约1%的差异作归因。日志build/liveview-baseline-control-20261007-*。尚未发布r3，尚未进入阶段2。

## 21:14 用户说明与控制采样边界

用户明确“重启过相机，继续”。基线control镜像UART发布后相机在设备时间416797ms才重新关联、417054ms获得DHCP、419841ms会话验证；此恢复包含用户相机重启，不可作为LCD自动重连通过，亦不能由“420s恢复”推断纯自动重连最终成功。首个615秒control采集中有效取景不足10分钟，因此仅保留诊断，不作为完整全屏性能门禁。现设备已取景，另开无复位615秒clean全屏采样，日志build/liveview-baseline-control-clean-20261007.log；不跨缺失区间拼接加权FPS。

## 同期有效对照与r3发布

clean control有效608.474秒，平均4.7668147/min窗口4.32fps，read抽样P50/P95/max175/208/286ms、display203/223/248ms，JPEG最大134337字节；min_internal32459/min_psram1126008/largest_internal23552字节，所有故障计数为0。日志及JSON为build/liveview-baseline-control-clean-{20261007.log,summary-20261007.json}。比早期4.83202基线低约1.35%，说明场景/无线环境随时间存在波动；后续同时列出两个对照，不择取较低者宣称严格无回归。

r3 Default3510176字节已COM8 UART仅ota_0应用烧录/hash验证通过。boot实测min_internal41603、largest_internal31744、min_psram1047324字节，UI主任务栈headroom6124；22.579秒SESSION VERIFIED、22.596秒LIVEVIEW。此PSRAM高于十进制1MB、略低于1MiB，保留精确值。正在full1815秒→settings135秒→restore测试，session98790；完整结果未完成，尚未通过阶段1门禁。维护脚本仅准备并语法检查，尚未切PC网络或执行Web/NVS实机测试。

## 21:44 用户停止与保存

用户明确“今天停止，整理memory”，任务已暂停。session98790退出1（Capture full failed），全屏1815秒未完成，后续设置135秒未开始；日志保留。首个615秒前缀按预定长度截取，有效607.710秒，平均4.83191595/min4.47fps、read162.5/194/221ms，故障0；与早期基线4.83202均显示为4.83，但精确加权值仍略低，不据此宣称严格无回归或完整阶段通过。

完整已取得部分采样有效714.848秒，平均4.82250948/min4.47fps、read163/194/221ms、display203/203/239ms；min_internal41603/min_psram1047324/largest_internal31744字节。设备时间816601–818321ms记录5行0x200F，818328ms一次Stream退出（backend7，shown3831/dropped33/bad0/stop0），818352ms租约排空。无NO_MEM/panic/display_failure。此为已知0x200F问题的观察，既不混同r2 TIMEOUT，也不据此判定PSRAM配置造成回归；停止时没有捕获后续恢复，原因未知。证据build/liveview-stage1-r3-{first615-summary,paused-summary}-20261007.json及原full.log。

停止后COM8 status查询失败（端口不存在），pyserial仅列出COM11/COM101，COM8/COM6当前不可用；不能确认实时供电、画面或取景状态。最后烧录应用仍为r3 Default ota_0，最后有画面phase日志为settings0。本轮UART采集进程已结束，无本任务编译/烧录/采样进程残留。

独立managed worktree liveview-window的Default窗口A预编译已结束0，20节点91边无环/41符号边通过，镜像3510176字节。实际配置wnd16384/recv16/tcpip64/OOS8/dynamicRX32/staticRX10/RXBA16/staticTX6，PSRAM优先；Stable/Release尚未构建、未烧录/实测。第一次从错误cwd启动的主工作区window-a-draft编译被中断，可能留下使用root r3配置的半成品，不能用于窗口A证据或烧录。维护/NVS脚本未执行，无OTA镜像上传、无PC网络切换、无提交/推送。三个managed worktree保留以供下次接续，重连与帧槽/异步代码尚未整合。
