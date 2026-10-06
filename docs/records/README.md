# 记录

按时间保留的抓包分析和实机测试。文中的“当前状态”是当时的状态，最新行为以 [相机连接设计的当前实现](../design/sony-ptpip-design.md#当前实现连接与运行)、根目录 README 和 [系统架构](../design/architecture-design.md) 为准。

| 文档 | 内容 |
| --- | --- |
| [2026-10-06 暂停与阶段提交](pause-checkpoint-20261006.md) | 保存模块迁移与通信修复；取景拒绝、LCD显示异常和稳定性未完成 |
| [2026-10-06 通信修复实测](communication-recovery-test-20261006.md) | 启动内存、消息锁、输入时序和菜单截止修复；最后在线长测失败 |
| [2026-10-06 正常 Wi-Fi 只读](module-wifi-readonly-20261006.md) | 编辑入口/worker删除、Web配置完成后设备重启、231项主机与三LCD构建 |
| [2026-10-06 正常 UI 偏好只读化](module-preferences-readonly-20261006.md) | 启动缓存、Web-only持久化、旧worker回归保留与228项主机验证 |
| [2026-10-06 Web 恢复出厂](module-web-factory-20261006.md) | scopes/确认/失败与回执丢失重启、正常入口删除、227 项主机与三 LCD 构建 |
| [2026-10-06 配对身份共享存储](module-identity-store-20261006.md) | 唯一 NVS primitive、原格式/内部栈保留、204 项主机与三 LCD 构建 |
| [2026-10-06 UART 相机与状态消息](module-uart-camera-20261006.md) | 控制编码、并行 request、快照错误及 extra 语义参数 |
| [2026-10-06 UART UI 偏好消息](module-uart-preferences-20261006.md) | 异步 admission/token、完成结果重试与统一 UART inbox 接收 |
| [2026-10-06 UART Wi-Fi 消息编码](module-uart-wifi-20261006.md) | 配置事务、确认/超时/代数与密码结果；100 项主机及三 LCD 构建 |
| [2026-10-05 消息总线基础](module-router-20261005.md) | typed contract、fake endpoints、期限/代数/lease 与 System/UI 初始接入 |
| [2026-10-05 main 模块拆分](module-split-20261005.md) | 任务基线、显示租约、UI / health 拆分与构建回归 |
| [2026-10-03 显示阶段计时](display-profile-test-20261003.md) | 全屏黑边清理、设置页阶段定位、合成图基准与分配失败修正 |
| [2026-10-03 双分区 OTA](ota-test-20261003.md) | 48 项测试、完整镜像失败 / 重试、双向更新及确认前回退 / 确认后保持 |
| [2026-10-03 维护安全重启](maint-restart-test-20261003.md) | 47 项测试、21 请求网页重启与维护占用 / KEEP 状态下 LCD 故障重启 |
| [2026-10-03 维护手柄入口](maint-gamepad-test-20261003.md) | 46 项测试、连接页长按、菜单双确认 / 取消及真实 I²C 回归 |
| [2026-10-03 网页热点设置](maint-wifi-test-20261003.md) | 45 项测试、38 个 HTTP 请求、热点重启 / PIN / 旧令牌与配置恢复 |
| [2026-10-03 维护网页基础](maint-web-test-20261003.md) | 43 项测试、HTTP 回环 / 退出、启动栈回收与显示 / I²C 回归 |
| [2026-10-03 CI 与可移植构建](ci-test-20261003.md) | 42 项测试、四项构建配置、文档链接门禁与远端验证边界 |
| [2026-10-03 三档信息显示](ui-info-test-20261003.md) | 41 项测试、触摸板 / gap / 真实 I²C 与重启持久化 |
| [2026-10-03 默认密码标签](wifi-default-label-test-20261003.md) | DEFAULT 状态、隐藏 / 自定义转换及原配置恢复 |
| [2026-10-03 LCD 本地模拟](lcd-local-sim-test-20261003.md) | 40 项测试、本地协议 / 输入 / 故障回归与物理轮询暂停恢复 |
| [2026-10-03 Matrix 调试](matrix-debug-test-20261003.md) | 39 项测试、四角校准 / 独立故障覆盖及串口回归 |
| [2026-10-03 ATOM 原始 I²C 请求](i2c-req-test-20261003.md) | 38 项测试、原始协议帧 / CRC / 参数拒绝及真实链路回归 |
| [2026-10-03 ATOM I²C 故障注入](i2c-fault-test-20261003.md) | 37 项测试、CRC / 丢响应 / 延时、三次失败断开与自动恢复 |
| [2026-10-03 双端 I²C 监视回归](i2c-monitor-test-20261003.md) | 36 项测试、双端烧录、变化 / 全量日志与每端 100 条状态压力测试 |
| [2026-10-03 ATOM 手柄模拟回归](pad-sim-test-20261003.md) | 35 项测试、双端烧录、真实 I²C 模拟输入 / 溢出保护及 50 次计时 |
| [2026-10-03 显示恢复与 UART 回归](display-uart-test-20261003.md) | 33 项主机测试、双端烧录、回调丢失发现 / 修正与离线实机复测 |
| [2026-10-02 Wi-Fi 配置回归](wifi-test-20261002.md) | 配置校验、异步重启、NVS 跨复位保存、热点重置与验证边界 |
| [2026-10-02 设置菜单回归](menu-test-20261002.md) | 七项参数、方向键、23 项测试、烧录与验证边界 |
| [通信分析与实测记录](protocol-analysis.md) | Sony 初始化顺序、属性格式、配对和取景优化各轮数据 |
| [2026-10-01 抓包与设计对照](protocol-analysis-20261001.md) | 三轮抓包、S1/S2/录像/MF、截图参数、取景拒绝及 JPEG 边界证据 |
| [2026-10-02 I²C v2 与 Matrix 回归](i2c-v2-test-20261002.md) | 新从机启动、DS4 高频输入下 CRC 故障及修正复测 |
| [2026-10-01 烧录与连接测试](connection-test-20261001.md) | 固件校验、启动、停止／恢复、重复启动与 DHCP 前置条件 |

抓包命令见 [抓包工具](../tools/capture.md)。原始 `.pcapng` 和未脱敏日志不提交，范围见 [通信记录的公开范围](../README.md#通信记录的公开范围)。

- [Wi-Fi 接口与驱动第一批迁移](module-wifi-20261005.md)

- [Wi-Fi NVS 边界与初始化迁移](module-wifi-store-20261005.md)

- [恢复出厂协调迁入 app_core](module-core-factory-20261005.md)

- [Wi-Fi异步配置worker/token迁移](module-wifi-jobs-20261005.md)

- [Wi-Fi TCP通道与消息桥](module-wifi-tcp-20261005.md)

- [PTP/IP消息客户端基础](module-ptpip-client-20261005.md)

- [标准PTP数据事务接入消息实例](module-ptpip-protocol-20261005.md)

- [Sony数据发送收口到PTP基类](module-ptpip-send-20261005.md)

- [PTP/IP初始化握手接入实例](module-ptpip-init-20261005.md)

- [消息事件通道轮询](module-ptpip-events-20261005.md)

- [Sony控制编码接入PTP实例](module-sony-client-20261005.md)

- [通用Camera backend契约](module-camera-backend-20261005.md)

- [Sony backend组合实例](module-sony-backend-20261005.md)

- [Camera通用属性快照与参数状态机](module-camera-properties-20261005.md)

- [Camera私有组件与会话所有者](module-camera-session-20261005.md)

- [Camera消息发现与通用Probe](module-camera-discovery-20261005.md)

- [Camera / UI 语义消息](module-camera-ui-20261005.md)

- [Camera身份、动作队列与通用控制](module-camera-controls-20261005.md)

- [生产JPEG lease消息路径](module-camera-frames-20261005.md)

- [Camera参数执行、控制同步与身份Worker](module-camera-execute-20261006.md)

- [Camera会话执行运行器](module-camera-stream-20261006.md)

- [Camera独立消息接收](module-camera-endpoint-20261006.md)

- [Camera通用backend生产主循环](module-camera-producer-20261006.md)

- [Camera component与Core生命周期](module-camera-facade-20261006.md)

- [Sony/PTP 生产源码收口](module-sony-ptp-cleanup-20261006.md)

- [输入状态机与 report 仲裁内核](module-input-reports-20261006.md)

- [统一输入 provider API 与 report owner](module-input-provider-20261006.md)

- [输入状态到 UI 消息](module-input-ui-state-20261006.md)

- [UI 偏好持久化与消息](module-ui-preferences-20261006.md)

- [UI 菜单消息与语义参数](module-ui-menu-20261006.md)

- [Wi-Fi 菜单归 UI 与消息配置](module-ui-wifi-menu-20261006.md)

- [统一输入服务接入生产](module-input-service-20261006.md)

- [独立 ATOM/SIM provider 与资源停止](module-input-providers-20261006.md)

- [显示基准归 UI 与消息预约](module-ui-bench-20261006.md)

- [独立 UART gateway](module-uart-gateway-20261006.md)

- [UI 偏好任务停止与资源归还](module-ui-preferences-stop-20261006.md)

- [UI Wi-Fi 菜单停止](module-ui-menu-stop-20261006.md)

- [UI 消息端点排空](module-ui-endpoint-stop-20261006.md)

- [渲染停止与固定维护画面](module-ui-render-stop-20261006.md)

- [Core 配置任务与正常网络排空](module-core-network-stop-20261006.md)

- [Camera/System 端点及路由器停止](module-core-router-stop-20261006.md)

- [Core 正常启动组合根](module-core-start-20261006.md)

- [HTTP SoftAP监听隔离](module-http-scope-20261006.md)

- [独立维护OTA与Core启动健康检查](module-maintenance-ota-20261006.md)

- [独立无认证维护Web与共享偏好存储](module-maintenance-web-20261006.md)

- [Core 原子模式与 UI 正常许可](module-core-mode-20261006.md)

- [独立维护 HTTP trigger lifecycle](module-maintenance-trigger-20261006.md)

- [Core 独占维护接入与旧控制器删除](module-core-maintenance-20261006.md)

- [UI 死页面清理与维护 model 清空](module-ui-clear-20261006.md)

- [Core 初始化屏障与提前 trigger](module-boot-barrier-20261006.md)

- [启动页 trigger 与正常服务启动顺序](module-startup-order-20261006.md)

- [Core 初始化失败排空与订阅冻结](module-startup-failure-20261006.md)

- [Core 私有契约与 Camera 版本检查](module-core-private-20261006.md)

- [UI 私有实现接口与 Camera 死兼容清理](module-ui-private-20261006.md)

- [实际依赖与运行关系图](module-dependency-graph-20261006.md)

- [模块符号归属与资源所有权](module-symbol-owner-20261006.md)

- [当前架构与维护设计同步](module-current-design-20261006.md)

- [网络设计与使用手册同步](module-network-docs-20261006.md)

- [Input 对等报告契约与当前设计](module-input-contract-20261006.md)

- [UI设计与测试覆盖核对](module-ui-docs-20261006.md)

- [真实UI JPEG renderer故障矩阵](module-jpeg-renderer-20261006.md)

- [Sony/PTP当前契约与文档核对](module-sony-contract-audit-20261006.md)

- [JPEG renderer连续模式切换](module-ui-switch-20261006.md)

- [Wi-Fi信息入口与输入释放证据](module-remaining-ui-input-20261006.md)

- [间接ops/callback绑定与运行图核对](module-indirect-callbacks-20261006.md)

- [Sony曝光纯转发函数收尾](module-sony-exposure-cleanup-20261006.md)

- [最终软件验收范围核对](module-final-software-audit-20261006.md)

- [当前操作文档收尾](module-current-guides-20261006.md)

- [真实Wi-Fi ESP32 factory契约](module-wifi-backend-contract-20261006.md)
