# 2026-10-06 输入状态 → UI 消息

生产 main/atom_link 的在线、电量、SIM与协议状态更新改为 lease-free INPUT_STATE event，经Console转UI；删除所有 app_ui_set_sim/atom_status/atom_protocol/controller_battery直接调用。消息源暂仍由旧ATOM业务路由使用Input endpoint，未来完整input_service接管；不把这一步称为ATOM纯provider完成。原I2C/ATOM状态机、50ms周期和3072/prio4任务保持，无新task/缓冲。

UI endpoint在启动时订阅INPUT_STATE，private ui_input_messages校验类型、Input来源、EVENT flags、无lease、非零/不过期generation，以及完整payload（类型0/1、电量0..10/255、云台0..3、int8轴、18位按键、来源与connected一致）。非法payload在任何UI setter之前拒绝；成功才更新SIM/ATOM/协议/电量。消息排队失败不退回直接renderer调用，后续周期快照重试。断线/mismatch通知在原1/5秒重试等待之前发出，不延迟至等待结束。

SIM来源可独立于实体ATOM连接；ui_model controller_online门禁允许sim_active，不伪造ATOM在线。实体断开仍清controller状态/电量，离线电量为255。APP_MESSAGE契约补充Input生命周期generation与report source_epoch/id区别，UI不解释硬件事件号。

新增test_ui_input_messages覆盖正确报告/未知电量/SIM、无物理ATOM的虚拟在线、错误来源/flags/lease/payload、旧及零generation、mismatch/断线；test_ui_model补充实际model中的独立SIM门禁与退出SIM，不改旧断言。host89/89通过（原54保留）。真实输入provider与consumer调度/实机电量或断线时延尚未验证。

三构建 Default/Stable/Release {"default": "0x358db0", "stable": "0x357f90", "release": "0x34c370"}，均<5MiB；三个ELF真正链接ui_input_message_apply，Release原禁止符号无；boundary/doclinks/diff通过。日志 build/module-input-ui-state-{host-build,host,default,stable,release,symbols}.log。未提交、推送、烧录，无新硬件结果。

仍缺input_service生命周期/消息consumer、真实ATOM与模拟provider接入、动作/caps/UI导航全消息；main/atom_link仍直接UI查询/导航、Camera caps/action及旧Wi-Fi/维护调用。UI偏好与Wi-Fi菜单仍main；后续先移UI偏好worker并拆UART adapter，补UI_MENU_ACTION/偏好消息，再接Input service/provider，最后独占维护/UART/Core完整组装。完整计划继续active，见[清单](../development/module-split-checklist.md)。
