# 2026-10-10 代码整理与三语文档核对

用户目标：代码整理后先提交推送；文档按当前源码修正，再增加英语与日语，入口顺序英/中/日，最后再次提交推送。此次未进行新的烧录、配对更换或未完成云台功能实现。

## 代码阶段

提交 `11266fe13336bf42cf97daf5635f42fa4df2ea7c` 已推送，origin/main核对一致。包括RS3控制、协议/TX门禁、共享BLE扫描、故障传播、独立Pan/Tilt、取景配置/分析工具、测试及协议参考许可。统一I²C feature/fault常量，修正独立速度串口帮助。

实际验证：Host267/267；全新cleanup20261010 LCD/ATOM Debug/Release四构建通过，LCD实际依赖/直接symbol owner、5MiB预算、Release禁SIM符号通过。日志在ignored `build/cleanup-*`。构建完成并不表示已烧录；实机结果仍属于此前镜像。没有运行中的硬件采集。

## 文档阶段

- 根与ATOM README以英文为主，提供中文与日文入口。
- 当前45主题（使用、开发、需求、设计、工具）均有英/中/日对应页，`docs/locales.json`定义完整范围和顺序；英文/日文主题按当前实现整理，详细日期台账与历史证据链接至原文。
- 历史实测保持中文原稿和日期，增加英/日索引。旧PIN/可逆维护和旧改善清单归档，当前要求改为启动Web无认证、排他维护、保存/退出后重启。
- 核对源码后纠正：Input service忽略触摸板info-next，不提供手柄档位切换；显示偏好Web保存后加载。POWER_ZOOM是用户声明；RS3已绑定不自动换机，多候选不选；heartbeat当前端点4与参考raw E5分开；native center不代表任意零位/软限位。
- 保留267主机/四构建与实机验收的边界，约14min不宣称30min，取景最终性能未达，新LCD故障显示未烧录。
- 链接工具覆盖root/ATOM三语README；增加language coverage/order gate至CI。新增回归验证漏译路径、漏列主题、错误顺序及非零失败退出；检查器不证明语义翻译质量。

## 验证与边界

实际执行 `python tools/check_doc_links.py`、`python tools/check_doc_locales.py`、`python tests/host/test_doc_links.py`、`python tools/check_module_boundaries.py` 和 `git diff --check`。最终文档全部本地链接、45主题三语范围/顺序通过；文档工具3个回归通过，模块边界通过。第二阶段只增加文档及其检查工具/CI，不需要重新把固件构建当成新增实机证据。

未请求新的实机测试，未清理三个managed worktree、ignored抓包/日志/备份，memory保持本机Git ignored。远端Actions与branch protection本轮未核验。完整硬件待办仍见 [当前状态](../development/current-status.md)，语言入口见 [文档索引](../README.md)。
