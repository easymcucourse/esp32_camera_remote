# 2026-10-06 全消息编号契约核对

新增 `docs/design/module-message-contracts.md`，按当前46个消息编号逐项记录owner、R/E/control、payload、代际、lease和完成/错误语义，包括退休配置/factory/forget及未使用UI_STATE。所有操作共用的task/deadline/correlation/transport-vs-business/lease规则位于文件首部及公共头，不为各行重复完整API说明。

逐域读取真实publisher及consumer：Wi-Fi bridge/channel、Camera outputs/frames/discovery/endpoint/runtime、UI frame/model/menu/preferences/bench、Input owner/ATOM/SIM、Core poll/session。核对进一步纠正公共头中的代际概括：Camera向Wi-Fi/System发出的发现/PTP/session请求使用attempt/backend实例generation，不能统称为endpoint epoch；router另行记录请求两端endpoint寿命。WIFI_SELECT_CAMERA是R，而非不等待回复的control。UI可处理CAPABILITIES事件，但当前Camera producer使用PROPERTIES中的caps，没有独立caps事件调用。

机器记录 `build/module-message-contract-coverage.json`：enum46、table46，无遗漏/额外/重复。此检查只证明列项覆盖，不证明表的全部语义；语义依据上述源文件及当前hostfixtures。设计目录过时“维护登录”索引改为启动触发/无认证。

本批仅注释和文档改变，没有功能代码/测试逻辑修改。沿用上一批HOST261/261及三LCD构建、ATOM此前两配置；本批重新执行boundary、doc links与diff检查。未重复构建制造新runtime证据。实机消息调度/SMP/超时取消效果仍待验证，文档表明确不存在全handler统一源权限校验或自动业务回滚。

S1.10“在每个message contract注明规则”以共享envelope规则加46行操作差异证明文档交付；它不替代V14/V15/V29/A31等运行验收。没有烧录、实机、远端CI、提交或推送，完整目标保持进行中。
