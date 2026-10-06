# 2026-10-06 Camera 通用 backend 生产主循环

生产camera_controller已经实际切换到camera_session/discovery/stream、默认私有Sony factory、PTP client消息TCP路径。删除旧fd、标准PTP/transaction、Sony初始化/属性/取景/控制、直接DHCP/RSSI调用及UI renderer/setter。生命周期/输入兼容API与endpoint暂在main，app_camera完整facade/component接管仍未完成，不宣称完整计划或阶段4完成。

- 发现使用DHCP typed snapshot，saved MAC过滤、串行TCP-only probe、关闭并销毁每个探测后才继续，唯一peer选择通过message确认。session独占backend，清理失败反复重试、保留handle和任务busy，不能覆盖/重连；旧socket/外部transaction/session-open计数不再存在于producer。
- complete backend connect执行唯一Sony初始化；仅成功后内部RAM identity worker CONFIRM，guid验证/InitFail IDENTITY要求显式用户动作、不自动重试。成对握手上限仍10s/120s；包含全部初始化的总deadline为50s/160s，backend每Sony协议请求仍5s。配对验证成功后300ms再次发现/完整连接验证，失败不自动循环验证；预览失败保持1/2/4/8/16/30秒重试，有已显示frame清backoff。
- producer使用原32768 PSRAM栈/core0/prio4与原2x1MiB对象buffer，stream是唯一参数/frame状态owner。intake只写guard保护的有界合并计数、control queue和latest MF request；caps读取同guard，IO不持锁。主循环真实使用generic property/settings/Mode实际回读屏障/MF/action/event/liveview；语义snapshot/state/status和JPEG全部通过Console。当前input menu选择仍读app_ui_settings_mode/menu_selected，待UI_STATE/Input迁移，不是完成UI边界。
- MF cancellation采用stream原子epoch，property IO返回后、MF写入前检查；producer从latest request保留原epoch/queued时间，避免“取消发生在队列检查与接受之间”把旧请求重新当新请求。参数写入前重新同步safety generation，释放引发的旧target取消。真实stream测试新增property read期间取消。
- 健康完整操作后停止，先用900ms剩余总预算释放控制，即使两个JPEG槽被租借；partial/cancelled操作直接cleanup。所有JPEG lease refs归还后才能释放buffers/context；同lifetime槽重用同时要求metadata与last-ref，跨lifetime teardown只要求last-ref，晚metadata因新generation失效，不含buffer指针。纠正此前记录把teardown误写为必须等metadata+last-ref；源码camera_frames_drained只验证refs。end在backend cleanup后清caps/可写状态。STOP确认仍等owner所有清理完成，未验证实机时间限。
- 维护临时session通知/关闭等待保持在main，完整Sony初始化完成后才通知并最多2秒等现行Web关闭，预览前必须关闭；最终独占无认证维护产品仍待阶段3替换。FORGET使用内部worker与typed network clear。状态last_io现在为generic backend结果枚举，不再PTP fd诊断枚举。

host84/84通过，原54保留。新测试直接编译真实生产camera_controller.c，连接真实session/discovery/stream/properties/controls/frame/lease，fake generic backend/Console/RTOS/NVS worker：验证初始化后才confirm、create/destroy单所有权、cleanup失败保留重试、双JPEG租借时先S2/S1释放再drain/free、InitFail不自动重试、配对300ms两次connect、load/allocate失败资源回收、state generation单调。fake backend不验证真实Sony设备/RTOS调度。原Sony/PTP fake wire测试继续通过。

Default/Stable/Release尺寸{"default": "0x358600", "stable": "0x3577e0", "release": "0x34bc70"}，三构建与5MiB门禁通过。三ELF实际链接camera_stream_tick/release、camera_session_open、camera_discovery_scan、camera_backend_sony_create和ptpip_client_request_data；旧initialize_sony/handshake/ptpip_connect/JPEG worker/identity worker不存在。Release模拟/编码器禁用符号无。旧menu Sony parser不再生产CMake编译，但原host fixture适配仍在main供tests用，待最终legacy支持目录清理。

日志build/module-camera-producer-{host-build,host,default,stable,release,symbols}.log；boundary guard新增生产controller禁止vendor/protocol/Wi-Fi/renderer调用；doclinks/diff通过，所有handles终态。新host测试最初因fake ops忘声明capabilities被validate拒绝并循环，补齐fake契约并添加模拟时间watchdog后84/84，不是生产故障；首个CTEST已Ctrl-C终态，无相关残留process。未烧录/提交/推送，无实机功能/FPS/停止时限/长稳结论。

下一步去掉main的UI menu读取/维护跨域直接依赖，将producer和endpoint一起迁入app_camera，建立core-only生命周期facade；接network-generation/RSSI缓存与typed UI_STATE/Input消息，删除原compat/旧Sony/PTP API并迁纯Sony解析，继续独占维护/UART/组合根。见[完整清单](../development/module-split-checklist.md)。
