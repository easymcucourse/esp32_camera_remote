# 项目修改清单

[English](../en/request/improvement-request.md) · **简体中文** · [日本語](../ja/request/improvement-request.md)

2026-10-10优先级按现状整理。源码、主机、构建、烧录、实机功能和稳定性分别判断；已接入不代表全部验收通过。当前证据见 [当前状态](../development/current-status.md)。

| 优先级 | 当前剩余工作 |
| --- | --- |
| P0安全/稳定性 | 冷启动、各owner完整排空、显示失败恢复、输入释放、NVS故障；禁止强删仍持有资源的任务。 |
| P1相机/网络 | 动态发现/授权/重连全组合、真实参数/录像/拍照效果、raw枚举、持续断流恢复。 |
| P1云台 | ≤100ms停止、取消回中、ATOM真实冷启动/异常断开、电量与姿态字段可信性、30min并发；任意零位/软限位需先获得可靠角度。 |
| P2性能 | 按 [分阶段计划](../design/liveview-memory-fps-plan.md)完成纯全屏/设置测量、heap/stack门禁、Stable和LCD重启后相机恢复；前段未过不直接增加第三JPEG槽。 |
| P2维护/UI | 新版Web排他/AP隔离、save/reset/OTA/掉电、全部显示/故障视觉、Ultimate实体映射。 |
| P3工具/文档 | 明示port/SDK、通用取景样本抽取、匿名fixture来源、英中日导航维护。 |

## 已接入基础

结构化Sony描述、DHCP目标、可取消typed TCP、参数目标合并/I²C v2/gap、模块资源与lease门禁、四项Debug/Release CI、双槽OTA、Stable配置、heap/FPS分析和RS3基础控制均已存在，不重复列为待新增。完整实机和性能验收另列，远端CI与branch protection本轮未核验。

## 后续范围

触摸备用控制、可配置按键、焦点位置/放大、广泛BLE兼容、任意云台零位/限位仍属未来功能。本次文档整理不实施这些功能。取景计划最终指标未达；约14min云台观察不算30min。

代码清理必须保留有意义的旧fixture/字节断言和日期证据。当前触摸板info-next由Input忽略；显示档位通过启动Web保存后重启。当前POWER_ZOOM是用户声明，不是自动识别。详见 [测试](../development/testing.md)、[模块验收](../development/module-split-checklist.md)。

被取代的原稿见 [历史归档](../records/improvement-audit-superseded-20261010.md)。

<!-- Preserve existing historical inbound fragments. -->
<a id="p0稳定性与故障恢复"></a>
<a id="p1atom-与手柄链路"></a>
<a id="p1ptpip-与-sony-协议"></a>
<a id="p1相机发现与网络配置"></a>
<a id="p2代码结构与可维护性"></a>
<a id="p2功能与交互"></a>
<a id="p2显示性能与资源"></a>
<a id="p2测试与持续集成"></a>
<a id="p3文档与工具"></a>
<a id="已核实但暂不列为当前缺陷"></a>
<a id="建议实施顺序"></a>
