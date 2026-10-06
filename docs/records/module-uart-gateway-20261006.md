# 2026-10-06 独立 UART gateway

LCD UART 行处理、正常命令分派、帮助与结果格式归 components/app_console。Camera/Wi-Fi/UI偏好/status/I²C/SIM/基准编码器随迁，私有头只供 UART 实现；Console CMake 只依赖基础SDK/common，不依赖功能 component。LCD 的 common/debug_console 单一编译实例归 Console，ATOM 仍编译同一通用行处理源。

Core 直接启动 UART，不再通过 main 的 camera_console callback。UART 创建失败记录错误后仍启动 Camera；router 与功能服务不被 UART 启停控制。Core 重启先关闭 UART 接口。保留原 UART0、512 字节 RX、4096 内部 RAM任务/优先级2、20ms 读取周期、参数解析与编号回复。新 cooperative stop 关闭 UART endpoint、取消关联等待，再等待 reader/driver释放；超时保留任务与驱动，不强删。创建失败但驱动释放失败时保留所有权，stop/retry继续清理。读取错误退出先关闭 UART endpoint，异步生产者可感知旧 UART 退休，driver释放失败重试。

固定订阅在 Core 冻结前建立；UART 单独重启复用冻结订阅，router 重启后重新建立。未知命令只输出错误，不改变 router/业务生命周期。删除 main/maint_probe.c/.h、维护串口 command/poll、旧手柄维护 API 和无人调用 input_console；旧结果队列删去，HTTP off 完成由控制worker归还计数。旧维护网页控制器、认证、LCD入口呈现及全局生命周期仍待替换，当前不提供串口维护入口。修复 fault help 被误放 poll 持续输出的问题，仅 help 时显示。

主机111/111成功，原54注册和断言保留；新增console_lifecycle、Debug/Release gateway fixtures，覆盖创建/driver失败、停止超时保留、读取错误退休、未知/维护命令拒绝、poll不输出帮助、frozen订阅重启与router重启。Core回归注入UART失败仍保留相机启动，再次boot可重试UART。Windows fixture采用CRT stdout锁别名；初期fixture缺fake_log及旧边界把本地UART头错认功能头已修正，最终-Werror与全套通过。fake非RTOS/SMP/cache-off/真实UART故障证明。

最终构建数值见下方。证据build/module-uart-gateway-{host-build,host,default,stable,release,atom-debug,atom-release}.log和module-uart-gateway-graph.json。正常UART只链接typed消息/纯值/SDK诊断，全部LCD ELF无maint_probe/maint_mode_command/poll/gamepad；Release无SIM/bench/fault实现，compile graph无main编码器，单UART reader source。未烧录、实机测试、提交或推送。

完整计划仍active。下一步Core组合根、UI/config/正常端点全停止、启动SoftAP:80网页触发与不可逆独占维护、旧兼容API全清理。

最终构建：LCD Default `0x35ae20` / Stable `0x35a000` / Release `0x34eda0`，ATOM双模 Debug `0x1043f0` / Release `0x1017f0`，五构建均成功。全部本批构建会话终态，无本批后台串口/构建；其他聊天状态未检查。
