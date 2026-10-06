# 2026-10-06 正常 UI 偏好只读化

按拆分计划3.9/S3.6，正常应用配置由维护Web保存、下次启动加载。生产ui_preferences.c改为启动读取preferences_store_load并缓存info/controller，不再创建3072内部栈worker、请求/结果队列或写锁。加载失败/不支持schema仍使用完整显示与DS默认值，不改写原记录；重复start保持当前boot缓存。关闭只清除ready许可，缓存原子值保留供正在停止的输入/UI读取。

UI偏好message仅接受GET，普通INFO_SET/NEXT/PAD_SET/RESET全部NOT_SUPPORTED；UART仅保留ui info/ui pad查询，设置参数不会发送请求。Input不再转发手柄INFO_NEXT持久化动作，其他相机/菜单/安全操作不变。共用preferences_store_set单键写primitive无生产调用方，连同旧实现移至legacy测试fixture；维护继续使用schema1整体write/reset。app_ui删除直接nvs_flash依赖，边界门禁禁止正常UI/Input/Console调用偏好写接口。

旧UI worker/停止及UART异步token测试保留原断言，源迁至tests/support/legacy；新增production只读fixture链接真实新UI/UART代码，只有loader stub，没有存储写函数或任务API。测试启动缓存、GET、全部操作拒绝、UART写参数无RPC、关闭拒绝、错误加载默认；Input fixture新增INFO动作不增加send计数。HOST228/228通过，原54保留。三LCD构建及门禁结果见本轮日志build/module-preferences-readonly-*；当前未烧录或实机验证。

这不是整个Web-only配置阶段的完成：正常Wi-Fi UART/LCD菜单/消息写接口仍待移除；A35启动顺序、UI模型清空与完整计划审计未完成。ATOM未涉及，未重建；没有提交或推送。

最终LCD Default0x35a530、Stable0x359720、Release0x34e570全终态0；三个ELF保留shared load/write，旧request/set_pad/result/event/store_set符号不存在。边界/doclinks/diff通过：130文档548本地链接0问题。所有本批构建handle终态，无本批后台任务。
