# 2026-10-06 Core 私有契约与 Camera 版本检查

按计划组合根职责与最小接口要求，Core 唯一公共头 `app_core.h` 只保留 API_VERSION 与 app_core_start。健康回调、路由/System/UI 启动、网络绑定、provider prepare/start/stop、Camera/UART 编排等内部声明归 private/app_core_services.h；重启预约/commit 与 factory 纯事务头也收回 private。生产只有 Core 包含这些内部头；主机 fixture 使用指定私有 include 路径，无旧路径转发头。历史 factory worker/stop 回归只修改编译 include 路径，断言未改。

Camera 生命周期增加 APP_CAMERA_API_VERSION 与 app_camera_api_version。Core 在创建 Camera owner 前验证实际链接版本；不匹配返回 NOT_SUPPORTED，不创建 Camera/endpoint/UART/producer。与既有 Wi-Fi 对象版本/能力检查一起满足可替换接口的版本入口。其他公共契约仍在审计，不能把这一个检查当作全工程版本/生命周期证明。

新增边界门禁：Core 公共目录只允许 app_core.h，公共函数只允许 start；其他 component 不得包含内部服务、factory 或 restart 头。进度文档更新当前生产状态，将历史迁移顺序单独标记；验收清单更新 Core、UI冻结、Web-only配置与 A35 的证据，硬件要求仍标为部分。

HOST 241/241（原 54 保留），Camera fixture 覆盖版本不匹配零创建。首轮旧 factory worker/stop fixture 的 include 路径遗漏，已补齐 private 路径；未删测试或放松告警。LCD Default `0x3587a0`、Stable `0x357980`、Release `0x34c7a0`，三构建终态 0，均小于 5MiB。日志 `build/module-core-private-{host-build,host,default,stable,release}.log`。ATOM 固件未涉及未重建，无烧录、实机、提交或推送。

完整计划仍 active。下一步 Camera 死兼容生命周期/forget 路径、UI 公共实现 API、其他公开/private 契约与资源退出全面审计；然后具体 compile/runtime 图、当前文档和全 S/V/A38 验证。真实硬件启动、安全切换与稳定性仍未证实。

三实际 compile graph Core/Camera owner 唯一，ELF 均有 app_camera_api_version，证据 build/module-core-private-graph.log。新增边界脚本首次缩进错误已修复，再跑完整 241 项通过。最终 boundary/doclinks（136 文档、556 链接、0 问题）及 diff 通过，所有构建 handles 终态。
