# 工具

[English](../en/tools/README.md) · **简体中文** · [日本語](../ja/tools/README.md)

只写仓库根目录 `tools/` 里脚本的用法。实测数据和协议结论在 [记录](../records/README.md)，排错步骤在 [使用手册](../user-guide/troubleshooting.md)。

| 文档 | 脚本 |
| --- | --- |
| [抓包工具](capture.md) | `capture.ps1`、`analyze.py`、`extract_liveview_sample.py`、`extract_property_sample.py` |

编译脚本 `idf.ps1` 和串口脚本 `serial_log.py` 的说明在 [开发文档](../development/README.md)。

UART回放只使用 `tools/uart_scripts/` 当前脚本；`legacy/` 保留已退休维护/偏好流程的原内容供历史证据对照，不用于新固件。显示基准脚本先经设置页进入NORMAL；UI偏好脚本只查询并确认旧写命令被拒绝。当前脚本还需新固件实机回放，语法检查不代表硬件PASS。

文档门禁：`python tools/check_doc_links.py`检查本地链接；`python tools/check_doc_locales.py`检查45当前主题三语覆盖与英/中/日顺序，不能代替语义内容审查。
