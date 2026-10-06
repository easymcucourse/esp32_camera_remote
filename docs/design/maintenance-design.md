# 维护页面设计

本文按2026-10-06当前源码描述独立维护 Web。根目录 [拆分计划](../../main-module-split-plan.md) 的启动页 trigger、无认证和不可逆独占要求取代旧 PIN/登录、串口/手柄入口与原地恢复正常模式设计。需求来源见 [维护需求](../request/maintenance-request.md)，其中旧认证条目须按新计划解释；历史实现证据仍保留在日期记录中。

## 1. 模式与所有权

Core 唯一拥有 STARTUP/NORMAL/ACTIVATING/MAINTENANCE/RESTART。UI 正常许可与 HTTP 维护请求竞争同一个原子模式；普通绘制进入 NORMAL 前必须关闭 trigger admission 并由外部 Core owner 同步停止 HTTP。首个路由或404/405请求在 TRIGGER 阶段请求独占，等待 Core 完成后返回302到 `/`；ACTIVE 后才执行完整页面/API。其他阶段返回409。不是到相机建立会话才关闭 HTTP。

Core 先建立 UI、同步 ATOM I²C device，再启动 AP/config和端口80 trigger，之后才建立普通 router/Input/providers/Camera/UART。HTTP 先 claim 时等待 boot 屏障，不能关闭正在创建的对象。request_exclusive共用35秒绝对期限等待 startup结束及激活；失败转关闭/重启，不能回正常模式。详细顺序见 [系统架构](architecture-design.md)。

独占排空由Core health执行：关闭普通 admission，等待 UART/Input/providers/benchmark、Camera/网络/UI/renderer，最后 System/router；清空冻结 normal model并发布固定 `MAINTENANCE`，启动维护配置 owner，激活同一个 HTTP server。Web/OTA只依赖通用Wi-Fi和注入 ops，不包含Core私有头，不直接调用Camera/Input/UI/Console。成功后退出、设置修改、恢复出厂和OTA都通过重启生效，没有idle TTL或原地恢复。

所有热点客户端拥有完整维护权限，页面与 `/api/info` 明示 `authentication:none`；无PIN、token、session或login。仅监听SoftAP，通过SDK HTTP各translation unit的bind adapter绑定AP地址，普通TCP/PTP channel不覆盖bind。STA监听隔离实机尚待验证。任务/端口/队列和停止范围见 [资源所有权表](module-resource-ownership.md)。

## 2. 分区表

当前双 OTA 分区表为：

| 名称 | 类型 | 子类型 | 偏移 | 大小 | 说明 |
| --- | --- | --- | --- | --- | --- |
| `nvs` | data | nvs | `0x9000` | `0x6000`（24KiB） | 位置和大小不变，保留配对身份和热点配置 |
| `otadata` | data | ota | `0xF000` | `0x2000` | 记录启动分区和回退状态 |
| `phy_init` | data | phy | `0x11000` | `0x1000` | 从 `0xF000` 后移 |
| `ota_0` | app | ota_0 | `0x20000` | `0x600000`（6MiB） | |
| `ota_1` | app | ota_1 | `0x620000` | `0x600000`（6MiB） | |
| `data` | data | spiffs | `0xC20000` | `0x3E0000` | 当前未使用，比原来小 64KiB |

```text
# Name, Type, SubType, Offset, Size, Flags
nvs, data, nvs, 0x9000, 0x6000,
otadata, data, ota, 0xf000, 0x2000,
phy_init, data, phy, 0x11000, 0x1000,
ota_0, app, ota_0, 0x20000, 6M,
ota_1, app, ota_1, 0x620000, 6M,
data, data, spiffs, 0xc20000, 0x3e0000,
```

- 应用分区从 12MiB 缩小到 6MiB。当前固件内嵌三套裁剪字体，实际大小需用 `idf.py size` 确认；CI 中检查镜像不超过 5MiB，留出余量。
- 改表后第一次必须通过 USB 执行 `idf.py flash`（写入新分区表、`otadata` 和 `ota_0`）。`nvs` 位置不变，配对身份和热点配置保留；`phy_init` 移动后 PHY 校准数据在首次启动时重新生成。
- 之后的固件可以通过 OTA 或 USB 更新。USB 烧录 `idf.py flash` 会写入 `ota_0` 并擦除 `otadata`，因此总是从 `ota_0` 启动。
- 同步更新 [编译与烧录](../development/build-and-flash.md) 中的分区说明和 [故障排查与恢复](../user-guide/troubleshooting.md) 中的擦除地址。

## 3. 模块划分

| owner | 实现 | 职责 |
| --- | --- | --- |
| app_maintenance | maintenance_trigger.c | 一次初始化、TRIGGER/ACTIVATING/ACTIVE/CLOSED gate；复用原 HTTP task，无额外控制任务 |
| app_maintenance | maintenance_web.c、maint_json.c、maint_wifi.c | 页面、15 routes、严格JSON字段解析、维护配置请求 |
| app_maintenance | maintenance_ota.c、ota_header.c | prefix校验、单上传gate、流式Flash写入、验证及切换启动分区 |
| app_maintenance | web/index.html | 内置网页：无登录；热点/偏好/工厂恢复/OTA/重启 |
| app_core | app_core_maintenance.c、app_core_shutdown.c | 原子独占、排空、注入存储/重启ops、配置token完成跟踪 |
| app_core | app_core_ota_health.c | pending镜像健康确认与失败回退，使用已有health task |
| wifi_esp32 / common_runtime | 配置jobs/共享存储primitives | Wi-Fi record、UI prefs、Sony identity的唯一保存实现 |

## 4. HTTP 配置与路由

HTTP端口80、priority3、6144-byte内部栈、SDK默认未绑定CPU、最多3 sockets、LRU purge、recv/send各10秒，max_uri_handlers=15。外部Core停止时先关Web admission，再通过SDK session-close请求关闭客户端并同步join，不直接操作lwIP socket。session close只排队，阻塞handler仍可能等I/O timeout；JSON分段接收后检查停止，OTA由Core shutdown callback在接收间隙取消。项目层3000ms参数不能保证SDK join严格有界。没有旧maint_ctl或认证任务。

| method / route | ACTIVE 行为 |
| --- | --- |
| GET / | 内置维护页面 |
| GET /api/info | 版本/build/分区、运行信息与无认证声明 |
| GET/POST /api/controller | 偏好查询/保存的兼容route，与settings同handler |
| GET/POST /api/settings | controller/info等级查询/保存；保存后重启加载 |
| GET/POST /api/wifi | 热点当前配置、token结果查询；修改经staged prepare/回执/commit |
| GET /api/wifi/random_password | 生成符合热点规则的随机密码，不直接保存 |
| POST /api/maint/exit、/api/reboot | 预约重启，无原地正常恢复 |
| POST /api/factory | 显式scope/confirm恢复出厂并重启 |
| POST /api/ota/check | 预检288-byte prefix及X-Image-Size |
| POST /api/ota | application/octet-stream完整LCD固件上传 |
| GET /api/ota/status | 收包/验证/完成/失败进度快照 |

上表GET/POST各计一个handler，共15。TRIGGER时所有已注册路由先通过统一gate，尚不执行业务handler；404/405错误handler也经过同一gate，未注册路径及不匹配method可触发独占；ACTIVE后才返回404/405。Controller route保留兼容命名，无额外保存实现。

## 5. 配置、偏好与恢复出厂

正常UART/LCD只读配置，旧编辑器、偏好writer及factory/forget入口已删。Web热点修改先预约重启，再staged prepare；成功发送回执后commit并将token交Core。health追踪pending，成功才提交重启，失败取消预约；不依赖客户端继续查询。回执发送失败取消stage、不提交配置。`reboot_after_apply`表达成功应用后重启，不能把AP delay当成严格完整重启时限。字段/100-byte record校验沿用 [Wi-Fi设计](wifi-ap-design.md)。

UI偏好由common_runtime存储，正常启动加载。Web保存后1500ms重启，已保存成功则回执丢失仍重启；不调用停止后的UI endpoint。

POST /api/factory只接受 `{"scope":"wifi"|"all","confirm":true}` 两个字段；拒绝重复/额外字段/缺少确认。Core确认MAINTENANCE且无upload后冻结config worker，读取最新saved记录，使用HTTP内部栈调用共享存储primitive。wifi范围保存默认热点；all另清Sony配对及重置ui_prefs为DS/完整，不清ATOM绑定。成功保持冻结，回执丢失也1500ms重启；失败取消重启预约、恢复维护配置admission，并仅尝试Wi-Fi回滚。跨namespace没有原子事务，其他记录可能部分改变；不恢复normal owners。详见 [Web恢复证据](../records/module-web-factory-20261006.md)。

## 6. OTA 与启动健康确认

只更新LCD，ATOM仍由USB更新。check请求为288-byte prefix，Content-Type为application/octet-stream，X-Image-Size为十进制完整镜像长度；检查ESP32-S3 chip、project、大小及app header，返回编译时间比较信息。upload重新检查header，不把预检结果当授权。目标为非当前运行OTA分区。

单上传原子gate及Core upload_begin/restart_prepare防止并行维护写入。4096-byte PSRAM chunk在HTTP内部栈执行esp_ota_write，完整读取后esp_ota_end验证，再set_boot_partition。成功free buffer/upload_end，1500ms提交重启，最终回执丢失也重启。失败abort未完成handle、释放buffer、cancel重启预约并保存failed进度；不会把未校验镜像设为启动分区。SDK返回错误、断流和掉电恢复仍需实机验收。

Core启动检查pending状态，已有health task每秒检查heap与AP。ready后60秒确认有效镜像；heap损坏、60秒AP未就绪或确认失败请求SDK invalid rollback/reboot。显示致命失败由原health排空/重启，未确认镜像交bootloader回退。它不要求相机在线，不新增OTA health task。源码/主机证据见 [维护OTA记录](../records/module-maintenance-ota-20261006.md)。

## 7. 验证范围

当前源码、主机fixtures、三种LCD构建、真实依赖/符号门禁已覆盖主要生命周期与错误路径；不等同于Flash/cache-off/SMP、真实LCD固定画面、STA隔离、OTA掉电回退或长时间稳定性验收。当前证据与未完成项目见 [完整清单](../development/module-split-checklist.md)、[资源核对记录](../records/module-symbol-owner-20261006.md)。
