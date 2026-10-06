# 2026-10-06 消息路径与旧头审计

上轮 HTTP 退出收口属于代码进展。本轮重新核对仍为“待实施”的 S5.1/S6.2，以及 V11/V12/V25/V28/V32 和 A14/A17/A19/A21。以下以当前源、实际 archive 边和 host 链接命令为依据，不能替代硬件验收。

## 指定跨域路径

| 路径 | 当前生产入口与所有权 | 直接证据 |
| --- | --- | --- |
| 客户端发现/RSSI/选择 | Camera discovery 向 Wi-Fi endpoint 发 CAMERA_DISCOVER/WIFI_SELECT_CAMERA；UI 状态由 Wi-Fi message 提供 | camera_discovery.c、app_wifi_messages.c、UI Wi-Fi message handler；fake discovery/UI/bridge tests |
| PTP TCP | sole ptpip_client 经 Console request/reply 请求 Wi-Fi channel；两lane worker持有 opaque app_wifi_channel | ptpip_client.c、wifi_channel_messages.c；真实PTP client+fake Console及fake driver/bridge |
| 菜单/参数/输入动作 | UI/Input/UART 编码 Camera/UI request，Camera endpoint 统一校验/调度；domain kernel 仅在本组件直接调用 | ui_menu_messages.c、input_service.c、camera_commands.c、camera_endpoint.c；menu/service/UART/endpoint tests |
| JPEG | Camera frame slot 创建 readonly lease，Console fanout，UI endpoint借用并在message释放时归还；result metadata回Camera | camera_frames.c、app_message_router.c、ui_frames.c；frame/outputs/router/UI frame tests |
| Camera状态/属性 | camera_outputs 发布 copied值或readonly properties lease；UI handler检查generation后更新自身model | camera_outputs.c、ui_camera_messages.c、app_message.h；outputs/UI tests |

生产三配置实际符号边上一批已查：Default/Stable41、Release37，无跨域门面调用；间接注入只存在计划规定的 Core→维护系统ops、网络driver ops、Camera generic factory/ops 和 Input内部 action callback，均不是功能模块间保存另一个模块的运行时动作函数。共享纯值/codec/storage primitive及Core生命周期直接调用不属于业务消息旁路。Sony/PTP内部方法仍是各自backend边界。

`app_camera.h` 只由 Core 与本组件使用，UI/Input/UART不含此头。维护仅Wi-Fi/SDK/系统ops。`wifi_esp32_create` 只在Core网络组合根选择，修改网络实现无须修改PTP/Web/UI/Camera；可替换性已有fake driver和fake Camera backend验证，尚无第二个真实实现的硬件验证。

## 独立构建与主机验证

读取现有 `build/host/CMakeFiles/<target>.dir/link.txt`：camera_discovery、ui_camera_messages、input_service、input_service_release、uart_gateway_debug/release、uart_camera_commands、uart_wifi_messages、uart_preferences、uart_bench 十个目标没有 app_camera/camera_backend/Sony/PTP/app_wifi 实现库链接。其 fixtures 编译真实被测源，使用假消息 endpoint；部分源码通过 C include进入fixture，不能仅从link对象名判断实际源码范围。快照 `build/module-route-header-audit.json`。

21/21选定CTest重跑：PTP client、Wi-Fi channels/bridge、实际router、discovery、UI camera/frames、Camera outputs/frames、Input service两配置、UART各encoder/status/bench/gateway及console lifecycle，见 `build/module-route-header-host.log`。上轮完整259仍为全套基线；本轮不改固件源码。

Console lifecycle实际debug_console覆盖UART read失败/unknown command/重复start/driver删除失败重试，只退休UART owner，不调用其他业务生命周期；gateway只消费typed messages，维护/probe命令返回未识别。关闭UART/router由Core分别编排。fake gateway本身的测试不会证明其他业务状态，因此同时依据其完整依赖与生产调用边。

PTP client在network generation改变时取消并拒绝旧channel，bridge验证旧token/旧generation拒绝与迟到结果归还；facade旧channel在generation变化后返回STALE。原token不能进入新会话。实际Wi-Fi并发/设备断网时延仍待硬件。

## 旧头与边界

扫描 main/components/common 的全部生产header，去掉注释、include及include guards后：没有只有转发include的旧头，也没有重复header basename。`main/`仅入口源、CMake与Kconfig；旧camera_pair/controller/transport/device/compat路径已删除。测试legacy故意保留原协议支持，不进入生产，不应为满足扫描而删除测试依据。

backend/PTP/Sony include仅在Camera私有实现、generic backend自身、Sony backend与PTP内部。新增通用boundary规则按header basename拒绝这些头跨到其他功能component。源码门禁通过，当前SDK网络header只在wifi_esp32。

工具与当前开发README搜索没有失效旧main/Sony路径（verifier里的拒绝名单和历史批次记录有意保留）；全docs历史引用与全部辅助工具仍待全量审核，不能把此次有限搜索当作S6.3/A38整体完成。

无新固件构建、烧录、实机、远程CI、提交或推送；LCD/ATOM构建及禁符号基线分别见[HTTP退出](module-http-stop-20261006.md)与[五配置](module-api-build-audit-20261006.md)。PTP/Sony每个未使用项的删除依据及硬件冒烟仍待审计，未做无证据删除。

## 补齐真实UART Input编码器覆盖

进一步追踪 fixture 发现 gateway 把 i2c_console_command/lcd_sim_command 替成 stub，provider 测试只覆盖被编码后的 message，不能单独证明编码器行为。新增 test_uart_input_commands 直接编译真实 i2c_console/lcd_sim、真实 app_message lease 与 pad_cmd parser、纯 i2c formatter，fake Console endpoint 接收所有消息。第十一个独立实际链接目标快照已加入 JSON。

覆盖 I2C log/stats/reset/poll与非法mode拒绝；SIM选择失败不发送ENABLE、transport失败、ENABLE拒绝及正常两步选择、状态查询、未知电量值/范围拒绝/gap；tap复制序列为readonly lease并分配非零token，transport失败消费lease、32槽耗尽不发送请求且保留已有owner；完成事件旧SIM generation或UART epoch不打印。fake reply也附真实lease，所有请求/回复终态准确归还，live count归零。

新增fixture首次编译缺内部lease计数声明，补include app_message_internal.h（测试允许的私有支持）；生产public API未扩展，无降低告警。最终完整260/260通过，原54保留，日志 build/module-route-header-full-host.log。未更改固件源码，因此不重复五配置构建；上一批LCD/ATOM构建继续有效。
