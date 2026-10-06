# 2026-10-06 Owner停止与协议保留依据

重新检查实际 Core maintenance/shutdown、UI endpoint/render/model 与 Sony/PTP 当前库，完善清单验证范围。没有修改固件算法或删除协议分支。

## 停止与独占证据

Core真实normal_stop的顺序由测试trace确认：UART→Input→providers→benchmark→Camera physical→Camera endpoint→network config/bridge→preferences→UI endpoint→renderer→System/router，然后固定画面→孤立配置worker→完整Web activation。各阶段失败不发布完整维护路由，只走RESTART；前置owner失败时不会删除依赖资源。boot barrier期间不开始排空；正常模式关闭HTTP trigger，首次HTTP claim赢得独占后不可返回NORMAL。

这个组合测试编译真实Core/mode/shutdown，但下层owner是stub，因此只能证明顺序/条件与失败政策。真实Input安全释放、Camera lease/channel排空和UI停止分别有owner级fixture，不能据此宣称所有真实任务在SMP上组合运行通过。

UI endpoint fixture实际停止时拒绝新帧、归还已收lease、保留未发送result直到可重试；render fixture等待正在使用的canvas返回再清理workspace，之后所有普通渲染入口拒绝。model freeze清空普通状态且拒绝迟到setter。fixed画面测试用假字形把一个白像素写到黑画布并检查字符串MAINTENANCE，**不证明真实48px字体或LCD最终像素**。

Web/trigger/OTA独立目标不链接正常应用，factory回调由Core注入；fake实际Web可直接操作settings/config/factory/reboot/OTA，无认证分支。Core factory真实函数覆盖freeze/read/write/identity/preferences/rollback失败与超时，但NVS操作是stub，不能代替持久化与掉电验证。

88/88相关Core/maintenance/UI/parser主机回归通过：`build/module-owner-evidence-host.log`。另15/15 factory reset、四份props样本和IPv4/IPv6 HTTP scope通过：`build/module-owner-evidence-extra.log`。这些是现有260全套中的选择复核，本轮没有新增测试。

## PTP/Sony使用清单

从当前Default实际PTP/Sony archives提取47个global函数，再与最终ELF核对：43个仍存在，4个不进入固件但有fixture依据：sony_client_set_exposure_mode、sony_encode_set_exposure_mode、sony_parse_properties、sony_parse_scalar_properties。对应源与完整逐函数清单见[使用清单](../development/module-camera-symbol-usage.md)，机器记录 `build/module-camera-export-usage.json`。被ELF引用不等于实机成功执行；fixture mention还需核对实际注册，此处PTP protocol/原parser及四样本目标已复跑。

两个代码常量头共59个PTP/Sony宏或packet枚举：55个有生产或测试source mention，4个无source mention：PTPIP_PORT、PTP_TYPE_INT16、PTP_TYPE_UINT16、PTP_TYPE_UINT32。记录 `build/module-camera-code-usage.json`。后续需核对这些声明的需求/样本语义与具体删除/合并路径；没有只因缺少文本引用就删除。

该清单仅涵盖global函数和两个常量头，不冒充所有static helpers、协议解析分支、删除符号与历史样本的全量使用证明。Sony client control一层writer绑定适配仍需对照“纯转发wrapper删除/合并”要求逐项决定；原fixture依赖应保留或等价迁移。

## 实机条件

当前只枚举到COM11/COM101，历史LCD COM8/ATOM COM6未出现。已请求用户确认设备连接与当前端口；未打开设备或烧录。原计划明确协议删除后需真实相机冒烟且整个结构拆分至少一次LCD冒烟，当前不能满足这些证据。测试/构建通过，硬件待验证。

无新firmware构建（源码未改）、实机、远程CI、提交或推送。完整目标继续，未将硬件条件从范围中移除。
