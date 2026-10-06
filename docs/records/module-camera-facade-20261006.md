# 2026-10-06 Camera component / Core 生命周期

生产producer和endpoint真实迁至app_camera/camera_runtime.c、camera_endpoint.c，main不再编译Camera业务源，不依赖PTP/Sony/backend或Camera私有头。app_camera.h只含init/messages_start/start/quiesce/messages_stop，配置仅DEFAULT枚举，无backend/任务/缓冲/可修改快照；只有Core直接包含此facade。原main/camera_pair.h兼容声明仍供未迁移Input/UART/维护/bench调用，函数实现在app_camera private；这部分旁路及完整组合根仍待后续阶段删除，不能称完整计划完成。

- Core app_core_camera_boot拥有Camera init/endpoint/原UART启动/自动preview顺序；旧UART/正常维护实现由冻结的启动ops临时绑定，endpoint创建失败可重试且不重复init。Camera不再包含UI/维护头，正常维护准备改为SYSTEM_CAMERA_SESSION request，Core验证Camera source/flags/generation/deadline并等待旧Web关闭<=2秒，后续完整独占维护会删除此过渡产品策略。关闭通知为不等待回复的direct command，避免Core quiesce等待Camera时互相等候。health Camera drain调用Core quiesce，内部RAM health属性不变。
- 原INPUT源在atom_link_start注册{8,1}，菜单步进当场捕获UI选中项并发SETTING_ADJUST semantic property/direction/safety generation；Camera再不读取UI选中行，legacy MENU_STEP拒绝未指定语义属性的命令。Input整体/provider/UI navigation/状态仍main且有直接UI/Camera compat，是剩余阶段5工作，不把注册source当成完整Input endpoint。
- Camera endpoint订阅WIFI_NETWORK_CHANGED，校验Wi-Fi来源/非零网络generation；原子通知和取消predicate终止旧attempt，owner IO返回后调用backend.network_changed、保留ownership清理再按原backoff新attempt。网络与Camera业务generation分别保存；网络变更后未再confirm旧identity。producer host新增properties读中网络变更→取消→cleanup→fresh attempt成功，不自动置用户STOP。
- app_camera_init校验配置/重复/队列创建；messages_start单次，quiesce保留排空reservation拒绝新start，messages_stop在drain后停止router endpoint并等接收task退出，再释放metadata队列。pending STOP与收到的metadata均不携JPEG lease；不撤销渲染引用。原producer32768PSRAM/core0/prio4、endpoint4096内部prio4、单槽MF/2x1MiB保持。
- 恢复出厂身份清除使用System→Camera FORGET request，flag=true只允许System REQUEST并验证已有maintenance_gate+busy reservation且worker inactive；身份NVS仍经内部worker，保留reservation直到原协调者释放。普通UART forget不能借此旁路gate。main去掉低层camera_identity头；原Sony菜单fixture适配移tests/support/legacy，测试原断言保留。

host85/85通过，原54保留。真实runtime/endpoint源码host路径同步，不再需要main/UI/维护头；producer测试新增facade init/start/quiesce/stop、network取消重试、reserved forget边界；endpoint测试新增subscribe/来源/network/停止队列回收；Core policy fake lifecycle测试配置非法、部分创建失败重试/一次init-console、来源/超时/关闭通知/固定2秒策略。不是RTOS调度/实机稳定性证明。

三构建尺寸{"default": "0x358cc0", "stable": "0x357ea0", "release": "0x34c280"}，均<5MiB；三ELF真正链接public Camera lifecycle、Core boot、完整session/discovery/Sony/stream/identity/frame路径；旧camera_pair_console_init/handshake/initialize_sony/ptpip_connect/worker无符号，Release模拟/编码器禁用符号无。messages_stop未被正常boot调用，可能由链接器裁剪；host有停止源码测试，完整独占维护/router quiesce仍待集成。日志build/module-camera-facade-{host-build,host,default,stable,release,symbols}.log。

boundary/doclinks/diff通过；所有handles终态，未烧录/提交/推送，无新硬件功能/时限/FPS/长稳结论。下一步迁纯Sony解析并删旧PTP/fd/compat，输入/provider/sim与Camera caps/action/status/UI navigation全消息化；随后独占维护/UART与Core完整组合根/固定订阅/quiesce编排，按[完整清单](../development/module-split-checklist.md)验收。
