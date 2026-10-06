# 2026-10-06 UI 偏好持久化 / message

生产ui_preferences持久化迁components/app_ui，private头只供UI；main/ui_preferences_console.c保留原UART解析/结果文本，main/ui_preferences.h为尚未删除的旧UART/维护compat声明，没有转发private头。UI实现不再包含debug_console/ATOM协议/link，不直接调用UART输出或provider唤醒。app_ui_messages_start拥有偏好启动，仍在ATOM前；main去掉偏好启动和低层头。

INFO/PAD/RESET写入统一经原ui_preferences内部RAM3072/prio2 worker，队列4/旧完成队列8保持。启动读取和NVS ui_prefs/info、pad键/0..2、0..1格式不改；部分队列、mutex、task创建失败清理可重试，不重复启动worker。INFO next按worker处理时最新level循环，保存成功才更新UI原子值。原UART pad/旧维护pad setter同步等待内部worker（借用完成context直到Give，之后worker不再访问），不在HTTP/UI调用者栈写NVS。原UART INFO结果文本/排队token保持。请求metadata变大，不增加任务/栈/全屏缓冲。

已有UI_PREFERENCES契约补GET/INFO_SET/NEXT/PAD_SET/RESET；UI endpoint GET只读，写命令入同一worker，REQUEST在NVS完成后回复，lease-free旧请求元数据保存，不把排队接受冒作commit。非REQUEST发送UI结果event。来源/flags/lease/value/deadline校验；worker写前检查绝对deadline和UI endpoint epoch，过期/停止后的旧作业不写NVS。RESET只System允许，排队起拒绝新修改，失败解除fence，成功直到reboot保持fence。恢复出厂main callback发System→UI request并等待完成；旧无调用reset wrapper删除。输入Touch/INFO改UI typed命令。

Pad改变后UI不直接调用atom_link_wake；旧ATOM等待分<=25ms片检查持久化pad缓存，不改变50ms通信周期或原1/5秒退避总预算，改型后重新HELLO/释放保持。原ATOM还直接读此缓存，这是待provider全消息化删除的compat，不称完整输入已接入。

异步token纯逻辑迁common/async_token.c，LCD common_runtime独立底层编译单元，UI/Wi-Fi/UART共用唯一计数器，不使通用Wi-Fi依赖Console业务。debug_async_token与app_wifi_next_token转用同一原算法（非零、原wrap语义保持）。ATOM独立工程main编译同一个纯C源，计数器按设备隔离。UI host实际交错generic/Wi-Fi/UI token，防重号；所有原fixture断言保持。

host90/90（原54保留）；新增真实UI worker测试：部分创建fail/retry资源、原3072/prio2、load/ordered next/queue full/结果、所有NVS写在worker、同步PAD失败保持状态、message deferred直到commit、过期/epoch stale不写、non-request event、System-only reset失败恢复/成功fence、跨域token。fake调度/critical并非SMP/cache-off或实机证明。

构建尺寸 {"default": "0x359120", "stable": "0x358300", "release": "0x34c6e0", "atom-debug": "0x1042a0", "atom-release": "0x101690"}。三LCD均<5MiB；三LCD与ATOM Debug真实链接共享token和UI/UART或ATOM链路；ATOM Release没有异步模拟作业，未使用token函数被裁剪，Release各禁止符号无。最初m5_atom_matrix/build失败：旧本地Classic-only sdkconfig触发BLE guard；未更改本地配置，转既有build/ci-atom-debug和release双模配置（BTDM/BLE/GATTC）均通过。失败和成功分别保留build/module-ui-preferences-atom.log及-atom-{debug,release}.log；其他日志build/module-ui-preferences-{host-build,host,default,stable,release,symbols}.log。

boundary/doclinks/diff通过；所有handles终态，无提交/推送/烧录。完整计划仍未完成：UI偏好quiesce/stop、UART/维护旧兼容调用、Wi-Fi菜单、input service/provider生产接入/动作/caps/UI导航、独占维护和Core组合根。下一步UI_MENU_ACTION/偏好GET用于替换Input/UI直接查询，Wi-Fi菜单迁UI并只发typed request；随后Input整体provider与UART gateway。见[全清单](../development/module-split-checklist.md)。
