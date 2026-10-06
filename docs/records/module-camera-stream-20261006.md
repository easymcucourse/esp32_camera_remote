# 2026-10-06 Camera 会话执行运行器

本批把generic properties、controls、settings执行、frame leases和semantic UI outputs串成私有camera_stream；它借用session backend，由调用者提供原两块object buffers和时钟，不增加任务/对象缓冲，不复制PTP协议或session状态。仍未接入生产producer，完整计划未完成。

- begin保留guard和安全代数，完整初始化每会话参数/frame状态；message合并Mode/menu步进与latest-only MF，代数变化取消旧target，MF Cancel取消待发步进。owner tick先控制再events/property刷新、最多一项参数写入、Mode实际回读屏障、MF写前确认能力/新鲜度，最后preview发布。属性定期5秒刷新，pending参数/录像缩短到500ms，MF可取消且不会延后执行过期步骤。
- 两个JPEG槽都被UI租借时，直接S1/S2/快门安全释放仍可调用backend；依赖新属性的录像/变焦按下保留队列等待scratch。queued release取消尚未执行按下、保留并释放physical latch；不撤销UI租借来抢buffer。
- stop先停止接收新动作/清目标，healthy-boundary release不需要JPEG scratch，多个释放共用一份timeout预算；超时保留未释放latch供caller cleanup策略处理。修复S2释放后用“任一latch存在”比较导致S1漏释放的问题，改为逐项比较。end等待metadata和last-ref都归还，再清session/caps/可写状态；backend健康/cleanup由最终session owner负责。
- preview完整NOT_READY保持50次/100ms健康重试，其他REFUSED立即返回；坏对象进入既有十次damage阈值；队列丢帧保持neutral/租借回收规则。UI snapshot/命令状态通过现有输出，不调用UI实现。

host82/82通过，原54保留；新增camera_stream以fake generic backend和真实frame/lease/outputs测试双槽占用时控制/stop drain、S2/S1完整释放、共享900ms预算及部分释放超时保留、Mode屏障/实际回读、MF取消/新鲜能力、50次暂未就绪/拒绝分类/十坏帧。它不链接Sony/PTP/Wi-Fi/UI实现。Default/Stable/Release均构建通过，尺寸{"default": "0x356310", "stable": "0x3554f0", "release": "0x3499e0"}，均小于5MiB；生产identity/frame路径真实链接，旧worker与Release禁用符号不存在。新stream可能由链接器移除，不将archive编译宣称为生产使用。

日志build/module-camera-stream-{host-build,host,default,stable,release,symbols}.log；boundary/doclinks/diff检查通过。未烧录/提交/推送，无新实机FPS、停止时限或长期稳定性结论。

下一步接完整Camera endpoint/facade/producer，STOP在IO等待期间独立接收/取消，真正复用session/discovery/identity/stream，然后删除旧fd/PTP/Wi-Fi/UI直接路径；继续输入/provider/sim、独占维护、UART与组合根/compat清理。见[完整清单](../development/module-split-checklist.md)。
