# 2026-10-06 当前操作文档收尾

当前user-guide、tools与构建文档逐项对照生产源和脚本。发现并修正：troubleshooting开头、三条相机身份恢复建议及结尾仍要求已删除的UART u/手柄热点reset；改启动页Web全部重置并说明网络/偏好一起重置。camera手册“LCD不显示手柄电量”改真实LIVE/SETTINGS电量及绿黄红灰规则。controller“枚举到边界停住”改生产camera_stream使用wrap=true的首尾循环及EV右增左减。serial说明s的OK只是接收、fault需SIM和底层fault支持、当前ui-info只读脚本。quick-start/serial把COM8/6明确为历史示例，须确认实际端口。

build-and-flash旧52项改当前262/原54；补cJSON主机Web依赖与SDK source path，Windows完整MinGW configure示例已在现有同generator目录实际执行成功，build/module-current-guide-configure.log。CMakeCache当前MODULE_CJSON_SOURCE_DIR与示例一致，Linuxlibcjson-dev对应CI安装步骤。只configure不构建/串口/烧录，本批无生产修改，最近262与三LCD沿用Sony exposure批。

准确backtick项目路径扫描当前README/guide/tool/design/request/development（排除module批次文档与已标历史implementation-status），61引用：58存在；3例外已分类，两个云台源/test为明确未实现未来设计，Sony旧display.h草案位于已明确替代的历史1–14节。build/module-current-guide-path-audit.json保留逐行索引与分类。不是把文字例子扫描当作全部需求语义证明，也不改历史设计为虚构当前文件。

相关源码证据：Camera stream menu_step(...,true)、UI renderer电量与颜色、实际camera_commands/type消息、UI/network信息菜单、Core/Web恢复出厂；未知手柄/协议历史效果不推断为当前实机。troubleshooting旧InitFail/Camera disconnected必达日志改Camera语义状态；停止示例Camera task finished和LIVEVIEW日志实际仍存在。抓包脚本历史输入路径已明确依赖不随仓库提供的原始capture，不下载/公开原数据。

模块边界、167文档615链接0问题及diff检查通过（新增本记录前）；最终检查见本批输出。文档实现范围可以独立验收，真实UART脚本/浏览器/显示/恢复/重置、启动安全/新Sony/ATOM以及SMP稳定性仍待设备，完整计划不标完成。无烧录、remote CI、提交或推送。
