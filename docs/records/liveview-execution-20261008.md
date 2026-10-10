# 取景完整计划续测（2026-10-08）

本记录区分配置、编译、UART发布、实机功能、性能和稳定性；完整计划尚未通过。10月7日全部证据保留，今日采样不覆盖原日志。用户继续执行目标，发布仍用UART，没有提交/推送请求。

## 重新核对与r3

HEAD668d84e，root固件C仍基线，配置TX6/PSRAM优先。10月8日枚举COM8与COM6可用，COM8 status确认ota_0 valid、session1/settings0/SIM0/display_failed0、ATOM与DS4在线；用户新确认画面正常持续更新，采样保持场景/模式/页面不动。证据build/liveview-resume-status-20261008.log。

UART先s停止排空，重刷r3 Default3510176字节，仅ota_0应用0x20000，59.6秒/hash通过，NVS/otadata保留。日志build/liveview-stage1-r3-resume-20261008-uart-flash.log。此次boot AP2.508秒，但95秒时仍session0，后续wifi show DHCP客户端0；旧接口不能区分“未关联”和“已关联但无DHCP”，不可据此确定根因A/B。原1815秒计时脚本已中止（exec85833 exit1），该段是等待/重连诊断，不作30min性能样本。

新guard先等SESSION VERIFIED，再启动full1815秒采样。设备286.865秒获得DHCP、289.594秒会话验证；已询问用户是否重新选择热点或重启相机，回答尚未收到，不能宣称纯自动恢复。采样exec61913进行中，前缀build/liveview-stage1-r3-live-20261008；结束后自动settings135秒并恢复全屏。今日实际性能/稳定性仍待终态。

## 窗口A编译准备（未发布）

managed liveview-window独立工作区，固件C保持基线。Default/Stable/Release全部build/20节点91边无环/项目符号门禁通过，Release禁SIM通过。实际wnd16384/recv16/tcpip64/OOS8/dynamicRX32/staticRX10/RXBA16/staticTX6/cacheTX32/reserve32768，发送窗口5760、不启用窗口缩放。这只是阶段2编译准备，不表明阶段1通过或窗口实机有效。

| 档位 | 应用字节 | SHA256 |
| --- | ---: | --- |
| Default | 3510176 | 3c0c2f7c2ea3bea7be63b74dee2bc2f53e541ed855a25388b8b955ac20e71ee6 |
| Stable | 3506560 | 3e7e82e07465d782d43fb24e5a452e00dc64a0fac72a7f62a0028f5d8689edd3 |
| Release | 3460928 | b5c6812fc253cd15199e6380263e0372277e0086abcecc6b50c822a9224afea8 |

sdkconfig/bin/elf/map/size/graph/symbol完整快照：该工作区build/window-a-snapshot-20261008/及builds.json。Stable/Release今日日志window-a-{stable,release}-build-20261008.log；Default为10月7日window-a-default-build.log。Root错误cwd生成的window-a-draft半成品继续排除。

## 三槽内存调查（未改资源）

当前第三个512KiB槽将最低PSRAM从约1.05MB减至约0.52MB，低于计划至少1MB。没有静默缩小槽容量或字符集。Chinese font当前1878360字节/7621字符，XIP进入PSRAM；调查使用隔离build/font-eval-20261008环境，不改变工程日常构建依赖。

标准DEFLATE分块无损压缩仅省178052–229099字节，还需运行时解压与scratch内存；不足以补齐第三个512KiB槽。使用[Adobe cffsubr](https://github.com/adobe-type-tools/cffsubr)重排CFF子程序，候选1834948字节，省43412字节；cmap/字序/水平度量相同。后来用Host FreeType2.13.2逐字比较7621字符×10字号（8/12/16/17/18/24/32/40/48/96），76210次灰度像素、位置和advance均一致；固件FreeType2.14.3和LCD视觉尚未验证。因节省不足，不采用。原字体和覆盖范围保持不变。证据pipeline工作区build/font-subr-verification-20261008.{log,json}。

基础异步LCD+count2三档fresh编译已通过（pipeline工作区tag async-r3-prep、exec35200 exit0），scan-core关闭，实际IPC栈1280字节；镜像3510656/3507056/3461408字节。完整快照build/async-r3-snapshot-20261008/。尚未发布或实机证明收益。此工作区defaults只是编译候选，不能复制到root作为阶段1/2配置。后续需要先验证基础异步，再独立评估scan-core提交。

19:27进一步修正pipeline的结果队列：按实际开机槽数分配2×slot_count（两槽4项、三槽6项），而非默认两槽也分配最大三槽的6项。这保留原两槽内部RAM占用，避免预备三槽API白白增加两个app_message_t；具体字节/实机低水位仍须测量。新增普通/Debug两种三槽endpoint变体，用真实生产handler验证owner阻塞网络IO时能保留全部槽的完成metadata并按FIFO提取。完整Host270/270通过（build/pipeline-host-test-20261008.log）；三档增量重新编译/边界门禁exec13545进行中。上述首次快照仍是固定6项旧候选，后续应用须用更新快照，不混作最终队列版本。

## 未完成门禁

- r3连续30min与设置2min、严格基线对比、Stable实机和维护/NVS回归。
- 窗口A逐档实机10min/设置2min/停止恢复/维护切换，必要时合法OOS范围内评估B/C。
- 第三槽内存预算与实机生命周期，独立异步LCD性能/视觉/恢复验收。
- 重连草稿整合及Web重启60s、显示故障重启60s、物理LCD断电120s、相机断开重连；ARP/saved-IP/完整GUID守卫。
- 最终平均FPS≥5、最低5秒窗口≥4、read抽样P50≤150/P95≤300ms、min_internal≥40KB且不低于基线、largest≥16KB、min_psram≥1MB、连续30min无NO_MEM/Stream退出。具体字节值保留，KB/MB与KiB/MiB分别列明；当前未证明完整达标。
- OTA全镜像上传尚未做。用户指定UART发布，维护Web/NVS和OTA头预检需分别记录，不以预检替代真实上传证据。

## 停止时最终证据（2026-10-08T19:39:34+09:00）

依用户停止要求结束主机采样，goal已paused；exec61913终态exit-1，settings135阶段未开始，也没有后续烧录/维护/网络切换。停止时仍全屏，仅终止主机脚本和串口采集，没有发送停止取景或复位。本记录前文“进行中”指当时状态，以本节终态为准。最新用户确认LCD正常、持续更新；停止之后的实时设备状态需重查。

页一致性检查通过，settings0 phase329条、settings1为0。实际有效全屏1682.095秒（约28.04分钟），加权平均4.72355166FPS、帧计数平均4.72387112、最低5秒窗口3.66、最长报告间隔5.446秒；read稀疏抽样P50/P95/max181/233/492ms、display203/236/244ms、JPEG中位116500/最大128495字节。min_internal41603/largest_internal31744/min_psram1049068字节；后者比1MiB高492字节。NO_MEM、panic、Stream退出、0x200F、display_failure日志计数均0。证据build/liveview-stage1-r3-live-20261008-full.log及paused-summary.json。

该段不足连续30分钟，设置页未测；平均FPS、最低窗口与read P50未达最终门禁，也低于原版与clean control平均值，不能判阶段1完整通过。历史r2 TIMEOUT/r3昨日0x200F证据保持独立，不能拼接多个日期补满30分钟。相机本次恢复是否人工操作仍未知。

pipeline最终队列版三档exec13545已exit0，build/graph/symbol门禁及Release禁SIM通过；Host270/270已终态通过。正确完整快照为该工作区build/async-r3-queue-snapshot-20261008/，包含对应源码SHA。配置count2/basic-async开启/scan-core关闭/IPC栈1280；下列镜像尚未烧录，大小与旧版相同不代表内容相同。

- ci-lcd-debug-async-r3-prep: 3510656字节，SHA256 `f20c23b29a894e1262bf83814bb71c28df078dcf94c223e799d6ae1315cad8c7`。
- ci-lcd-debug-stable-async-r3-prep: 3507056字节，SHA256 `de0584efcac037e632dbf3ea5cb52833d4a67b5da1bab404fd6f6d0d2875f609`。
- ci-lcd-release-async-r3-prep: 3461408字节，SHA256 `16786d2b9bdab7691f277d2825569f0bef005afc18bf45ffef295b98ba558aed`。

所有源码、工作区和本机证据保留，未提交或推送。完整计划仍未完成；memory/current-state.md与会话记录保存继续入口。
# 停止交接后的继续执行

随后新的完整目标继续消息恢复任务，get_goal active；前文暂停终态仍作为当时事实保留。重新核对root源码/git与COM8 status session1/settings0/SIM0/display_failed0，最近4.91FPS。在同一r3固件上启动新的连续采样exec55180，前缀build/liveview-stage1-r3-round2-20261008；full1815→settings135→全屏→停止25→重新开始90，不拼接旧28min证明30min。

隔离重连工作区新增独立默认关闭受控重启前deauthentication比较开关：仅维护关闭与normal drain成功后发送所有AP站点断开通知，保留AP/配置/NVS，存在活跃channel或生命周期锁冲突拒绝。Host263/263通过，三档候选编译exec16850进行中，未实机发布。维护helper加强UART配置快照，使用当前配置SSID且验证默认密码仍在用之后才切PC网络；不请求真实密码。Helper仍未实测。
# 新候选编译终态

重连工作区fresh reconnect-deauth三档全部build/graph/symbol/Release禁SIM通过，exec16850 exit0；Host263/263及模块边界通过，完整对应源码快照build/reconnect-deauth-snapshot-20261008。镜像3513136/3509536/3463936字节，实际重启前deauth开关y；未烧录。默认开关仍关闭，必须实机比较后再决定最终配置。

pipeline额外three-flash-rodata候选也三档通过，exec12651 exit0。count3、每槽512KiB，async/scan-core关闭、SPIRAM_RODATA关闭而SPIRAM_FETCH_INSTRUCTIONS保持y；应用3509856/3506256/3460624字节。快照build/three-flash-rodata-snapshot-20261008；Default静态DIRAM168647/rodata2215760字节，不等于运行时回收量。字体覆盖、图像容量、LCD时钟/bounce均保留；编译后finally恢复原候选defaults。此方案尚未采用或烧录，Flash写入期间扫描、WiFi、配对NVS及FPS必须实测。原SDK同时搬指令和rodata的Flash期间外存访问保障发生变化，不能仅因空间估计足够而宣布三槽通过。

完整门禁索引见[验收索引](liveview-acceptance-checklist-20261008.md)。r3 round2仍在独占COM8采样，不切PC网络或同时打开串口。

## 最终收尾（2026-10-08T20:23:44+09:00）

用户目标改为“停止目标，记录结果并结束”；原完整优化计划未完成，停止后续测试/实施，保留未提交工作区与原始证据。后台进程复查无本任务残留。前文进行中状态为历史，以本节终态为准。

- r3 full：有效/最长连续1807.454s，平均4.76081452FPS，最低5秒窗口3.34；read稀疏抽样P50/P95/max171/214/350ms，display203/233/243ms，JPEG中位114684/最大127469B。NO_MEM/panic/Stream退出/0x200F/display_failure均0，页门禁settings0通过。
- 内存：min_internal41603、largest_internal31744、min_psram1049068字节，达到计划内存门槛。30min无错误是本r3样本结论；平均FPS低于4.83202原基线，最终平均≥5、最低窗口≥4、read P50≤150均未通过，不能宣称阶段1或完整优化目标全部通过。
- settings：有效128.769s、平均3.67226071/min3.27FPS，read169.5/199/202ms、display255/306/306ms，incident均0，settings1页门禁通过，随后恢复全屏。
- stop/restart：leases drained与Camera task finished观察=True；j后SESSION VERIFIED观察=True；后续维护采集复位前取景帧观察=False。来源round2-stop-drain/restart-live日志及maint UART，原采样exec55180 exit0。
- 维护回归：exec10884 exit1，在首次访问LCD HTTP前出现Windows网络不可达WinError10051；来源build/liveview-stage1-r3-maint-20261008.json与.uart.log。未到设置保存/NVS读回/OTA预检，未上传固件。temporary_setting_still_needs_restore=false、PC原WiFi恢复=true、临时profile删除=true。根因未诊断，不能把网络不可达定性为固件HTTP故障或DHCP故障。维护复位后实时相机/显示状态未知。


重连草稿新增实际SDK AP start/stop事件日志，首次Host缺fake_log链接失败已修正，最终Host263/263及三档build/graph/symbol/Release禁SIM通过。最新应用3513232/3509632/3464032字节，完整快照为重连工作区build/reconnect-ap-event-snapshot-20261008；前文reconnect-deauth-snapshot是旧源码。未发布新候选。

没有继续诊断网络不可达，也没有实施新的PC路由/DHCP修改。窗口A顺序选择未收到回答，随停止失效。当前设备应用仍最后UART r3；维护过程中RTS复位，最后实时相机/显示状态未重新查询。PC原网络与临时profile清理均有脚本结果证明，无临时显示设置待恢复。未提交、推送或归档工作区。
