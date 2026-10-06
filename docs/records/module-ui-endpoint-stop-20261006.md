# 2026-10-06 UI 消息端点排空

Core新增 app_ui_messages_quiesce，用于Input/Camera producer与UI bench/menu/preferences已停止后的消息worker关闭。停止期不执行普通UI/偏好/菜单操作，请求回复INVALID_STATE；JPEG不解码，产生相同token/generation的frame-result并归还原lease。完成metadata控制队列满时继续保留两个原slot/worker/endpoint重试，metadata发出或生命周期退休后才停止UI endpoint并自删task。超时不purge正在渲染的lease、不强删worker，后续quiesce继续等待。原32768 PSRAM/core1/prio4、control16/bulk2与正常JPEG路径保持。

ui_frames_drop只生成结果metadata，incoming JPEG lease仍归caller释放；ui_frames_pending提供UI owner自己的排空条件，没有跨任务读取共享结果队列。端点task唯一owner处理drops/flush/release；Core只通过atomic started/closing与有界等待协调。Camera producer排空时UI继续处理帧与completion，Core在排空后关闭UI。Core重启仅在UART/Input/providers/Camera/bench/menu/preferences都完成前置停止时才调用UI消息停止；当前重启策略超时仍最终重启，不是最终不可逆维护成功证明。

UI端点创建/订阅失败会关闭自己拥有的endpoint并有界停止已创建menu/preferences，失败清理未完成则下一次start先重试，不覆盖活跃worker。冻结前建立固定订阅；UI单独restart复用已有frozen订阅，router重新启动且尚未freeze时重新建立。连接页refresh与renderer尚未停止，完整UI生命周期和固定MAINTENANCE锁仍缺。

新增ui_endpoint_stop fixture，保留原54注册和旧frame回归断言。覆盖task创建失败/cleanup重试、frozen订阅restart、关闭阶段排队JPEG/普通请求拒绝、completion满保留与超时、渲染中关闭后处理剩余lease、重复stop/restart。扩展真实ui_frames回归验证drop不渲染、只保留metadata且JPEG caller只归还一次。主机112/112通过，-Werror保持；fake不是RTOS/SMP/cache-off或实机证明。

日志build/module-ui-endpoint-stop-{host-build,host,default,stable,release}.log，最终构建补充见下方。ATOM本批未改未重建；无烧录/实机/提交/推送。完整计划active，下一步连接页refresh/renderer停止与固定维护锁，Core全局资源编排/启动SoftAP:80无认证独占维护、legacy API全清理。

最终LCD Default `0x35b740` / Stable `0x35a920` / Release `0x34f690` 构建成功，均<5MiB，三ELF含UI stop/drop生产路径；boundary/docs/diff通过（117文档/516链接/0问题）。所有本批构建会话终态，无本批后台串口/构建；其他聊天未检查。
