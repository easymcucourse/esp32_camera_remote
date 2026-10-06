# 2026-10-06 Core 初始化屏障与提前 trigger

此前端口 80 trigger 在 Camera 启动后开放。本批提前到维护初始化完成后、Camera/UART 创建前；HTTP 已可在 Camera 不存在时取得维护模式。Core 使用原子 booting 屏障标记单次初始化期间的资源创建：HTTP 可先从 STARTUP 竞争为 ACTIVATING，但等待屏障释放后才关闭普通 admission。原 health 在屏障释放前不执行生命周期操作；普通 close/stop 与维护 activation 也拒绝在屏障期间触碰资源。

启动线程发现维护已取得模式时跳过 Camera；竞争发生在调用边界时，Camera boot 在 ACTIVATING/booting 下也直接跳过；已进入的初始化调用允许完成，之后才排空。启动错误设置 RESTART 并释放屏障，原 app_main 仍将错误视为 fatal；成功在 health 创建完成后释放。HTTP 等待使用原请求的 deadline，屏障超时不关闭尚在创建的 owner，只要求重启；不恢复正常。没有新增 task/queue，也未改已有 task 核心、优先级、栈或内存属性。

HOST 234/234：增加启动期 claim 跳过 Camera、维护等待屏障释放后停止、屏障超时未关闭任何 owner 三个场景；实际 Camera fixture 验证 booting 下 claim 不创建资源，mode fixture 验证不能提前 activate。原竞争及失败测试继续通过。fake 时钟与注入回调验证流程约束，不能证明真实 SMP、启动耗时或 I²C/AP 安全。

本批只完成 A35 的初始化竞争屏障及 trigger 先于 Camera。当前 router/System/UI/Input 和 ATOM/SIM 仍先于 AP/trigger；下一步必须分离 ATOM 物理准备与正常 provider 启动，把普通 Input/Console/Camera 组合移至 trigger 后，并完善部分初始化清理及 UART subscription 冻结失败策略。旧的“正常服务全部完成后才开放 trigger”设计已被本批取代，不可作为当前实现依据。完整计划仍 active；尚无烧录、实机、提交或推送。

LCD Default `0x358680`、Stable `0x357860`、Release `0x34c680` 三构建终态 0；三 compile graph 唯一 Core 生命周期源，ELF 屏障函数均在。日志 `build/module-boot-barrier-{host-build,host,default,stable,release,graph}.log`。boundary/doclinks（133 文档、551 链接、0 问题）及 diff 检查通过。ATOM 未涉及、未重建。全部本批 handles 已终态。
