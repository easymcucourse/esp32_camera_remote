# PTP/Sony全局函数使用清单

[English](../en/development/module-camera-symbol-usage.md) · **简体中文** · [日本語](../ja/development/module-camera-symbol-usage.md)

> 2026-10-10：下方为有日期的详细台账，旧路径/件数/未完成项按当时范围解释；最新源码与验证以 [当前状态](../development/current-status.md)为准。当前main启动栈24576字节，旧32768表已被取代；Host基线267、四个新构建通过，未烧录。


采集2026-10-06，最新Sony exposure纯转发函数移入test-only后，依据当前Default ELF与两个真实component archives。39个生产global函数中37个存在ELF，2个只有fixture依据。旧47函数快照保留在build/module-camera-export-usage.json，合并批40函数机器快照build/module-sony-control-merge-export-usage.json保留历史；本次移出项与归属见build/module-sony-exposure-move.json和module-sony-exposure-symbols.json。static helper/常量源码核对已补，真实smoke仍待完成。

| 符号 | 当前Default ELF | fixture来源 |
| --- | --- | --- |
| `camera_backend_sony_create` | 存在 | `tests/host/test_sony_backend.c` |
| `ptp_parse_device_info` | 存在 | `tests/host/test_camera_parsers.c` |
| `ptp_wire_decode_event` | 存在 | `tests/support/legacy/ptp_session.c` |
| `ptp_wire_initialization_exchange` | 存在 | `tests/support/legacy/ptp_send_data.c` |
| `ptp_wire_operation` | 存在 | `tests/support/legacy/ptp_session.c` |
| `ptp_wire_receive_packet` | 存在 | `tests/support/legacy/ptp_session.c` |
| `ptp_wire_request_data_result` | 存在 | `tests/support/legacy/ptp_session.c` |
| `ptp_wire_send_data` | 存在 | `tests/support/legacy/ptp_send_data.c` |
| `ptpip_client_cancel` | 存在 | `tests/host/test_ptpip_client.c` |
| `ptpip_client_cleanup_begin` | 存在 | `tests/host/test_ptpip_client.c` |
| `ptpip_client_close` | 存在 | `tests/host/test_ptpip_client.c` |
| `ptpip_client_init` | 存在 | `tests/host/test_ptpip_client.c`, `tests/host/test_ptpip_protocol.c` |
| `ptpip_client_initialize_command` | 存在 | `tests/host/test_ptpip_protocol.c` |
| `ptpip_client_initialize_event` | 存在 | `tests/host/test_ptpip_protocol.c` |
| `ptpip_client_network_changed` | 存在 | `tests/host/test_ptpip_client.c` |
| `ptpip_client_next_event` | 存在 | `tests/host/test_ptpip_protocol.c` |
| `ptpip_client_open` | 存在 | `tests/host/test_ptpip_client.c` |
| `ptpip_client_operation` | 存在 | `tests/host/test_ptpip_protocol.c` |
| `ptpip_client_poll` | 存在 | — |
| `ptpip_client_receive_packet` | 存在 | — |
| `ptpip_client_request_data` | 存在 | `tests/host/test_ptpip_protocol.c` |
| `ptpip_client_scope_begin` | 存在 | `tests/host/test_ptpip_client.c` |
| `ptpip_client_send_data` | 存在 | `tests/host/test_ptpip_protocol.c` |
| `ptpip_client_timeout_set` | 存在 | `tests/host/test_ptpip_client.c`, `tests/host/test_ptpip_protocol.c` |
| `ptpip_client_transaction_begin` | 存在 | `tests/host/test_ptpip_client.c` |
| `ptpip_client_transaction_end` | 存在 | `tests/host/test_ptpip_client.c` |
| `ptpip_client_transfer` | 存在 | `tests/host/test_ptpip_client.c` |
| `sony_descriptor_choice` | 存在 | `tests/host/test_sony_descriptors.c`, `tests/support/legacy/camera_menu_sony_legacy.c` |
| `sony_encode_manual_focus_step` | 存在 | `tests/host/test_ptpip_protocol.c`, `tests/support/legacy/sony_ext.c` |
| `sony_encode_movie_record` | 存在 | `tests/host/test_ptpip_protocol.c`, `tests/support/legacy/sony_ext.c` |
| `sony_encode_set_scalar` | 存在 | `tests/host/test_ptpip_protocol.c`, `tests/support/legacy/sony_ext.c` |
| `sony_encode_setting_step` | 存在 | `tests/host/test_ptpip_protocol.c`, `tests/support/legacy/sony_ext.c` |
| `sony_encode_shutter_button` | 存在 | `tests/host/test_ptpip_protocol.c`, `tests/support/legacy/sony_ext.c` |
| `sony_encode_zoom` | 存在 | `tests/host/test_ptpip_protocol.c`, `tests/support/legacy/sony_ext.c` |
| `sony_liveview_parse` | 存在 | `tests/host/test_sony_liveview.c`, `tests/support/legacy/liveview_pipeline.c` |
| `sony_parse_descriptors` | 存在 | `tests/host/test_sony_descriptors.c`, `tests/support/legacy/camera_menu_sony_legacy.c` |
| `sony_parse_focus_caps` | 存在 | `tests/host/test_focus_caps.c`, `tests/host/test_property_fixtures.c` |
| `sony_parse_properties` | 无，需fixture依据 | `tests/host/test_camera_parsers.c`, `tests/host/test_property_fixtures.c`, `tests/host/test_sony_descriptors.c` |
| `sony_parse_scalar_properties` | 无，需fixture依据 | `tests/host/test_camera_settings.c`, `tests/host/test_property_fixtures.c` |

补充：四个无source/test引用的PTP声明已删除，47个static helper定义均有本源调用或ops/table引用。它们的文本清单仅用于定位，不冒充运行可达或真实smoke，见[本批记录](../records/module-unused-declarations-20261006.md)。

本次移出生产的`sony_encode_set_exposure_mode`只固定property/type转发scalar encoder；原函数体与声明移tests/support/legacy/sony_exposure_encoder.*，只三个fixture目标编译，既有backend直接scalar路径保留。两组55个保留常量均有定义头之外的source/test引用，机器清单build/module-sony-retained-constants.json；引用定位不等于运行分支/样本语义证明。见[本批记录](../records/module-sony-exposure-cleanup-20261006.md)。
