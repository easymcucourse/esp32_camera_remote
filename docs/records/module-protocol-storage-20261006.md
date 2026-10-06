# 2026-10-06 协议与持久化兼容核对

本批以当前HEAD `01d6b4e920e7fff2c9d4b9eb9f544d7524083365` 为重构前源码参照，直接读取Git blob及当前文件，不将过去报告当本次比对结果。机器记录 `build/module-protocol-storage-compat.json` 保存路径、LF规范化SHA256及比较范围。

14个可比较文件中13个内容一致：common的ATOM protocol/client/sim各源头6个、PTP dataset源头及packet头3个、Sony props/liveview源与liveview/codes头4个。唯一不同为sony_props.h：内嵌sony_mode_state_t成员移成同布局camera_choice_state_t typedef，成员类型/顺序均保持，parser源一致。sony_control_encoder.c不在HEAD基线中，不能作逐字相同结论；控制合并后的真实encoder/PTP fixture仍保留线字节及事务断言。

Wi-Fi codec `main/wifi_config.c`→`common/network_config.c` 在类型/API/常量/random_fn命名映射后，忽略空白的完整源码相同。它是明确重命名下的词法对照，不是所有网络运行行为等价证明。

| 存储 | 当前兼容规则 | 本次依据 |
| --- | --- | --- |
| wifi_ap/cfg | 100-byte blob，byte0版本1；channel/show_password/SSID和password长度及padding位置不变。默认配置删除cfg，坏/过大/未知版本记录仍按旧策略修复默认值。 | 原/current codec对照；真实wifi_saved_config及wifi_config断言。未知Wi-Fi版本不采用ui_prefs的保留策略，不能混述。 |
| sony_remote/guid、peer | GUID16/peer22，不改变原namespace/key或记录布局；已有GUID缺peer视为未配对，orphan/尺寸错拒绝，不正常自动erase。 | camera_identity_store源及identity_work/producer/factory测试；旧调用源码保存相同尺寸。 |
| ui_prefs/info、pad | 原u8 keys保持；新增schema=1。缺schema按legacy v1读并保留有效info/pad；未知schema拒绝且load不写，普通UI启动fallback不重写。维护显式保存才写三key并commit。 | 原ui_preferences NVS key/type读取；真实preferences_store+core_settings legacy/future/missing/read/set/commit失败场景。 |

UI prefs新增schema是版本化兼容扩展，未建立新的namespace或迁移原key。factory跨namespace仍不原子；单handle commit不能推导所有Flash故障路径整体原子。OTA分区未因本批或上述记录布局改变。

执行当前26项ATOM/PTP/Sony/配置/identity相关CTest全部通过，`build/module-protocol-storage-host.log`；此前完整HOST262仍有效。本批只新增审计证据及验收范围，无功能/测试代码变化，不重复构建；LCD/ATOM五配置沿用已记录基线。未烧录、未实机、未远端CI、未提交/推送。

A36软件字段/版本/测试证据补齐，真实Sony/I²C wire、HTTP无认证触发和硬件启动效果仍待实机。精确文件比对不能替代重构后真实通信；plan要求的LCD/Sony冒烟继续未完成。
