# Wi-Fi 热点设计

按2026-10-06当前源码描述网络对象、ESP32后端、正常消息桥和维护配置。原 [热点需求](../request/wifi-ap-request.md) 的数据格式/校验继续适用；修改入口、维护认证和重启规则以 [拆分计划](../../main-module-split-plan.md) 为准。旧UART/手柄编辑器及正常factory已删除，历史实机记录不能代替当前实现验收。

## 1. 设计约束

Wi-Fi驱动使用RAM storage，只有wifi_esp32接触SDK、netif、socket及wifi_ap NVS。普通业务通过app_wifi_messages typed requests查询发现/RSSI、使用opaque TCP channel；维护Web直接使用隔离app_wifi对象，不连接普通消息总线。Core创建唯一对象并控制生命周期。

保存记录与相机sony_remote/UI ui_prefs分开；密码不写普通日志，UART仅显式wifi show password输出明文。正常UART/LCD无配置修改入口。启动层只加载配置，但后端保留无效记录自动修复策略，详见加载节；不能把没有Core直接writer描述成全过程无NVS写入。

## 2. 模块划分

| owner | 实现 | 当前职责 |
| --- | --- | --- |
| common_runtime | common/network_config.* | 纯C默认值/字段校验/100-byte codec/随机密码，唯一编译 |
| app_wifi | app_wifi.c、wifi_apply.c | 版本/能力/opaque对象、config/channel facade与保存/驱动重启/回滚策略 |
| wifi_esp32 | wifi_esp32.c、wifi_config_jobs.c、wifi_saved_config.c、wifi_tcp.c | AP/netif、NVS、串行config queue/history、独占TCP channel；共享值逻辑为显式private依赖 |
| app_wifi_messages | app_wifi_messages.c、wifi_channel_messages.c | 正常状态/发现/RSSI、command/event两TCP lane与lease；退休config写消息拒绝 |
| app_core | app_core_wifi_boot.c、app_core_network.c、维护回调 | 对象创建、启动/绑定/停止、Web结果跟踪及工厂恢复协调；private startup契约无旧wifi_ap compat |
| app_maintenance | maintenance_web.c、maint_wifi.c | Web字段校验、staged prepare/ACK/commit、查询及成功后重启 |
| app_console / app_ui | wifi_console.c / 私有model | 正常只读查询/网络信息展示，无菜单worker或配置writer |

实际依赖与运行关系见 [模块图](module-dependency-graph.md)，任务/queue/停止范围见 [资源表](module-resource-ownership.md)。

## 3. 数据结构

```c
#define NETWORK_SSID_MAX      32
#define NETWORK_PASSWORD_MIN  8
#define NETWORK_PASSWORD_MAX  63

typedef struct {
    char    ssid[NETWORK_SSID_MAX + 1];         /* NUL 结尾，1–32 字节 */
    char    password[NETWORK_PASSWORD_MAX + 1]; /* NUL 结尾，8–63 个可打印 ASCII */
    uint8_t channel;                         /* 1–13 */
    bool    show_password;                   /* 连接页是否显示明文，R4.2 */
} network_config_t;
```

加密方式固定为 WPA2-PSK、不要求 PMF，最大客户端数固定为 4，不放入配置（R1.3）。

### NVS 记录

| 项目 | 值 |
| --- | --- |
| 命名空间 | `wifi_ap` |
| 键 | `cfg`，blob |
| 内容 | 下表，小端，固定 100 字节 |

| 偏移 | 长度 | 字段 |
| --- | --- | --- |
| 0 | 1 | `version`，当前为 `1` |
| 1 | 1 | `channel` |
| 2 | 1 | `flags`：bit 0 `show_password`，其余为 0 |
| 3 | 1 | `ssid_len` |
| 4 | 32 | `ssid`，不足部分填 0 |
| 36 | 1 | `password_len` |
| 37 | 63 | `password`，不足部分填 0 |

NVS 本身对 blob 带 CRC，记录内不再加校验。解码时逐项检查：blob 长度为 100、`version == 1`、长度字段在范围内、内容通过下述字段校验；任一项不满足即视为损坏。当前仅接受version1，不实现其他版本迁移；未知版本按无效记录处理。

## 4. 默认配置（R1）

默认easycamctrl / 00000000 / 信道6 / 显示密码。缺失记录使用这些值；保存完全默认配置时后端删除cfg key并commit，恢复出厂使用同一primitive，不擦除整个NVS。固定密码是产品既有默认值，维护无认证，任何加入热点的客户端具有全部维护权限，见 [维护设计](maintenance-design.md#1-模式与所有权)。

Web生成随机密码为12字符，字母表 `abcdefghjkmnpqrstuvwxyz23456789`，esp_fill_random提供随机字节；纯值kernel按实际字母表长度计算拒绝采样上界，避免modulo偏差。生成操作不直接保存。旧wifi newpass/手柄NEW PASSWORD入口已删。

## 5. 校验（R2.2）

SSID为1–32字节，拒绝控制字符0x00–0x1F和0x7F；允许UTF-8。密码8–63个0x20–0x7E可打印ASCII字符，允许空格，不支持64位hex PSK。信道1至国家范围上限且不超过13。错误由network_cfg_error_t及公共文本表示，无旧wifi_cfg_error_t兼容别名。

后端初始化按APP_WIFI_COUNTRY配置国家码（默认JP），从SDK读取max_channel；固定国家配置并不证明相机实际可使用12/13信道，仍待实机。codec要求version1/固定长度/合法flags/规范零padding；嵌入NUL和非零padding拒绝，不改变原记录格式。

## 6. 加载与启动

Core初始化NVS，错误保留分区、不调用nvs_flash_erase；wifi_esp32创建后saved read加载。无namespace/key返回默认值。后端对过大、无效或不可读blob保留原修复路径：默认值并调用wifi_saved_write，删除cfg并commit；修复失败返回诊断，但输出仍为defaults。namespace打开失败直接返回错误，没有自动擦除分区。Core记录不可读错误，按对象默认值继续启动；国家范围无效但codec有效的记录由Core选RAM默认值，不主动保存。

Core本身没有保存接口调用，与后端读内部自动修复是不同证据范围。后端storage mutex串行保存/读取；修复可能执行Flash写入，不能把Core-only fake测试称为完整cache-off验证。见 [本次文档核对](../records/module-network-docs-20261006.md)。

UI基础初始化与ATOM同步prepare之后启动AP/config；maintenance trigger先于普通bridge/Input/Camera。成功启动信息来自实际netif，无密码日志。当前完整启动顺序见 [架构](architecture-design.md#3-启动顺序)。

## 7. 修改与生效（R2.4）

Web先预约设备重启，app_wifi_config_prepare得到staged token；成功发送响应后commit，发送失败cancel。config queue深度2/results8，只淘汰已完成项，token/网络generation与绝对deadline维持原语义。Core health独立跟踪已commit token：pending继续等待，成功提交1500ms重启，失败取消预约；无需客户端继续轮询。

worker执行wifi_apply：再次校验，相同配置成功不重复写；先保存NVS，失败不重启AP；网络字段变化才stop/set/start，仅show_password变化不重启AP；驱动失败尝试恢复旧持久化与AP，回滚不完整单独报错。保存与AP状态并不天然是原子事务。普通相机owners在维护前已停止，不能依赖正常取景重连来处理Web更新。

## 8. 修改入口

UART仅wifi show与wifi show password；旧wifi set/newpass/display及factory拒绝并提示Web。LCD Wi-Fi行只显示信息。启动连接页访问当前AP地址触发独占维护；NORMAL后HTTP已停止，需要重新启动才能竞争维护。无PIN/login/串口maint或手柄维护入口。操作见 [快速上手](../user-guide/quick-start.md#维护网页)。

## 9. 显示（R4）

连接页SSID/密码/动态IP由Core启动信息和正常网络状态message提交UI model；event callback不绘图，UI renderer独占surface。show_password关闭显示********，default-password标志按真实密码等于默认值计算。地址未知显示--，不把192.168.4.1硬编码进绘图层。普通应用不得包含board/display私有头。坐标见 [界面设计](ui-design.md#连接页)。

## 10. 相机 RSSI（R4.3）

Camera通过typed目标选择/查询指定已选MAC，bridge每2秒刷新对应RSSI，不取首个手机客户端替代相机。发现来自DHCP实际地址，TCP命令/事件由各lane owner执行，无旧wifi_ap_select_camera直接调用。普通bridge退出后维护保留AP，普通RSSI绘制不再继续。

## 11. 恢复出厂设置（R5）

仅POST /api/factory，body严格scope wifi/all与confirm true。wifi保存默认热点（删除cfg）；all另清Sony identity与UI偏好，不清ATOM绑定/PHY。成功保持冻结并1500ms重启，失败恢复维护config admission、仅尝试Wi-Fi回滚，可能已经部分改变其他namespace，不恢复正常应用。完整步骤/失败语义见 [维护设计](maintenance-design.md#5-配置偏好与恢复出厂)，无法访问维护时的NVS恢复见 [排查手册](../user-guide/troubleshooting.md)。

## 12. 并发

config/current/history属于后端，queue/短临界区保护；NVS storage mutex串行操作。普通bridge两个TCP lane分别独占channel，request caller必须确认取消并归还lease。Core先停止普通network owners，再重启维护config owner；对象和history保留，不表示普通消息桥可恢复。详细budget、retained对象和错误退出见 [资源表](module-resource-ownership.md)。

## 13. 测试

纯值测试覆盖字段边界、默认、codec/规范padding及随机拒绝采样；后端保存测试覆盖缺失/损坏修复、保存/commit失败、仅cfg清除和无关key保留；jobs/facade/bridge/channel tests覆盖token、排队、generation、lease及停止。Core boot测试只验证编排层无直接writer、RAM fallback及初始化错误，不能代替后端保存测试。原54回归保留，新增当前tests见 [测试文档](../development/testing.md)。

真实射频/Flash/cache-off、维护时AP重启、信道12/13相机连接、丢ACK和factory重新配对仍未验收；旧损坏记录修复/菜单操作日期证据不能证明当前完整拆分版实机行为。最终范围见 [清单](../development/module-split-checklist.md)。
