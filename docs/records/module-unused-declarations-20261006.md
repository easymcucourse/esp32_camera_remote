# 2026-10-06 未引用声明与当前路径核对

删除ptp_codes.h的4个未使用声明：PTPIP_PORT、PTP_TYPE_INT16、PTP_TYPE_UINT16、PTP_TYPE_UINT32。扫描整个生产/测试源无名称引用；端口15740、标量类型编码与现有解析逻辑仍保留。Sony历史设计中PTPIP_PORT只是旧示例命名，不是新实现的调用依据；本批没有删除任何解析分支、操作或样本断言。

完整HOST260/260、LCD Default0x3586c0/Stable0x3578a0/Release0x34c690全部构建终态0；同名前缀host/build日志。ATOM未改。本批记录了前后image SHA256，**实际并不相同**（build/module-unused-declarations-binary-compare.json），因此不声称生成binary逐字相同。尺寸与主机协议断言通过只证明其对应范围，不将不同hash推断为特定原因或当作硬件证明。

当前精确backtick路径扫描发现：I2C设计旧main/atom_link.c、Wi-Fi需求旧main/wifi_ap.c已纠正；common/atom_protocol.h/.c简写展开为两个实际文件。云台control及其test路径本是未来设计，明确标注规划/当前不存在，不实施计划之外的云台控制。记录build/module-current-doc-path-audit.json保留最初发现，后续分类见下表。

| 最初发现 | 处理 |
| --- | --- |
| main/atom_link.c | 当前app_input_atom路径 |
| main/wifi_ap.c | 当前wifi_esp32路径及Core/app_wifi职责 |
| common/atom_protocol.h/.c | 两个实际路径 |
| gimbal_control.c/test_gimbal_control.c | 尚未实现的规划文件，明确范围 |

Wi-Fi需求R2.3按最终拆分计划更新为启动维护Web-only，旧串口/手柄编辑取消；历史实测不冒充新版本。Matrix/gimbal设计修正实际BTDM/BLE/GATTC与Ultimate 2已编译事实，保留云台并发/30分钟硬件目标。Sony旧设计1–14已有历史标记，不将旧fd目录图重新当成当前架构。

PTP/Sony static helper补充源码token/表引用清单，机器记录build/module-camera-static-helper-audit.json。它只是去注释后的定义/名称引用辅助定位，不能单独证明运行可达；真实host Werror构建与当前factory/ops绑定仍是验证依据。未根据单一文本命中删除函数。

剩余：协议删除/合并后的真实相机smoke、全量文档示例语义、计划中原可逆系统租约与最终不可逆切换语义、真实owner/SMP/cache-off/启动/显示/HTTP隔离验收。设备信息仍待用户确认，无烧录/实机/远端CI/提交/推送，完整目标继续。
