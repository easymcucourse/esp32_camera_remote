# 2026-10-06 显示基准归 UI 与消息预约

显示基准执行迁入 app_ui/ui_bench.c，UART main/display_bench.c 只编码请求、查询 token 和格式化完成事件。沿用原 32768 字节 PSRAM / CPU1 / 优先级4任务、512 KiB 对齐 JPEG 缓冲、20 帧和相同 JPEG renderer/画布刷新路径；没有新增全屏 framebuffer。合成标记及连接页恢复由 UI 管理。

UI 通过 Input、SIM 和 System 消息复核前置条件，并向 Camera 获取独占显示预约。Camera 禁止新启动并等待生产 owner/帧 lease 排空，接收任务仍能处理 frame-result；请求超时不撤销预约，UI 必须用同一 token 清理。排空迟到时先完成排空再恢复原运行相机。Core 重启先取消基准，有界等待超时保留 worker；普通 RPC 可取消，归还预约 RPC 独立清理。UART admission 超时保留 token 并查实际结果，队列拥塞时完成事件保留重试；生命周期退休拒绝迟到结果。完整 UI/Core 停止编排仍待落实。

System mode 从旧占位改为 Core 注入过渡维护策略的实时快照；这取代前批关于占位的当前状态描述，尚不等于最终 STARTUP/ACTIVATING/MAINT 不可逆状态机。SIM 仅新增 UI 的只读 STATUS 权限，控制仍限 UART。故障命令及处理、基准 worker/编码器、Camera 基准预约辅助函数均 Debug 条件编译；生产 help/status/控制保留。

主机 108/108 通过，原54注册与断言保留。新增 UI/UART 基准、Camera Debug endpoint/producer、Release 命令回归，覆盖创建/分配/渲染失败、超时预约清理、延迟排空恢复、取消保留资源、生命周期退休、结果拥塞与重复 token。第一次新增 producer 场景复用计数器导致旧 actions==4 断言失败；仅重置新场景计数器后通过，失败证据保留 module-ui-bench-host-fixture-failure.log。fake 调度不证明 SMP/cache-off/实机效果。

最终 LCD Default `0x35ce40` / Stable `0x35c020` / Release `0x34efb0` 构建成功，均小于5 MiB。Release compile_commands 无 display_bench/ui_bench/SIM/lcd_sim，ELF 无相关基准、SIM、故障注入符号。日志 build/module-ui-bench-{host-build,host,default,stable,release}.log 与 module-ui-bench-release-gates.json；全部本批构建会话终态成功。ATOM 未修改未重建，前批双模证据保留。未提交、推送、烧录或实机验证。

编码器仍在 main，独立 UART gateway、旧维护串口/探针清理、Core 组合根与全生命周期、启动网页独占维护仍未完成。
