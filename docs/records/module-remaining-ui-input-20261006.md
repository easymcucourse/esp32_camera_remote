# 2026-10-06 剩余 Wi-Fi 信息与输入释放证据

生产`ui_menu_messages.c`在SETTINGS网络行selected=7保持HANDLED，无Wi-Fi编辑器。`test_ui_menu_messages.c`直接编译当前menu/model，现在对Input和UART两种来源逐一验证CONFIRM、STEP、BACK、RELEASE，保持settings/selected且wifi_menu/extra_menu=false。原导航/参数/错误断言保留。该目标无Wi-Fi writer链接，历史编辑器目标不作生产路径证明。

`tests/host/CMakeLists.txt`显示`test_ui_wifi_menu`和`test_wifi_menu_current`均编译`tests/support/legacy`里的旧编辑器；后者输出中误称Production改为Legacy/test-only，未修改原逻辑或断言。当前普通Wi-Fi写拒绝及Web保存后重启依据[module-wifi-readonly](module-wifi-readonly-20261006.md)，真实Web在ACK成功后config_commit，Core等待完成再重启；不以历史AP热应用测试代替设备重启验收。

真实input_owner/provider/reports/gamepad等价回放中，ATOM/SIM两来源分别在gap前保持RT与gap按钮，新增断言首动作RELEASE_ALL、次动作MF_CANCEL且value=0，gap窗口不发新普通动作；offline也检查完整释放与MF取消在前。既有同timeline全动作逐项等价断言保留。test_input_reports另覆盖来源切换release失败/MF失败时阻止新按下、重试后held baseline；test_input_owner覆盖unregister、新handle、overflow、过期backlog；test_input_service覆盖下游timeout/失代/过期reply与UI cancel重试。下游Camera是fake，不证明相机物理对焦/zoom/录像释放。

验证：host build及262/262 CTest terminal0，原54保留，日志build/module-remaining-ui-input-host-build.log与build/module-remaining-ui-input-host.log。只改主机测试/说明/清单，生产固件源未改，沿用UI vendor批LCD三构建及API批ATOM两构建。实体ATOM断线、UART时序、AP配置/设备重启、Sony动作与SMP仍待实机；无烧录/提交/推送/remote CI。V36/V38部分证实，不作完整计划验收。
