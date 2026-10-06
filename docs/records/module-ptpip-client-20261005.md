# 2026-10-05 PTP/IP 消息客户端基础

本批是阶段4.2的具体进展，未完成该条或完整计划。新增ptpip_client由独立fake Console测试验证；生产Camera/Sony/PTP仍用旧fd路径，新基类虽参与component编译，尚未接入生产调用。

- ptpip_client_t可被后续vendor backend内嵌，实例拥有双channel token、业务generation、网络generation、timeout、嵌套事务depth/绝对deadline、取消、最近错误/诊断/部分字节数。session/next_transaction字段仅预留，旧session/transaction尚未迁移，不能声称目前只有一套会话。
- 新实现只通过app_console的OPEN/SEND/RECEIVE/CLOSE请求，无app_wifi/lwIP依赖、无fd。标准packet/session/event poll/handshake和Sony组合仍待接入。旧ptpip component仍保留lwip和旧生产源，下一批必须替换并删除，而不是把两套长期并存。
- 传输使用同一个producer buffer lease，send readonly、receive writable；每个请求使用绝对deadline，并受嵌套事务最早deadline约束。RPC超时/取消后仍等待最后lease callback，确保不会提前复用stack/图像buffer。网络代数原子更新不能被晚到OPEN回复覆盖。
- router新增可取消RPC入口，保持原入口和bounded waiter；每25ms检查owner predicate，且回调在锁外运行，可非阻塞发送关闭控制消息。取消不强制撤销消费中的lease，迟到reply照常释放。
- Wi-Fi CLOSE可用token0+opening_correlation取消尚未收到token的OPEN，仍验证source+业务generation。OPEN回复被router拒绝时自动关闭已建立的孤儿通道。保留最近8个closed token/owner/generation，允许取消后的close幂等重试；其他owner不能使用这份记录。关闭历史有界，过旧/未知token仍拒绝。

主机68/68通过，原54保留。新PTP客户端测试仅链接真实ptpip_client/app_message和fake Console/时钟/task，不链接app_wifi/lwIP：双通道、不同实例、nested earliest deadline、只读/可写租约、partial timeout、错误short reply、late reply等待所有权、stop期间CLOSE、gen变化、close失败保留/retry、OPEN/gen race。真实router新增25ms取消/锁外重入/消费租约不撤销/迟到释放；真实Wi-Fi worker新增late OPEN清理、opening correlation取消和close owner幂等测试。此前失败是新增断言把“未到deadline但waiter已取消”的错误期望写成TIMEOUT，修正为现有路由INVALID_STATE后回归通过，未放宽原断言。

Default 0x355490 / Stable 0x354670 / Release 0x348aa0编译通过、均<5MiB；Release禁止符号不存在。日志build/module-ptpip-client-{host,host-build,default,stable,release,symbols}.log；boundary/doclinks/diff通过。fake调度器不证明真实RTOS或硬件行为；没有烧录、提交、推送。

下一步将ptp_session和Sony扩展改为client实例调用，迁移command/event初始化握手、标准session/transaction和事件poll，改Camera生产网络/发现/RSSI到message并删除旧裸fd transport。再继续输入/UI菜单/独占维护/backend/Camera拆分/UART/完整组合根和兼容清理。见 [完整清单](../development/module-split-checklist.md)。
