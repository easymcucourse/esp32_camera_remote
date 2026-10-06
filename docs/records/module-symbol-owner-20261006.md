# 2026-10-06 模块符号归属与资源所有权

新增 tools/check_module_symbols.py，读取真实组件静态库的 nm 定义和未定义符号，检查函数及数据的直接跨库引用是否有显式依赖，以及 Core→Camera/UI 生命周期、Input provider 回报、SDK HTTP→Wi-Fi bind 的允许接口。检查包含库中最终可能被链接器裁剪的函数；SDK 基础库除 HTTP bind 外不纳入项目功能边界。它不能证明间接 callback/ops 的运行时行为，仍须结合源码审计。

实际检查发现 Console 的 i2c_monitor_result_name 由 app_input_atom 提供，违反 Console 无功能模块依赖。将纯格式转换实现迁至 common/i2c_monitor_format.c，由 common_runtime 唯一编译；ATOM 私有 monitor 继续归 provider。app_maintenance、app_wifi_messages、wifi_esp32 补齐它们实际使用的 common_runtime 私有直接依赖。三种实际图均为 20 项目/绑定节点、92 条显式依赖；Default/Stable 静态库各 41 条直接符号边，Release 37 条，全部通过，无项目图环。

删除 Core Wi-Fi 旧兼容客户端函数，保留明确的 private/app_core_wifi_boot.h 启动所有权接口。无效保存配置启动时仅使用 RAM 默认值，不回写 NVS；维护 Web 配置写入和自动配对确认保持各自所有权。新增真实 boot owner 的七个主机用例，测试链接不提供保存 writer：正常、无效配置、create/read/get/init/start 失败。六个符号检查器正反例覆盖声明缺失、provider 旁路、私有 UI 调用、非 Core Camera 调用和 HTTP 非 bind 调用。

[资源所有权表](../design/module-resource-ownership.md)记录任务、栈、队列、lease、JPEG/画布、持久化与停止顺序，并明确 retained 资源、各阶段预算及 SDK HTTP 同步 join 的时间限制。[当前依赖图](../design/module-dependency-graph.md)已按三构建新元数据更新。LCD CI 调用符号门禁并上传 JSON，远程 CI 尚未执行。

CTest 252/252 通过，原 54 保留。LCD Default `0x358780`、Stable `0x357970`、Release `0x34c770`，三构建终态 0。日志 build/module-symbol-owner-{host-build,host,default,stable,release}.log；符号边 build/module-symbol-owner-{default,stable,release}.json。ATOM 本批未改未重建；无烧录、实机、提交或推送，无本批仍在运行的构建。

完整目标仍 active：当前设计/开发/用户文档同步、全 S/V/A38 证据核对及五配置最终验证尚待完成。源码/主机/构建通过不能代替硬件启动、SMP/cache-off、画面和稳定性验证。

最终重跑 CTest 252/252；boundary/doclinks（141 文档、570 本地链接、0 问题）与 diff 检查通过。真实 Release ELF 的 CI 23 个禁符号全部缺席，日志 build/module-symbol-owner-release-symbols.log。

证据范围更正（2026-10-06后续源码核对）：no-writer链接测试只覆盖Core boot owner。wifi_saved_read内部仍调用wifi_saved_write修复损坏blob，保留原错误恢复行为；前文“无效保存配置不回写”仅适用于Core的国家范围RAM fallback，不能扩展为后端读取无任何NVS写入。见module-network-docs-20261006。
