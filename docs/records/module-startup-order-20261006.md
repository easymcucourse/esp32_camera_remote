# 2026-10-06 启动页 trigger 与正常服务启动顺序

本批落实计划 A35 的源码顺序：基础 UI → 同步 ATOM I²C prepare → AP/配置 owner → 独立维护初始化/端口 80 trigger → router/System/UI/Input → 正常 Wi-Fi bridge → ATOM/Debug SIM provider → Camera/UART → health/boot barrier release。普通阶段间检查 STARTUP；HTTP claim 后跳过后续阶段，已进入的调用完成后由上一批屏障保障停止。AP 启动不再隐式启动正常网络桥。

`input_atom_prepare()` 只取得板级 I²C bus 并添加 0x42/100kHz 设备，不创建任务、endpoint、Input registration，也不收发协议。重复 prepare 幂等；错误同步返回。start 可使用准备好的设备或为独立调用方准备；stop 在没有 worker 时也返回未使用的设备，移除失败保留句柄以供重试。运行中由原 task 返回设备，失败继续重试，返回完成后发布 running=false。原 3072 字节/pr4 task、50ms 周期、协议及 15ms 等待均未改变。

HOST 241/241，原 54 保留。新增 K/M/G 启动失败测试以及 m/g/p/a 阶段结束时 claim 场景，验证后续普通阶段跳过；原 trigger claim 场景验证所有普通资源创建跳过且未冻结不存在的 UART。生产 ATOM fixture 验证 prepare 无 task/endpoint/registration、幂等、bus 失败、未使用设备释放失败保留/重试，以及原 worker 协议/安全释放。第一次回归发现 fixture 的既有首次 removal_failure 也应用于 prepared 设备，补充对应拒绝/保留断言后通过，未隐藏错误。

LCD Default `0x358710`、Stable `0x357900`、Release `0x34c710`，三构建终态 0；ATOM 固件源码未改，未重建。日志 `build/module-startup-order-{host-build,host,default,stable,release}.log`。无烧录、实机、提交或推送。

A35 的源码顺序已调整，但真实硬件安全时序没有验证。同步 I²C prepare 保留“设备先于 AP”约束，不能证明旧 task-before-AP 的效果仍成立。部分初始化清理、UART 失败时订阅冻结策略、最终 S/V/A38 与所有配置构建仍待完成；完整 goal active。

三 compile graph 的 physical/Core owner 唯一，三 ELF 均包含 staged prepare 和正常 bridge 启动函数；证据 `build/module-startup-order-graph.log`。boundary/doclinks（134 文档、553 链接、0 问题）及 diff 检查通过，所有本批 handles 终态。
