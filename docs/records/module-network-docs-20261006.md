# 2026-10-06 网络设计与使用手册同步

重写wifi-ap-design.md，按真实network_config codec、wifi_saved_config、wifi_apply、Core startup及Web handler核对格式/加载/后端自动修复/config tokens/回滚与重启。删除已退休的wifi_ap_*兼容API、菜单worker/编辑器、正常factory与UART写入口描述；维护权限、启动页入口和不可逆行为对应最终计划。

重要证据范围更正：wifi_saved_read仍在无效/过大/不可读blob路径调用wifi_saved_write(defaults)，删除cfg并commit，保留原有后端错误恢复。Core boot没有直接writer，国家范围无效记录只RAM fallback，但不能由该stub测试推出整个启动无NVS写入。上一批过宽说明在记录追加更正，architecture/resource表及本地记忆同步；未因文档承诺删除既有修复行为。真实后端测试test_wifi_saved_config覆盖修复成功与erase/commit失败。

quick-start增加启动页Web操作/无认证权限/成功重启/OTA文件范围；controller/camera/troubleshooting去除旧编辑与factory/u步骤，README项目结构改组件化并删除PIN与旧reset入口。源码还证实ATOM有BLE客户端及Ultimate 2解析，修正旧“未实现BLE”的笼统断言；没有把源码支持推广为全部手柄或当前固件实测。

纯文档修改，无代码行为变化、烧录、硬件验证、提交或推送。后续仍需其他当前设计与开发文档、逐项S/V/A38源/测试证明、最终五配置构建及硬件待验证记录。完整目标active。

验证：文档链接143文档/577links/0issues，diff通过；重新执行真实后端wifi_saved_config及Core boot七例，共8/8通过。无源码变化，沿用最近252全host及三LCD构建证据；未宣称本批全构建重跑。gamepad/UART设计仅修正相关入口及链接，其他旧分层/线程表仍待逐段同步。
