# 2026-10-06 UART UI 偏好消息与统一事件接收

`main/ui_preferences_console.c` 不再包含 UI 私有偏好、overlay 或 ATOM 协议头；读取、pad 保存及 info 修改只发送 UI_PREFERENCES。info 修改保留异步 queued token 与 DONE/FAIL 输出。UI_REQUEST 写设置 command.flag 时，UI 确认内部队列 admission 后回复 token，原内部 RAM worker 保存后发布结果；其他调用方仍按原 commit 后回复。该模式仅允许 UART REQUEST 的偏好写操作，GET/RESET/其他来源拒绝。

完成结果在 UART 拥塞时保留当前 worker 请求与容量预留，每 10ms 重试；UART/UI 生命周期关闭则退休，不对旧 UART 写配置或报告结果。UART 跟踪最多八个 token，同时核对来源、生命周期、类型和操作；重复/过期/不匹配结果不输出。main/camera_console.c 统一接收 UART endpoint 消息并分别转交 UI 与 SIM 格式化，最终统一归还消息，避免 SIM 私有 poll 丢弃 UI 完成结果。Release 也接收 UI 结果。SIM encoder 不再读取或释放 UART inbox。

新增 uart_preferences 主机测试，扩展现有 UI worker 回归测试，保留全部原断言：正常读取/pad 保存/info 排队/错误、容量、代数、错误操作、重复与不相关事件；worker admission ack、满 UART 重试、写前来源退休及结果重试期间退休。最终测试和固件尺寸见本批日志 `build/module-uart-preferences-{host-build,host,default,stable,release}.log`。本批未修改 ATOM 固件；未烧录、未验证 SMP/缓存关闭或实机稳定性。

编码器仍暂位于 main，UART 的独立生命周期与 app_console 整体迁移、Camera/status/bench 编码、UI 偏好旧兼容入口清理、完整停止编排及独占维护继续待完成。本记录不表示完整计划验收。

最终验证：主机 101/101；LCD Default `0x35b7b0` / Stable `0x35a990` / Release `0x34ebd0` 三构建成功，均小于 5 MiB。全部本批构建会话已成功结束。
