# 2026-10-06 Sony / PTP 生产源码收口

本批只整理生产模块与测试支持路径，不改变协议算法和原回归断言。Sony 属性解析、取景校验、控制编码及 client controls 四个源文件迁入 `components/camera_backend_sony/`，五个相关头归其 `private/`；移除独立 sony_camera 的 CMake 注册。后端只私有依赖 camera_backend/ptpip，不公开 Sony 头。

旧 Sony fd API sony_ext.c/.h、PTP fd session/send_data/transport 和两个头共七个文件移入 tests/support/legacy。生产 ptpip 只编译消息 client/protocol、唯一标准 wire engine 与 dataset；移除 lwIP/log 构建依赖。旧 fixtures 仍编译这些测试支持源，并共享同一 wire/encoder，不删除或降低原断言。16 个文件移动时逐个验证 SHA256；后续仅三个测试支持源的 include 相对路径调整，生产 Sony 纯逻辑内容哈希仍一致。

边界检查覆盖全部生产 PTP 源/头，拒绝旧 fd/Sony 头与实现、独立 sony_camera、PTP/Sony 后端 CMake 的 lwIP/旧组件依赖。源码与三 ELF 核对：真实 Sony backend、message client、descriptor parser、JPEG extractor 已链接，旧 ptpip_connect/ptp_transaction/sony_set_prop_u8/u16/u32 无生产符号；Release 原模拟器/编码器禁止符号无。

主机85/85通过（原54项保留），Default/Stable/Release 尺寸 {"default": "0x358cc0", "stable": "0x357ea0", "release": "0x34c280"}，全部小于5MiB。日志 build/module-sony-ptp-cleanup-{host-build,host,default,stable,release,symbols}.log，移动清单 build/module-sony-ptp-source-move.json。

空目录删除被自动审批拦截，未给更具体原因；保留空的旧 Sony 目录，无源码/CMake 注册，不影响固件。没有绕过拒绝。未提交、推送、烧录，无新增实机或稳定性结论。

完整计划仍未完成：Input/provider/sim、UART、启动独占维护、Core 组合根及旧调用方兼容 API 尚待迁移。S4.5/S4.13 的全部 wrapper/死代码审计仍待逐项完成，不能把本批生产 fd API 收口等同于整个阶段完成。见[完整清单](../development/module-split-checklist.md)。

## 移动清单

- `components/sony_camera/sony_props.c` → `components/camera_backend_sony/sony_props.c`
- `components/sony_camera/sony_liveview.c` → `components/camera_backend_sony/sony_liveview.c`
- `components/sony_camera/sony_control_encoder.c` → `components/camera_backend_sony/sony_control_encoder.c`
- `components/sony_camera/sony_client_controls.c` → `components/camera_backend_sony/sony_client_controls.c`
- `components/sony_camera/include/sony_codes.h` → `components/camera_backend_sony/private/sony_codes.h`
- `components/sony_camera/include/sony_props.h` → `components/camera_backend_sony/private/sony_props.h`
- `components/sony_camera/include/sony_liveview.h` → `components/camera_backend_sony/private/sony_liveview.h`
- `components/sony_camera/include/sony_client_controls.h` → `components/camera_backend_sony/private/sony_client_controls.h`
- `components/sony_camera/private/sony_control_encoder.h` → `components/camera_backend_sony/private/sony_control_encoder.h`
- `components/sony_camera/sony_ext.c` → `tests/support/legacy/sony_ext.c`
- `components/sony_camera/include/sony_ext.h` → `tests/support/legacy/sony_ext.h`
- `components/ptpip/ptp_session.c` → `tests/support/legacy/ptp_session.c`
- `components/ptpip/ptp_send_data.c` → `tests/support/legacy/ptp_send_data.c`
- `components/ptpip/ptpip_transport.c` → `tests/support/legacy/ptpip_transport.c`
- `components/ptpip/include/ptp_session.h` → `tests/support/legacy/ptp_session.h`
- `components/ptpip/include/ptpip_transport.h` → `tests/support/legacy/ptpip_transport.h`
