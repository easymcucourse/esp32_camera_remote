# 2026-10-06 实际依赖与运行关系图

新增 tools/check_component_graph.py：读取实际 IDF component public/private 元数据、compile_commands 与 post-project HTTP target link interface；验证项目组件集合、跨功能依赖白名单、Core 公共契约无实现依赖、Camera backend 私有、全部 HTTP translation units 的 SoftAP bind 编译定义、实际 HTTP→wifi_esp32 链接边，以及项目节点加注入 SDK HTTP 节点无环。普通 project_description 在顶层绑定边注入前生成，不能单独当作最终图。CMake 用 file(GENERATE) 保存真实 target property，没有修改 SDK 文件。

Core app_wifi 依赖收回 PRIVATE。SIM 条件依赖首轮导致 Debug 缺 input_sim.h：IDF 在 sdkconfig 前展开依赖，full configuration 的条件不能可靠改变 early requirements。最终保留空 Release SIM 注册，仍由既有 Kconfig 门控实现 sources；检查实际 Release 编译列表无 SIM，不能把空注册描述成链接实现。三个实际图均为 20 项目/绑定节点、89 条显式依赖，无环；SDK 自身内部图不在无环声明范围。

[当前图](../design/module-dependency-graph.md)包含由实际 Default Debug 元数据生成的编译图，以及逐个 endpoint/provider/Core/backend/画布路径核对的运行关系图。完整基础依赖 JSON 位于 build/module-dependency-graph-{default,stable,release}.json。运行关系图是源码分析，不是动态 tracing 或硬件证明。

检查器新增 10 个正反例：Debug、Release 空注册、Core public、功能旁路、public backend、HTTP adapter 回路、漏实际链接、漏编译 adapter、Release SIM 实现、漏必须组件。它们验证 verifier 的拒绝边界，不替代真实三构建图检查。CTest 244/244，原 54 保留。CI LCD 构建调用实际图门禁，上传 component-graph.json；扩大 LCD Release forbidden symbols 至 SIM/LCD模拟/benchmark/故障/Camera显示预约。远程 CI 尚未运行，不能声称绿色。

LCD Default `0x3587b0`、Stable `0x3579a0`、Release `0x34c7a0`，三构建终态 0；日志 build/module-dependency-graph-{host-build,host,default,stable,release}.log。ATOM 本批未涉及未重建。无烧录、实机、提交或推送；所有本批 handles 终态。

完整 goal active。后续仍需全 S/V/A38 逐项证据核对、各 owner 完整退出路径/资源表、当前旧文档同步、五配置最终验证与硬件证明；实际图不能证明全部源码没有直接调用旁路，须结合边界扫描与调用方审计。

最终 exact CI Release forbidden 23 symbols 在实际 Release ELF 中均无，日志 build/module-dependency-graph-release-symbols.log；完整244重跑通过。boundary/doclinks（139 文档、562 链接、0 问题）和 diff 通过。
