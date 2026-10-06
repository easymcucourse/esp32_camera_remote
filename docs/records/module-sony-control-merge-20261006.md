# 2026-10-06 Sony控制转发层合并

按计划“合并只转发参数而不增加校验/状态/所有权的wrapper”，删除独立sony_client_controls源/私有头及其7个sony_client_*入口。它们只构造writer并转发原encoder，没有独立状态、校验或清理语义。

Sony backend的set/step/action现在在现有scope内直接构造writer，context指向同一内嵌ptpip_client，write callback使用原ptpip_client_send_data。timeout/transaction_end/accepted→统一result、能力/属性/动作校验均保留。sony_control_encoder.c没有修改；线格式仍只有一份，原fd历史fixture也继续复用该encoder。

协议fixture改为直接encoder+同样的PTP writer binding，保留全部循环、opcode/property/宽度/bitpattern、transaction递增、probe response、拒绝值及零I/O断言。原NULL client测试等价地使用writer.context=NULL；PTP仍拒绝空context。没有删除测试或降低告警。首次fixture替换漏掉两个非法方向调用的writer参数，Werror报类型不匹配后修正，完整重跑通过。

## 删除与替代路径

| 旧入口 | 当前替代 |
| --- | --- |
| sony_client_set_scalar | backend set→sony_encode_set_scalar |
| sony_client_setting_step | backend step→sony_encode_setting_step |
| sony_client_manual_focus_step | backend action→sony_encode_manual_focus_step |
| sony_client_shutter_button | backend action→sony_encode_shutter_button |
| sony_client_movie_record | backend action→sony_encode_movie_record |
| sony_client_zoom | backend action→sony_encode_zoom |
| sony_client_set_exposure_mode | 无生产调用；原fixture直接sony_encode_set_exposure_mode |

需求/样本依赖的是控制语义和相同PTP字节，不是这层转发入口；对应编码与样本断言保留。原七入口的测试调用已等价迁移，生产/CMake/测试不再引用旧头或源。boundary新增规则拒绝旧转发层重新进入生产。

## 验证与限制

- 完整HOST260/260（原54保留），`build/module-sony-control-merge-host.log`。
- LCD Default0x3586c0、Stable0x3578a0、Release0x34c690，三构建终态0，均<5MiB；`build/module-sony-control-merge-*.log`。ATOM源未改，沿用最近两配置。
- 实际图20项目/绑定nodes、91显式边无项目环；archive direct边41/41/37通过。graph/symbols机器记录同批前缀。
- 最新两个archives40global函数、最终Default ELF37，3个fixture-only有来源。七个退休函数在archive/Default/Release均不存在，Release23禁符号不存在；使用清单与release-check JSON见本地同批证据。

真实相机冒烟仍缺失：已请求用户确认设备连接/端口，未收到确认，未烧录。计划要求合并后的真实相机smoke，因此本批只记录“构建/主机测试通过，硬件待验证”，不将S4.5/S4.13/A26标完整。其余static helper、4个无引用常量及全部文档/工具仍待收尾；目标继续。
