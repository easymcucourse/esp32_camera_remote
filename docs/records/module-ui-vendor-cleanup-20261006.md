# 2026-10-06 UI 原始厂商接口残留清理

生产审计发现ui_model.c仍定义app_ui_set_camera_property(uint16_t code)与app_ui_set_command_status(uint16_t code)，camera_settings.c仍定义camera_extra_codes的9个Sony property ID。当前生产调用方已全部使用ui_camera_messages的语义property/control处理，这三个旧符号仅用于host/legacy fixtures，无生产caller。

从ui_model.c和private头删除两个旧setter；函数体原样移到 `tests/support/ui_vendor_model_legacy.c`，只注册test_ui_model目标。原9个property ID移到 `tests/support/legacy/ui_camera_vendor_codes.h`，原camera_settings parser fixture及legacy UI Wi-Fi model包含它；生产camera_settings只保留当前显示formatter。没有新建运行兼容层，没有把Sony ID重新塞入app_message。当前ui_camera_messages更新模型的代码不变。

保留test_ui_model所有原value/frozen断言，以及原54中的camera_settings vendor parser/formatter断言；只调整fixture include/支持源。提取函数体LF SHA256记录 `build/module-ui-vendor-move.json`，内容未改。boundary新增禁止三个退休符号出现在生产C/头，防止旧API复活。

完整HOST **262/262**，原54保留，日志 `build/module-ui-vendor-host-build.log`、`build/module-ui-vendor-host.log`。LCD Default0x3586c0/Stable0x3578a0/Release0x34c690三构建终态0，archive边41/41/37通过；3ELF无退休UI符号，Release23禁符号无。结果见同名前缀日志和JSON；ATOM无相关改动，沿用此前两配置。没有烧录、实机、远端CI、提交或推送。完整计划仍在进行，保留的UI布局/枚举显示值没有改变；硬件验收等待设备确认。
