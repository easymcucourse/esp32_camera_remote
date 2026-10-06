# 2026-10-06 当前UART脚本与日志说明收尾

当前工具扫描发现旧维护on/off/probe、手柄维护和普通UI偏好写入仍留在可回放目录；它们不能验证最终Web-only维护流程。将maint-web/wifi/restart/ota-upload/gamepad、pair-maint-gamepad、旧ui-info/ui-info-persistence/pair-ui-info共9个脚本移到 `tools/uart_scripts/legacy/`；旧display-bench另原样复制。每个移动/复制都核对前后SHA256，记录 `build/module-script-archive.json`。原历史命令保留，不删除验收证据或冒充新脚本。

当前display-bench删除旧maint交互，先通过S/S进入NORMAL并回全屏，避免无相机首帧时STARTUP preflight拒绝；保留二十帧/设置页/重复基准与SIM拒绝流程。新增当前ui-info只查询及检查旧写命令拒绝，不假定保存档位为full。两份显示恢复脚本改为匹配真实Core日志 `Restart: maintenance_closed=1 normal_stopped=1; NVS preserved`；旧camera_drained字符串已不由生产源打印。

当前serial-log文档删除不存在的AP READY/client列表、INIT ACK/InitFail/vendor参数/断线日志必达说明，使用语义status及仍存在的session/liveview日志。用户serial说明补基准NORMAL前置与待实机；tools目录说明legacy边界。display-profile历史记录的完整路径指向归档原脚本，其旧硬件结论保持当时范围。

实际Runner.execute以dry subclass检查11个当前脚本语法/设备名/时限/ASCII命令，未打开UART，不模拟ACK或证明expect能匹配实机。记录 `build/module-current-script-syntax.json`。五项uart_script/preferences/bench/gateway Debug/Release host通过；之前完整262仍有效。本批不改生产C行为，不重复固件构建。

剩余：11脚本新固件实机回放；新维护只能通过启动页HTTP操作，不能以旧UART脚本替代。其他当前文档与全部S/V/A验收继续按范围核对。无烧录、实机、远端CI、提交或推送；串口设备确认仍待用户回复。
