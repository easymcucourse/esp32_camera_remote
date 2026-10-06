# 2026-10-06 Core 初始化失败排空与订阅冻结

生产组合根此前在 Input 创建失败后直接 quiesce router，未先等待已启动的 UI task 退出。本批在 System/UI/Input 组合失败时，依次等待 Input、preferences、UI owner；只有全部返回才关闭 System/router，任一失败则保留 router 供整体排空。每个局部等待 1000ms，保留原始初始化错误，不把 cleanup 结果冒充启动成功。

Camera/UART 组合不再容忍 UART allocation/subscription 失败后继续取景。UART 错误返回 Core，producer 不启动；正常 STARTUP 必须有已注册 UART 才冻结订阅，否则初始化失败。提前取得维护模式可以跳过全部普通服务，无需冻结不存在的 UART。此策略取代先前“正常 Camera 继续、冻结延后、UART 将来重试”的过渡实现。独立内部 Camera boot fixture 的失败重试仍验证不重复创建已存在的 Camera endpoint；顶层 Core start 不允许再次启动。

顶层失败先将模式置 RESTART、释放 boot 屏障，再关闭 HTTP 并尝试正常 owners 的不可逆停止；之后返回原错误，由 app_main fatal 处理。UI 初始化尚未尝试时没有正常 owners，跳过排空。面板、Wi-Fi/AP 对象与失败排空的资源仍保留至重启，不强删活动任务、不恢复 normal、不擦除 NVS。HTTP stop 使用 SDK 同步 join，不能宣称该步骤有严格 3000ms 上限；配置停止有其自身预算。

HOST 241/241：真实组合根 fixture 覆盖 UI/Input 创建失败与 Input/preferences/UI 排空失败，验证 IPU 顺序和 router 只在成功后关闭；真实 Camera fixture 验 UART 失败 producer 不启动、成功重试只创建缺失 owner；启动矩阵验证错误模式、屏障释放、HTTP-before-normal 排空及失败 cleanup 不覆盖原错误。原 54 基线保留。新增 fixture 首轮缺少 string.h 在 -Werror 下失败，补齐 include 后通过，未关闭告警。

LCD Default `0x358790`、Stable `0x357980`、Release `0x34c790`，三构建通过。日志 `build/module-startup-failure-{host-build,host,default,stable,release}.log`。ATOM 固件未改未重建；无烧录、实机、提交或推送。

本批完善 Core 部分初始化/失败策略，但公共 API、各 owner 的所有资源退出路径、依赖图、当前文档与完整 S/V/A38 审计仍待检查。尚未用实际 SMP、I²C/AP 启动或 cache-off 验证，完整 goal active。

最终 boundary/doclinks（135 文档、555 链接、0 问题）及 diff 检查通过。所有本批构建 handles 终态，无后台验证任务。
