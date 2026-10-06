# 2026-10-06 Camera／router／UI 帧链路主机验证

新增 `tests/host/test_frame_bus_integration.c`，同时链接真实 Camera frame slots、Console lease/router 与 UI frame handler。仅 RTOS、时钟、UI 模式/代际与 JPEG 显示接口为 fake；显示接口断言借用的指针与长度等于 Camera buffer 内的原始区域。

覆盖已有 JPEG 队列中的 control 优先、多订阅共用只读 lease、UI 返回后 observer 仍占用 buffer、Camera control 队列满时两条 UI 结果缓存及后续按 token 消费、停止时排空 queued observer 而不撤销 delivered UI lease、停止后结果发送失败仍释放 JPEG，以及 partial fanout 失败后已收到消息的订阅者仍保持 buffer。

将原 router fixture 的 RTOS 部分原样提取到 `tests/host/router_stubs/fake_router_runtime.h` 供两个目标共享；原 router 断言和业务场景保留。该调度器通过显式轮询/协作式 yield 模拟 worker，不运行真实并发线程，不证明 SMP、cache-off、真实 JPEG 解码或 Core 完整停止顺序。

验证：host 构建终态0，四项相关测试通过；完整 CTest **261/261**，原54保留。日志 `build/module-frame-bus-build.log`、`build/module-frame-bus-host.log`。本批只修改主机 fixture、测试注册与证据文档，没有生产固件源码修改；五配置构建沿用已记录基线，不重复构建。没有烧录、实机、远端CI、提交或推送。

V14/V15/V29 的 Camera→router→UI 软件集成证据补齐上述范围；真实并发、Core stop 编排及硬件验收继续待验证。设备串口/实机范围的用户确认仍待回复。
