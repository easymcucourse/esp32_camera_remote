# 2026-10-06 Sony exposure纯转发函数收尾

最终源码审计发现`sony_encode_set_exposure_mode`仅固定SONY_DPC_EXPOSURE_PROGRAM/type=6转发`sony_encode_set_scalar`，当前Sony backend的set已直接使用scalar encoder与通用setting映射。该函数无生产调用，三个wire/backend fixture依赖它；原体与声明移`tests/support/legacy/sony_exposure_encoder.*`，生产encoder source/header删此入口，原测试逻辑/断言保留。boundary新增生产禁止符号规则。不是删除曝光设置协议：同一scalar encoder/codes[setting]/原写字节仍生产使用。

原体SHA一致（build/module-sony-exposure-move.json）。最初漏了包含test_ptpip_protocol.c的test_sony_backend目标，host link失败；补test-only target_sources后完整262/262通过，原54保留，无降低告警。无生产调用的sony_parse_properties/sony_parse_scalar_properties仍有异常响应/抓包fixture校验语义，按计划测试保留规则不凭当前相机未触发删除。当前39生产global中37在Default ELF，另2为上述parser；47 static helpers已有源内调用/table清单。

ptp_codes.h 23、sony_codes.h 32共55常量均有定义头之外的source/test名称引用，去注释清单build/module-sony-retained-constants.json。没有按单一文本命中认定运行可达；当前8个生产源职责为client/wire/protocol/dataset和Sony backend/descriptor/liveview/control编码，结合此前40global/47static、factory/ops/callback审计与保留样本/原assert依据，未发现新的无依据生产操作。迁移删除/替代清单沿用Sony/PTP清理与控制合并记录；测试支持不进固件。

验证：LCD Default0x3586c0、Stable0x3578a0、Release0x34c690三构建终态0，均小于5MiB。三个实际Sony archive与ELF均无退休wrapper；Release23调试禁符号无，build/module-sony-exposure-symbols.json。Default实际archive直接41边门禁通过，build/module-sony-exposure-direct-symbols.json。完整host及固件日志同module-sony-exposure前缀。ATOM源未改，沿用最近Debug/Release编译结果；不是本批重建ATOM。

构建/主机测试通过，硬件待验证。删除/合并后的真实Sony冒烟是原计划明确要求，S4.13/A26仍部分。未收到设备/串口确认，无烧录、实机、remote CI、提交或推送。
