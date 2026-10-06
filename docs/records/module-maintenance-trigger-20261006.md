# 2026-10-06 独立维护 HTTP trigger lifecycle

新增 app_maintenance.h 与 maintenance_trigger.c：Core 单次注入 request_exclusive/request_reboot，init 验证 app_wifi version/AP 与 Web 相同对象；trigger_open 复用原 Web HTTP 服务器，phase 只能 IDLE→TRIGGER→ACTIVATING→ACTIVE 或 CLOSED，关闭/失败后不可重开。activate 只允许 Core 在 ACTIVATING 调用；重复初始化/open/activate 拒绝。没有新增任务、队列或正常功能依赖。公开头使用 app_wifi，因此其 CMake 依赖改为公开 REQUIRES，而不是仅私有传播。

14条既有完整 Web 路由全部通过统一 dispatch；另注册 SDK 404/405 error handler，因此已知路径、未知路径与错误方法首请求都走同一 trigger gate。首请求只调用注入的独占申请（35000ms budget）；只有 Core 返回成功且已调用 activate，才响应302、Location:/，重载后才执行完整路由。首个 settings/OTA POST 不执行原操作，也不读取 body。切换中或闭锁请求409；返回成功却未激活会闭锁并通过回调要求重启。Core 拒绝可能意味着正常模式胜出，不由维护应用擅自重启；Core 在取得独占后失败必须自己安排重启。

HTTP 首次响应设置 no-store/Connection:close，dispatch/error handler 随后返回 ESP_FAIL 让 SDK 关闭连接。检查本机未修改 ESP-IDF 5.5.1 httpd_sess/httpd_parse/httpd_uri 源码后确认：仅加 Connection:close 仍会清空未读请求体；返回失败跳过 request delete/purge，避免首个大 OTA POST 占住同一个服务器任务。Core callback 等待期间不能停止 HTTP 任务，激活仅切 gate；正常切换必须由外部 Core close admission 并 stop 物理 listener 后才许可 UI。stop 唤醒客户端再等待原 httpd_stop，失败保留服务器 owner/闭锁状态。

当前还不是生产启动触发：Core 尚未调用新 init/open/activate/stop，旧 Main controller 仍通过 Web start 控制服务器。未初始化 lifecycle 的兼容 gate 暂放行旧 Web，必须随旧 controller 删除；不能把生产 gate 可达或 fake callback activate 当作真正完成全模块停止。Core 原子 mode、固定 LCD、配置 worker restart 已有前批接口，但仍需一起绑定；启动先后次序、factory Web、旧 compat/WHOLE_ARCHIVE 和所有直接 UI 路径最终审计未完成。

179/179 主机测试通过，原54与旧断言保留。现有独立 Web fixture 加真实 trigger 实现、真实 SDK cJSON；10个新增场景包含已知POST/未知404/错误405首次302且不执行原操作、拒绝、关闭、申请中重入、切换中关闭、缺少activate、服务器start/错误handler注册/stop失败。确认未激活没有完整操作、激活后可正常info、旧404/405语义保留、单HTTP start、停止后不可重开和failed-stop owner保留。SDK HTTP/Wi-Fi/Core callback均fake，不证明网络实机/跨核调度/真实切换顺序。host头新增 user_ctx 仅模拟 SDK dispatcher。

最终 LCD Default0x35c090、Stable0x35b270、Release0x3500d0构建成功，均<5MiB/6MiB分区；compile graph trigger/Web/helper唯一owner，实际 trigger/Web对象没有 Core/UI/Input/Console/Camera/旧WiFi/mode引用，生产 gate 符号可达。Release逐项检查无SIM/bench/encoder/debugfault/维护UART符号。init/open/activate尚无生产调用，相关符号回收属预期，不宣称运行时接入。日志 build/module-maintenance-trigger-{host-build,host,default,stable,release,objects}.log。ATOM 未涉及本批独立维护源码，未重建。

本批构建会话全部终态，无本批后台串口/构建；其他聊天未检查。未烧录、实机、提交或推送，SMP/cache-off/HTTP实际可达性/视觉/稳定性待验证。完整目标 active，下一步 Core 全局不可逆停止与新 lifecycle 生产接入，再删除旧 maint_mode/adapter/WHOLE_ARCHIVE。

最终边界/文档/diff门禁通过：126文档、540本地链接、0问题。
