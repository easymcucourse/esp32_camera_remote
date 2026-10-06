# 2026-10-06 Input 对等报告契约与当前设计

新增test_input_equivalence，实际编译input_provider/input_owner/input_reports/gamepad_input，无功能门面。分别注册ATOM/SIM source，输入同一时间线并比较全部77个pad_action的type/value/generation；覆盖RT迟滞按压/释放、LT录像、肩键、Mode/Focus、菜单/确认/返回、Start、gap及offline。断言关键动作确实存在，避免空序列相等；正常化报告经public provider API复制进入真实owner，初始化source选择housekeeping不计入动作序列。它不是两个实际协议/硬件报告解码一致性的证明，仍需端到端实测。

重写输入设计前3节到sole Input owner/private kernels/provider registry、来源/epoch/report ID/取消与停止顺序；保留阈值、协议与日期历史范围。UART设计替换旧remote_common/main transport草案为实际app_console gateway、common parser、typed encoders、release及裁剪策略。UART生产help不再宣称ui info可保存或touchpad循环保存，改为ui info/ui pad查询并提示Web；行为仍只读。

清单S2.4/S2.7、V7/8/9、A8/9/10/33/38逐项补源码/主机证据，硬件项保持部分，不将实现证明扩大为SMP/输入时序或相机效果验收。两个private头的旧future service措辞另修为当前service注释，无实现变化。

HOST253/253原54保留。LCD Default0x358780、Stable0x357970、Release0x34c770均终态0，日志build/module-input-contract-{host-build,host,default,stable,release}.log。本批未涉及ATOM未重建，无烧录/实机/提交/推送，所有构建handles终态。boundary/doclinks143文档571链接0issues及diff通过（追加record后最终计数另见下方）。完整目标active；剩余设计/开发当前文档、全S/V/A38具体审计、最终五配置与硬件证明待完成。

追加记录后的最终doclinks144文档/572links/0issues，diff通过。末尾两个private头仅注释修正，三LCD构建结果覆盖本批实际help实现改动。
