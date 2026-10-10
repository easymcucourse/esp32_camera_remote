# RS 3 Mini 接入记录 — 2026-10-10

用户目标：增加 RS 3 Mini 控制。用户确认云台可开机、没有协议资料；随后明确允许现在烧录测试，云台已准备好。此次允许 ATOM 运动控制，取代过去其他任务对云台的排除；没有提交、推送或 LCD 烧录请求。

## 软件与主机

实现和未实现项见 [协议与当前设计](../design/rs3-mini-protocol.md)。加入本地 DS4 控制、BLE 扫描协调、动态 GATT、NVS 目标/偏好、I²C 状态和云台电量。真实运动只消费 Classic DS4；当前采用云台原生零位，软限位未实现。

ESP-IDF 5.5.1，独立 `build/rs3-mini-atom` 从 ATOM defaults 生成 BTDM/BLE/GATTC 配置，BLE 连接数 3。本机旧 `m5_atom_matrix/sdkconfig` 是 BR/EDR Only，旧目录首次构建正确被 BLE 检查拒绝；未把旧配置误记为新固件配置。首次独立构建的相对 -B 路径有误，后来用绝对路径构建成功；最终位置见下文。

- `cmake -S tests/host -B build/host`、`cmake --build build/host -j 4`、`ctest --test-dir build/host --output-on-failure`：266/266 通过，包括 gimbal 与 ble_clients。协议测试使用独立抓包字节核对中立/运动 CRC 与字段；涵盖帧校验/重组、控制门禁、曲线、停止、回中超时、时钟 wrap、扫描互斥和失败释放。
- 独立 IDF 最终构建成功：应用 `0x106ba0`（1,076,128 字节），factory 分区 `0x177000`，余量 30%。应用 SHA256 `5dcc6ce9a75ad7b5f443fca4468bf6656fc45f23cb4881d765d260afde997807`。
- 编译、主机测试不证明硬件停止时延、方向或长期稳定性。

## 烧录与设备观察

COM6 返回 ATOM 的 status；esptool 识别 ESP32-PICO-D4。先读取 `0x8000/0x1000` 分区表并与构建产物逐字节比较，一致。随后 115200 波特率仅写应用 `0x10000`，esptool hash verified。未擦除 NVS、未写 bootloader 或 partition table。重启后实际加载 factory `0x10000`、ESP-IDF 5.5.1；保存的 DS4 配对信息仍可读取。

- 开机约 4.2 秒发现单台 Mini；约 7.5 秒日志 `RS3 Mini control ready`，表示 GATT/CCCD/有效 DUML/中立提交门禁已通过。
- 后续 UART status：gimbal=3、faults=0、gimbal battery=73，free_internal=117464 字节、SIM=0。云台电量尚未与机身人工核对。
- LCD link=0，不参与此云台连接；这是链路独立性的观察，不等于 LCD 断线时手柄运动已验收。
- 同一窗口 DS4 尚未 input ready，出现保存手柄连接超时与搜索。已请用户唤醒 DS4 并验证四方向、松杆停止与 L3。未用模拟输入制造运动结论。

## 未完成

真实 DS4 运动、实际方向、松杆停止≤100ms、L3 原生回中与取消、手柄断开安全停止、云台重启自动重连、NVS 断电恢复、Matrix/LCD 视觉及 30 分钟并发稳定性待验证。原完整需求中的任意零位、软限位和板载设置菜单未实现。

## 增量验证与失败 — 同日

- 新增 `gimbal_tx` 在途写入门禁、500ms完成超时与保留停止槽；完整Host回归 **267/267通过**，日志 `build/rs3-mini-tx-host-test.log`。修复重复on误发布Searching。
- 中间镜像`0x106fc0`已仅写应用且hash验证；下一镜像`0x107030`修复BLE HID误消费云台的断开事件，同样构建/烧录通过。两轮关闭/on重连脚本通过，恢复分别约9.52秒、8.36秒；第一轮15秒检查超时的失败日志保留，后来观察到自行恢复，不能把该轮短窗口记为通过。
- `gimbal off`后RTS重启，UART读取`state=0 enabled=0`；再on约26.28秒恢复Connected。证明启用开关跨ATOM软件重启保持；实际断电仍未测试。默认速度/方向/漂移偏移的独立持久化未测试。
- 真实DS4后来上线：`ds4=1 armed=1`、报告年龄1–62ms，记录四方向输入；运动提交达到176次，`center_queued=0`。云台与DS4短时同时在线、fault0。**用户随后明确反馈“云台开启，没有动作”**，因此物理运动未通过，不能用提交计数或电量判定成功。
- 仅姿态debug采样观察到`04/66`每秒约一帧，31字节TLV，角度候选tag22/23/24各两字节；`04/10`同名tag可能一字节，不能混用。采样发生在推杆后，缺少同步运动标定，未将数据用于软限位。
- 电量清除镜像`0x107050`构建/烧录hash通过；最后重启后曾持续Searching、rx0，随后fault1。off状态实测云台电量255（未知），再on恢复enabled1。设备仍开机由用户确认；当时没有恢复控制链路。
- 后续源码发现与Mini参考会话的差异：缺少向FFF4特征本身写`01 00`（已有2902写入不能代替该步骤）；另遗漏GATT CLOSE终态。现补会话特征属性/完成校验、300ms等待、CLOSE事件和按目标peer断开物理链路，并增加无设备身份的phase/扫描计数诊断。会话修正镜像构建通过；其烧录与实机结果见后续更新。

命令脚本 `tools/uart_scripts/atom-gimbal-link.uart`检查重复on、非法值拒绝、off/on恢复与关闭时电量未知。只测试栈/协议状态，不生成模拟运动；执行条件为真实DS4离线。

15:30更新：会话修正镜像`0x107330`（1,078,064字节），SHA256`c97714a4da2d85df03c02091e1cb9b5deb81018e9c93bea26d4a9b691ebd8b26`，COM6仅应用烧录hash verified。启动后诊断`state=1 enabled=1 fault=0 phase=0x03 scan_named=0 scan_saved=0 rx_frames=0 ds4=0`：正在扫描，没有收到Mini名称匹配的广播，尚未执行新握手。已请用户将云台关机再开机并关闭Ronin App，以排除残留连接。当前不能宣称最新运行镜像已经连接或运动通过。新证据`build/rs3-mini-atom-session-init-{build,flash,boot}.log`、`build/rs3-mini-session-init-status.log`。

15:40增量：Windows独立BLE主动扫描20秒，收到27个广播、6台设备，名称数据为0，无可识别RS/Ronin或FFF0候选。此结果与ATOM未匹配云台一致，但没有名称、没有已知目标身份比对，不能单独证明云台没有广播。扫描只读、未连接任何设备；Bleak3.0.2及WinRT依赖仅安装到忽略的`build/rs3-mini-python-deps`，未修改主机蓝牙设置。证据`build/rs3-mini-independent-scan.{json,log}`。

发现原先保存目标重连仍要求该包带Mini名称，可能遗漏无名称/无扫描响应的广播。已修正为保存地址匹配；首次配对仍检查名称，且另一台Mini不得替换保存设备。新增回归验证已保存无名设备、不同地址同名设备、未保存无名设备与空指针边界；相关Host **3/3通过**（gimbal/ble_clients/gimbal_tx），完整267/267为前一阶段基线。独立IDF构建成功；应用`0x107460`（1,078,368字节），SHA256`65694852949b090678305376f6502a60daa7e29a5dd2eed46cf321eb439c693d`，COM6 app-only烧录hash通过。首20秒仍未control ready，后续观察结果另记；用户云台重启确认仍待收。

15:42:42最后读取：state1/enabled1/fault0、phase01（扫描间隔）、scan_named0/scan_saved0/rx0/ds40，后续30秒窗口也无control ready。保存地址与名称都未匹配，最新握手仍未执行，运动没有通过证据。相关采集已结束并释放COM6。

15:44:49再次复查仍state1/enabled1/fault0、phase03、scan_named0/scan_saved0/rx0/ds40。目标设备未匹配、没有新鲜DS4，三连续goal轮次同一阻碍持续，已将目标标记为blocked等待外部设备恢复。全目标未完成：既有“没有动作”反馈仍有效，最新握手待实测，任意零位/软限位与全部物理验收仍保留。COM6释放；源码和烧录镜像仍为上段0x107460，未继续改变控制行为。证据`build/rs3-mini-blocked-audit-status.log`。

## 19:48–19:53 用户重启后复查

用户回复“已重启”，恢复本任务诊断。重启前 ATOM 连续运行约四小时；第一次 status 仍 Searching、enabled1/fault0、scan_named0/scan_saved0/rx0、真实 DS4 离线。随后仅 RTS 重启 ATOM，未重新烧录或清除配置；启动版本仍为上述 `0x107460`。

Windows 独立主动扫描20秒收到17包/6设备，named0、相关候选0，仍不能据此证明目标未广播。ATOM 的 BLE HID 扫描连续收到11–17包，说明蓝牙扫描能收到周围广播；编号 UART 观察脚本4条命令成功，三次云台 status 均 state1/armed0/ds40/rx0/scan_named0/scan_saved0，最终 phase03。普通 `status` 的 ds4=1 是 Searching 状态枚举，不能误解成真实手柄在线。

首次重启时立即发送的无编号命令未收到确认，后一次无编号命令返回 input line invalid；以上状态以随后编号脚本成功响应为准。已请用户 PS 唤醒 DS4，并把云台放到 ATOM 旁边、通过 Ronin App 搜索（先不连接）确认是否可发现。云台触屏蓝牙图标用于相机快门连接，不能作为 Ronin 控制链路的判断依据。等待这一外部信息；本轮未修改运动幅度、未发出运动指令，新握手及物理运动仍未验证。所有本轮串口/扫描进程已结束。

证据：`build/rs3-mini-after-user-restart-status.log`、`build/rs3-mini-user-restart-atom-reset.log`、`build/rs3-mini-restart-independent-scan.log`、`build/rs3-mini-restart-observe.{uart,log}`。独立扫描 JSON 的共享路径已被本轮结果覆盖，历史27包结果由此前日志保留。

## 20:06 DS4 恢复及 LCD 故障显示补齐

续轮直接读取设备：真实 DS4 已恢复，末次报告年龄14ms、电量档6、SIM0；云台仍 Searching/armed0/rx0/scan_named0/scan_saved0、phase03。ATOM BLE HID 扫描约99–121包，Windows独立20秒扫描30包/7设备，仍没有名称或可识别相关候选。手柄不可用这一项已被新证据取代；目前等待 Ronin 仅搜索的结果，以定位云台不可识别的原因。未改变 ATOM 镜像、幅度或发出运动指令。

需求审计发现 R4.3 的故障 bit3 虽由 ATOM 上报，LCD 输入 provider 先前丢弃它。现补 `gimbal_fault` 从 I²C provider→input report→typed state→UI model；连接页红色 Fault、取景/设置页红色 GIMBAL FAULT，普通信息隐藏时仍保留。健康状态清除；ATOM离线/重启/协议不匹配、UI关闭/冻结清除旧提示。其余故障位不触发云台提示。未改变 I²C 版本/长度或云台控制算法。

- 生产 I²C provider 注入含正确 CRC 的 bit3，再换成其他故障位，验证设置/恢复；input owner/service/UI message/model 覆盖传播、恢复、离线清除、冻结后晚到状态拒绝。相关 **5/5通过**。
- 全量 Host **267/267通过**，24.85秒，日志 `build/rs3-mini-lcd-fault-host-full-test.log`。
- LCD IDF最终构建通过，应用 `0x359020`（3,510,304字节），SHA256 `a3e0481fb23cac38141a69cffc3399ed7eb87ecae1c02305d8703e343448246f`，日志 `build/rs3-mini-lcd-fault-idf-build-final.log`。没有 LCD 烧录授权，本轮只构建，实际显示待验。
- `git diff --check`通过。本轮相机/取景目标没有恢复，未提交/推送。

证据：`build/rs3-mini-resumed-turn2-observe.log`、`build/rs3-mini-ds4-online-independent-scan.log`、`build/rs3-mini-lcd-fault-{host-build,host-test,host-full-test,idf-build,final-device-status}.log`。

## 20:09 恢复后的阻塞审计

第三个恢复轮次重新读取源码、工作区状态和完成表，再执行编号观察4条命令，正常结束并释放COM6。真实DS4仍在线、末次输入年龄26ms/SIM0；云台state1/enabled1/fault0/armed0/rx0/scan_named0/scan_saved0，末次phase01（扫描间隔），BLE HID扫描收到95包。最新握手未执行，物理运动失败的用户反馈仍未被新证据取代。

前两恢复轮次分别完成重启/独立扫描诊断，以及DS4上线/LCD故障提示补齐；本轮无法在缺少实际云台链路的情况下验证运动、姿态单位及零位/软限位。软件修正和现有主机检查已完成，Ronin仅搜索结果未收到，同一云台不可识别条件持续三轮。`update_goal`返回blocked，完整目标未完成。等待外部设备可发现性信息后继续；未重新烧录、未删除绑定、未修改运动幅度、未发运动或重复全量测试。

本轮证据 `build/rs3-mini-resumed-turn3-audit.{log,console.log}`；采集进程已结束，无本任务后台采集/构建/烧录进程。既有267/267和LCD构建证据保持其原范围，不作为实机控制通过证明。

20:25追加：新的goal continuation曾重新激活目标；20:22/20:24/20:25三个fresh连续轮复查同一云台不可识别，末次真实DS4输入年龄12ms、云台state1/phase01/rx0/scan_named0/scan_saved0。没有新的Ronin搜索回复，不能继续实机控制或姿态标定；目标再次返回blocked，完整目标未完成。只读观察均终止/COM6释放，本轮没有代码/配置/烧录/运动变更。证据`build/rs3-mini-reactivated-status.log`、`build/rs3-mini-reactivated-turn{2,3}-status.log`。

## 20:29–20:41 重新配对、通知特征修正及物理失败

用户明确重启云台并请求重新配对。`gimbal pair` 后已识别目标，旧“不可发现”阻碍解除；此前要求 FFF4 可写的校验却导致反复关闭。增加特征诊断后，实测 FFF0=16..23、FFF5 handle21/properties0x0c、FFF4 handle18/properties0x10。FFF4 是 notify-only；前一阶段关于必须写 FFF4 特征的结论被该实测取代，CCCD 2902 写 01 00 才是本设备可验证的通知开启路径。

移除错误的 WRITE 要求：保留 FFF5 WRITE_NR、FFF4 NOTIFY、CCCD 写入成功门禁；仅在其他设备明确声明 FFF4 可写时才尝试该可选分支。构建与 COM6 app-only 烧录通过，应用 `0x107630`（1,078,832字节），SHA256 `5beeb81cdff7c67889fc663d534fbc0cc6a8d3b31bc2d432bdddbfb070d7a39e`。实际在启动约14.54秒完成订阅/中立/有效DUML门禁，phase0xb9，state3/fault0。该镜像已连接；可写FFF4的其他分支未验证。

随后将本机持久化 span 从120调到380，以接近 Mini 参考运动抓包幅度，源码默认仍120。真实DS4恢复并完成用户操作窗口：末次输入年龄12ms、move_queued45、neutral_queued143、center_queued2、write_failures0、RX持续新鲜。**用户明确回答“仍没有动作”**，物理控制继续失败。用户进一步确认本机摇杆能正常转动，排除电机休眠导致所有操作不动这一情况；不从通知或栈受理推断执行成功。

本轮证据：`build/rs3-mini-user-repair.log`、`build/rs3-mini-gatt-diagnostic-{build,flash,live}.log`、`build/rs3-mini-notify-only-{build,flash,observe}.log`、`build/rs3-mini-movement-check.log`。采集已结束并恢复gimbal INFO。20:41正在修正与Mini参考不一致的心跳接收端（E5→4，payload本身一致），并补运动TX与控制回复诊断；新修正尚未构建/烧录/实机验证。完整需求仍未完成。

## 20:44–20:48 物理运动首次通过

运动诊断版构建通过，应用 `0x107790`（1,079,184字节），SHA256 `b9c33273894c93e09d81275f58b19ae7c953ee007e1aee2d211f126e50ec733f`。COM6仅应用烧录、hash通过，保留NVS。与上一版的功能差异为steady `04/12` 接收端E5→4；另加运动TX/控制回复日志。连接再次恢复，phase0xb9、fault0。

用户在span380实测后回复“摇杆或L3已有动作”，进一步明确**左摇杆和L3都生效**。日志捕获10次非中立运动提交，主要pan+380；L3 `04/4c seq5986` 后41ms收到匹配回复 `sender04 flags80 payload00`。候选姿态tag24原始值范围-11..345，tag22为-161..-123，证明字段有变化，但角度单位、绝对零点、边界仍未独立标定，不能据此启用软限位。

本轮物理失败已被新反馈取代，仅覆盖短推和原生回中。未重放未知全套prologue，不能宣称其在所有固件上不必要。参考builder的receiver4与该项目原始capture-samples中的E5不同；实际采用builder并以本机效果验证。

20:49默认120四方向采集53906已结束，13命令PASS/恢复INFO。TX实际达到Pan/Tilt各±120，134次非中立输出；候选tag22范围-193..-4、tag24为-61..97，L3再次ACK00。末次真实DS4 age21ms、state3/fault0；用户实际方向和松杆观察仍待收。上一采集20181也已结束。证据 `build/rs3-mini-control-trace-{build,flash,live,console}.log`、`build/rs3-mini-control-trace-summary.json`、`build/rs3-mini-directions-{live,console}.log`及其summary。

## 20:52–20:56 并发采集与只读姿态候选

30分钟并发采集41649已启动，COM6被该进程占用，预计21:22前后结束。最初四分钟逐分钟样本均state3/fault0、真实DS4在线/输入新鲜、write_failures0；这只是阶段结果，30分钟尚未完成。只发送状态查询，不注入模拟运动。

新增严格只读 `rs3_pose_raw` 解码及status快照：仅CRC正确/指定端点的04/66/header1，三轴完整、长度2且无重复、完整TLV边界；解析失败不覆盖旧输出，发布同一快照/独立年龄，关闭/断开清除valid。尚未使用这些raw值改变任何控制行为或启用零位/限位。

实际31字节Mini payload由独立固定版Python DUML参考重建帧CRC；主机覆盖正负/INT16边界/字段乱序、未知字段、错误宽度/缺轴/重复/截断/尾字节、错误端点/命令、所有短前缀及不部分发布。相关gimbal **1/1通过**；IDF候选构建成功 `0x1079f0`（1,079,792字节），SHA256 `461407fe8630c54e0f532716f461e01eca6a62cda8fe897768d775dcb98c069d`，**尚未烧录**。硬件保持运动已确认的0x107790；其原始bin另存 `build/rs3-mini-control-trace-atom.bin` 并核对旧SHA。当前 `build/rs3-mini-atom/m5_atom_matrix.bin` 已变成此未烧录候选。

证据 `build/rs3-mini-pose-host-{build,test}.log`、`build/rs3-mini-pose-diagnostic-build.log`、`tests/host/rs3_pose_fixtures.h`、`build/rs3-mini-stability-30min-{live,console}.log`。267/267为此前全Host基线，本次没有重跑全量。build14227已终止；只有并发采集41649仍运行。

## 21:03–21:12 独立Tilt调速及激活提示

用户反馈“上下移动太慢”。此前两轴共享120，现新增独立 `tilt_span`：`gimbal speed tilt 240` 只调Tilt，Pan保留120；原speed数字命令同时设置两轴。最大幅度各自钳制400，死区/二次曲线/方向/5Hz/停止不变。现有cfg v1 blob保持原布局/绑定，Tilt单独NVS uint16键；旧配置没有新键时继承旧共享速度。

主机gimbal **1/1通过**，新增覆盖Tilt独立240而Pan120、两轴正负方向/死区、单独限幅和旧共享配置兼容。IDF构建通过，应用 `0x107c40`（1,080,384字节），SHA256 `0749fff3d2cc81c66a407aeb6496d0bb28b87bbeca4cf55c7dfaf0ba1496e8e1`。COM6只写0x10000应用/hash通过，NVS保留；此前只读pose诊断随本版首次烧录。

编号UART配置6命令PASS，19/401/未知轴拒绝且设置不变；status确认Pan120/Tilt240。随后RTS重启的10命令观察PASS，设置仍为120/240，约39.86秒恢复保存目标连接，真实DS4输入新鲜/fault0，未清除绑定。只读pose曾实际解码成功（sourceE5），断开后valid清除。软件重启持久化已验，真实掉电仍未验。

原30min采集为调速更新而停止：共15个状态样本、有效840.297秒（约14分钟），样本全部state3/fault0、DS4输入<200ms，最大报告年龄66ms，RX年龄275ms，无日志断开/写失败事件。此为部分结果，**30分钟未完成**。父wrapper工具exit0只代表shell结束，不能代替被主动终止脚本的PASS。证据 `build/rs3-mini-stability-partial-summary.json`，41649已终止/串口释放。

最新用户在调速验收时反馈云台提示需要激活、无法进入下一步；对以前是否激活回答“不确定”。因此Tilt240实际速度是否合适尚未通过。DJI官方FAQ说明未激活最多5次试用，并需手机联网、通过Ronin App登录DJI账号完成激活；是否本台已耗尽试用次数仍为推断，不能冒充已读取激活状态。[官方FAQ](https://www.dji.com/rs-3-mini/faq)

为让官方App连接，已执行stop/off，3命令PASS；GATT与peer释放、state0/enabled0/fault0，Pan120/Tilt240保留。**目前ATOM云台控制持久化关闭**，等待用户通过Ronin确认/完成激活；退出Ronin连接后再on并继续实测。没有自动绕过激活或修改云台固件。1766采集已结束/恢复INFO；所有本轮build/flash/UART handle已terminal，无后台采集。

证据 `build/rs3-mini-tilt-speed-{host-build,host-test,build,flash,config,observe}.log`、`build/rs3-mini-activation-release.log`。源码与协议/README同步，diff检查通过；LCD未烧录，未提交/推送；完整零位/限位/停止/稳定性等仍保留。

## 21:20–21:24 用户确认激活后恢复连接

用户最新回复“已激活”。按已有ATOM测试授权执行stop/on，12命令UART脚本PASS，5.156秒达到ready；末次state3/enabled1/fault0，Pan120/Tilt240保持，真实DS4在线、armed1、报告年龄15ms、write_failures0。约65秒采样内运动提交计数45→168，L3 seq6229收到匹配 `04/4c flags80 payload00`。这些只证明输入和链路工作；本次上下速度是否合适、松杆实际停止仍等待用户反馈。

激活后本次 `04/66` payload为19字节：`01 06 01 f1 07 01 00 08 01 02 0a 01 00 0b 01 00 0c 01 00`，没有三轴tag22/23/24，严格只读解析器保持 `pose_raw_valid=0`。不能把激活前31字节可解析状态沿用为当前角度反馈，也不能把 `04/10` 的同名短字段当作姿态；任意零位/软限位仍未实现。

采集98833已正常结束，COM6释放，日志恢复INFO，无后台采集。没有再次修改或烧录固件；LCD未烧录、未提交/推送。证据 `build/rs3-mini-after-activation.{uart,log}`、`build/rs3-mini-after-activation-console.log`。

## 21:26 基础摇杆确认与关机重启测试

用户回复“摇杆正常，下面测下关机重启”。保留Pan120/Tilt240，激活后的基础摇杆控制已获实机确认；此反馈未提供100ms停止测量。开始采集云台关机/重新上电：ATOM与DS4保持开机，摇杆居中，不通过off/on或清配对代替真实电源循环。自动恢复、重连无自行转动和恢复后的摇杆/L3仍待本轮日志及物理反馈。

证据入口 `build/rs3-mini-power-cycle.{uart,log}`，运行状态见本地记忆；此节为测试启动记录，不是通过结论。

21:31初段64命令PASS结束；181.86秒内状态均Connected/fault0，未出现关机断开事件，因此没有形成电源循环证据。延长只读采集 `build/rs3-mini-power-cycle-waiting.{uart,log}` 供用户实际关机/开机操作，结果待收；不能把脚本PASS当作重启通过。

## 21:35 云台关机重启结果

用户对本轮回复“正常”。延长采集中99.578秒收到 `Disconnected reason=19`，随后进入Searching/armed0，真实DS4保持新鲜；170.735秒发现保存目标，171.828秒控制ready。发现到ready为1.093秒；断开到ready的72.250秒包含用户关机和操作等待，不能当成开机耗时。恢复后Pan120/Tilt240不变、fault0、write_failures0，运动计数366→423，L3计数4→5且seq6006收到匹配回复。

本轮确认云台关机/开机后自动恢复控制。ATOM真实冷启动和仪器测得的100ms停止尚未验证。结果取得后主动结束专属95412采集，末样本261.141秒state3/fault0、DS4fresh12ms；工具exit1来自指定采集进程被终止，不能写成完整脚本PASS或10分钟/30分钟稳定性。COM6已释放，日志INFO。

用户随后询问自动配对新云台。源码无保存目标时匹配附近唯一Mini并在控制ready后保存；已有目标只连接该目标，换新机需 `gimbal pair`；多候选不自动挑选。本轮没有执行pair、更改绑定或烧录。证据 `build/rs3-mini-power-cycle-waiting.log`、`gimbal_link.c`、`gimbal_proto_rs3.c`。

## 本机证据（Git ignored）

`build/rs3-mini-atom-{final-build,flash,first-boot}.log`、`build/rs3-mini-controls-live.log`、`build/rs3-mini-host-final-{build,test}.log`、`build/rs3-mini-partition-before.bin`、`build/rs3-mini-atom/m5_atom_matrix.bin`。原始串口可能包含设备地址，正式记录不复制这些身份。
