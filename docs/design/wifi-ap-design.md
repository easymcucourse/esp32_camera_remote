# Wi-Fi 热点设计

本文是 [Wi-Fi 热点需求](../request/wifi-ap-request.md) 的实现设计，定义热点配置的数据结构、默认值、NVS 持久化、校验、生效流程、相机 RSSI、显示（含 IP 地址）与恢复出厂设置。需求编号（R1.1 等）指需求文档中的条目。

> 配置校验 / 编解码、NVS 加载、独立应用任务、串口 wifi 命令、只重置热点的二次确认及连接页动态 IP / 密码开关已接入。手柄热点页已接入代码，两级恢复出厂已接入，网页维护入口仍待实现；构建与实机证据见实施状态。

## 1. 设计约束

- 热点配置只有一个数据源：本模块在 NVS 中保存的记录。Wi-Fi 驱动继续使用 `WIFI_STORAGE_RAM`，不让驱动自行保存一份配置，避免两份配置不一致。
- 配置读取失败不得导致无法启动（R3.2）：任何 NVS 错误都回退到默认配置并记录日志，不调用 `ESP_ERROR_CHECK`。
- 密码不写入日志。日志只输出 SSID、信道和密码长度；串口只在显式请求时输出密码明文（见第 8 节）。原因见 [通信记录的公开范围](../README.md#通信记录的公开范围)。
- 重启热点可能耗时数百毫秒，不得在 `atom_link`、`camera_pair`、`jpeg_decode` 等实时任务中执行。
- 相机配对身份（NVS 命名空间 `sony_remote`）与热点配置分开保存，修改热点或只重置热点都不影响配对（R2.4、R5.1）。

## 2. 模块划分

```mermaid
flowchart LR
    console["dbg_console<br/>wifi 命令"] -->|"请求"| ap
    ui["设置菜单<br/>热点页"] -->|"请求"| ap
    cfg["wifi_config<br/>纯 C：默认值 · 校验 · 编解码"] --> ap["wifi_ap<br/>NVS · 驱动 · RSSI"]
    cam["camera_pair / camera_identity"] -->|"目标相机 MAC"| ap
    ap -->|"SSID · 密码文字 · RSSI"| disp["board_7b / ui_presenter"]
```

| 模块 | 文件 | 职责 | 依赖 |
| --- | --- | --- | --- |
| `wifi_config` | `main/wifi_config.c/.h` | 默认配置常量、随机密码生成、字段校验、NVS 记录编解码 | 无（纯 C，可主机测试） |
| `wifi_ap` | `main/wifi_ap.c/.h` | 加载与保存配置、启动与重启热点、恢复出厂、客户端查询、目标相机 RSSI | ESP-IDF Wi-Fi、NVS、`wifi_config` |

`wifi_ap.c` 创建常驻任务 `wifi_config`（优先级 2，栈 4096 字节，内部 RAM），串行处理配置 / 重置、每 2 秒更新客户端及 RSSI、每 10 秒输出客户端日志。`app_main` 主循环只保留内存日志。

## 3. 数据结构

```c
#define WIFI_SSID_MAX      32
#define WIFI_PASSWORD_MIN  8
#define WIFI_PASSWORD_MAX  63

typedef struct {
    char    ssid[WIFI_SSID_MAX + 1];         /* NUL 结尾，1–32 字节 */
    char    password[WIFI_PASSWORD_MAX + 1]; /* NUL 结尾，8–63 个可打印 ASCII */
    uint8_t channel;                         /* 1–13 */
    bool    show_password;                   /* 连接页是否显示明文，R4.2 */
} app_wifi_config_t;
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

NVS 本身对 blob 带 CRC，记录内不再加校验。解码时逐项检查：blob 长度为 100、`version == 1`、长度字段在范围内、内容通过第 5 节的校验；任一项不满足即视为损坏。以后增加字段时提高 `version`，旧版本记录由解码函数迁移，未知的新版本按损坏处理。

## 4. 默认配置（R1）

默认配置是固定常量，所有设备相同：

| 项目 | 值 |
| --- | --- |
| SSID | `easycamctrl` |
| 密码 | `00000000` |
| 信道 | 6 |
| 显示密码 | 开启 |

```c
#define WIFI_DEFAULT_SSID      "easycamctrl"
#define WIFI_DEFAULT_PASSWORD  "00000000"
#define WIFI_DEFAULT_CHANNEL   6

void wifi_config_make_default(app_wifi_config_t *out);
```

- 默认值不写入 NVS。NVS 中没有记录即表示“出厂状态”，恢复出厂设置只需删除记录。
- 固定的简单密码是需求中有意的选择（相机屏幕键盘输入困难），代价是附近任何知道默认值的人都能加入热点。缓解措施：
  - 规划维护页面使用屏幕 PIN 保护（当前尚未实现），见 [维护页面设计](maintenance-design.md#52-登录与会话r2)；
  - 相机首次配对需要在相机上确认，加入热点的其他设备无法冒用本设备的配对身份；
  - 规划在默认密码旁显示黄色 DEFAULT；当前 `board_7b.c` 尚未绘制该标签。

`NEW PASSWORD`（手柄菜单）、`wifi newpass`（串口）和网页“生成随机密码”仍生成随机密码，供需要更高安全性的用户使用：

- 12 个字符，字母表为 `abcdefghjkmnpqrstuvwxyz23456789`（去掉易混淆的 `i l o 0 1`），约 59 位熵。
- 随机数取自 `esp_fill_random()`；这些入口只在热点运行后可用，射频已开启，熵源有效。
- 字符选取使用拒绝采样（随机字节 ≥ 248 时丢弃重取（31 字符字母表）），避免取模偏差。

```c
typedef void (*wifi_random_fn)(void *buffer, size_t length);

void wifi_config_make_password(char out[WIFI_PASSWORD_MAX + 1], wifi_random_fn random);
```

## 5. 校验（R2.2）

```c
typedef enum {
    WIFI_CFG_OK,
    WIFI_CFG_SSID_EMPTY,
    WIFI_CFG_SSID_TOO_LONG,
    WIFI_CFG_SSID_BAD_CHAR,
    WIFI_CFG_PASSWORD_TOO_SHORT,
    WIFI_CFG_PASSWORD_TOO_LONG,
    WIFI_CFG_PASSWORD_BAD_CHAR,
    WIFI_CFG_CHANNEL_RANGE,
} wifi_cfg_error_t;

wifi_cfg_error_t wifi_config_check_ssid(const char *ssid);
wifi_cfg_error_t wifi_config_check_password(const char *password);
wifi_cfg_error_t wifi_config_check_channel(unsigned channel, unsigned max_channel);
const char *wifi_config_error_text(wifi_cfg_error_t error);  /* 串口与界面提示共用 */
```

| 字段 | 规则 | 说明 |
| --- | --- | --- |
| SSID | 1–32 字节；不含 `0x00–0x1F`、`0x7F` | 按字节计长度，允许 UTF-8；控制字符会破坏界面和日志 |
| 密码 | 8–63 个字符，每个在 `0x20–0x7E` 之间 | 允许空格；64 位十六进制 PSK 形式不支持 |
| 信道 | 1 至 `max_channel` | `max_channel` 由国家码决定，见下文 |

错误文字为英文短句（例如 `password must be 8-63 printable ASCII characters`），串口 `ERR` 行和界面提示共用，满足“拒绝保存并提示原因”。

### 国家码与信道

ESP-IDF 默认国家码为 `01`，只允许信道 1–11，此时 SoftAP 设置信道 12、13 会失败。启动时用 `esp_wifi_set_country_code()` 设置固定国家码，取值由 Kconfig `APP_WIFI_COUNTRY` 指定，默认 `JP`（信道 1–13）。`max_channel` 从 `esp_wifi_get_country()` 读出的范围计算，不在代码中写死 13。

ZV-E10 能否加入信道 12、13 的热点待实机验证。

## 6. 加载与启动

```mermaid
flowchart TB
    start["app_main → wifi_ap_load_config()"] --> load{"nvs_get_blob(wifi_ap/cfg)"}
    load -->|"成功且解码通过"| use["使用记录"]
    load -->|"NOT_FOUND"| def1["默认配置<br/>INFO：出厂状态"]
    load -->|"其他错误或解码失败"| def2["默认配置<br/>WARN：记录损坏及原因"]
    def2 --> erase["删除损坏的记录<br/>失败只记录 ERROR"]
    use & def1 & erase --> drv["esp_wifi_set_config + esp_wifi_start"]
```

- 记录损坏时删除该记录，下次启动按出厂状态处理，不再重复报警。日志明确提示“热点配置已恢复为默认值”，相机需按默认 SSID 和密码重新连接。
- `nvs_open` 本身失败（例如 NVS 未初始化）时使用默认配置，热点照常启动。
- 启动日志：`AP READY: SSID=<ssid> channel=<n> WPA2-PSK password_len=<n> IP=<ip>`，不输出密码；IP 从网络接口读取。

## 7. 修改与生效（R2.4）

所有修改都通过同一个请求接口进入 `wifi_ap` 任务，按顺序执行：

```c
/* 配置复制入队、立即返回，完成结果按 token 查询。 */
esp_err_t wifi_ap_request_apply(const app_wifi_config_t *config, uint32_t *token);
esp_err_t wifi_ap_request_reset(bool all, uint32_t *token);
esp_err_t wifi_ap_request_result(uint32_t token, esp_err_t *result);
void      wifi_ap_get_config(app_wifi_config_t *out);
unsigned  wifi_ap_max_channel(void);
```

- 串口和界面均异步入队；深度 2，满时返回 ESP_ERR_INVALID_STATE。结果记录有 8 槽，只淘汰已完成项；查询返回 NOT_FINISHED（待完成）、OK（结果已取得）或 NOT_FOUND（历史已淘汰）。改变 show_password 也走同一接口，没有单独 setter。
- 执行步骤：

  1. 再次校验；失败直接返回，不改动任何状态。
  2. 与当前配置比较；完全相同时直接返回成功。
  3. 写入 NVS 并 commit。失败时返回错误，热点保持旧配置，不进入下一步。
  4. 网络字段变化时执行 esp_wifi_stop → esp_wifi_set_config → esp_wifi_start；仅显示开关变化不重启热点。
  5. 成功后提交内存中的配置（短临界区），失败保持旧配置并尝试回滚。
  6. 发布显示元数据及完成结果，由界面 / 串口轮询 token。

- 驱动重启失败时写回旧 NVS 配置并重启旧热点；回滚不完整会记录 ERROR，不能把内存保留旧值当作持久化和驱动均已恢复。
- 普通热点重启会断开相机，由取景重试逻辑负责重连；全部重置会显式取得相机维护占用，先停止 / 排空，再清除身份并重启 LCD。

| 变化内容 | 提示 | 相机侧 |
| --- | --- | --- |
| 只改信道 | `Wi-Fi restarted` | SSID 和密码不变，相机预计自动重连（待验证） |
| 改 SSID 或密码 | `Reconnect camera to new Wi-Fi` | 需要在相机上重新选择热点并输入密码；配对身份不变，不需要重新确认配对 |

## 8. 修改入口

### 串口命令

当前命令由 `camera_console` / `wifi_console` 按行解析，支持单双引号与反斜杠转义；修改先返回 [dbg] OK queued，随后 DONE / FAIL。完整 [UART 调试框架](uart-debug-design.md) 仍待实现。

| 命令 | 行为 |
| --- | --- |
| `wifi show` | 输出 SSID、信道、密码长度、显示密码开关、客户端列表（MAC、IP、RSSI） |
| `wifi show password` | 同上，另输出密码明文 |
| `wifi set <键> <值> [<键> <值>…]` | 键为 `ssid`、`password`、`channel`；全部校验通过后一次保存，只重启一次热点 |
| `wifi newpass` | 生成新的随机密码并生效，输出新密码 |
| `wifi display on\|off` | 连接页是否显示密码明文 |
| `factory wifi` / `factory all` | 进入恢复出厂确认，10 秒内需要 `factory confirm` |
| `factory confirm` | 执行待确认的恢复出厂；没有待确认请求时返回 `ERR nothing to confirm` |

示例：`wifi set ssid "My Cam AP" password "abc def 123" channel 11`。

### 手柄设置菜单

在 SETTINGS 参数菜单末尾增加 `WI-FI ›` 一项，进入后右侧面板改为热点页，右侧 256 像素宽；标题 y=8，九个可选项从 y=44 起、行距 54，每项最多两行、字号 16，页脚从 y=535 起：

| 行 | 内容 | 操作 |
| --- | --- | --- |
| 0 | `WI-FI HOTSPOT` | 标题 |
| 1 | `SSID easycamctrl` | 确认键进入 SSID 编辑 |
| 2 | `PASS 00000000` / `PASS ********` | 只显示 |
| 3 | `NEW PASSWORD` | 确认键生成新密码（草稿） |
| 4 | `CHANNEL ‹ 6 ›` | 左 / 右在 1 至 `max_channel` 间调整（草稿） |
| 5 | `SHOW PASS ON` / `OFF` | 左 / 右切换，立即保存，不重启热点 |
| 6 | `APPLY` | 草稿与当前配置不同时可用，执行“修改与生效” |
| 7 | `RESET WI-FI` | 恢复出厂（只热点），需二次确认 |
| 8 | `RESET ALL` | 恢复出厂（全部），需二次确认 |
| 9 | `BACK` | 放弃草稿，回到参数菜单 |

- 上 / 下移动光标，与参数菜单一致。确认键和返回键的分配在 [手柄控制方案](../request/gamepad-request.md) 中确定（当前 A / DS4 叉确认，B / DS4 圈返回）；热点页内这两个键不触发变焦。
- SSID、密码、信道的修改先进入草稿，有未应用的草稿时行尾显示 `*`，`APPLY` 后才保存。
- **SSID 编辑**：只提供字符集 `a–z`、`0–9`、`-`、`_`。左 / 右移动字符位置，上 / 下切换当前位置的字符，确认键结束编辑，返回键放弃。更多字符只能通过串口设置。
- **密码**：不提供逐字输入，只能“重新生成”（R2.3 允许的替代方式）；自定义密码通过串口设置。
- **二次确认**：在 `RESET …` 行第一次按确认键后，该行变为 `PRESS AGAIN` 并保持 3 秒；3 秒内再按一次才执行，移动光标或超时即取消。
- 热点页只在 SETTINGS 偏好下可达，相机离线时仍可导航。手柄断开 / gap、退出设置或取景返回连接页时放弃草稿；已经提交的请求继续执行。

### 网页维护页面（规划，尚未实现）

手柄只能编辑有限字符的 SSID、不能输入自定义密码。需要任意 SSID 和密码时，在连接页或设置菜单开启维护模式（取景中开启会先停止取景，相机连上后维护模式自动关闭），用手机或电脑浏览器打开 `http://192.168.4.1/` 修改。未来网页通过 `wifi_ap_request_apply(config, &token)` 异步提交，校验函数和错误文字与串口相同，详见 [维护页面设计](maintenance-design.md#6-热点设置r4)。

## 9. 显示（R4）

- 连接页和热点页均遵守 show_password，关闭时显示 ********。连接页 DEFAULT 标签仍是待实现项。
- **IP 地址（R4.4）**：连接页在密码行下方新增 `IP: 192.168.4.1` 一行。
  - 地址在 `WIFI_EVENT_AP_START` 处理中用 `esp_netif_get_ip_info(ap_netif)` 读取，格式化为点分十进制后提交显示；界面代码不写死地址。维护页面信息框中的 URL 使用同一个值。
  - 热点启动前（开机初期、热点重启期间）显示 `IP: --`；收到 `WIFI_EVENT_AP_STOP` 时也提交 `--`。
  - 当前坐标如下；其余连接阶段和提示位置见 [界面设计](ui-design.md#连接页)：

    | 行 | x | y | 字号 |
    | --- | --- | --- | --- |
    | `SSID: …` | 48 | 130 | 32 |
    | `Password: …` | 48 | 190 | 32 |
    | `IP: …` | 48 | 232 | 22 |
    | `Expansion unit (ATOM): …` | 48 | 270 | 30 |
    | `Controller (DS4): …` | 48 | 330 | 30 |

  - 当前统一用 board_7b_set_wifi_info(ssid, password, show_password, ip) 发布元数据；事件回调不绘图，工作任务调用 board_7b_refresh_wifi_info。
  - 取景画面和设置面板不显示 IP。
- `board_7b_init()` 使用启动配置；之后动态文字由 board_7b_set_wifi_info 更新。ui_presenter / 通用显示抽象仍是 [后续目标](sony-ptpip-design.md#115-板级对象board_7bh)。
- 串口 wifi show password 显式输出明文，wifi newpass 成功后也输出新密码；热点页仍遵守显示开关。

## 10. 相机 RSSI（R4.3）

当前按已选目标相机 MAC 查找关联客户端，不再取 clients.sta[0]：

```c
void wifi_ap_select_camera(const uint8_t mac[6]);   /* NULL 清除目标 */
```

- 目标 MAC 由 camera_controller 在动态发现 / 身份确认后提供，结束或清除身份时移除。
- `wifi_ap` 任务每 2 秒调用一次 `esp_wifi_ap_get_sta_list()`，在列表中找到目标 MAC 时提交其 RSSI，否则提交“无值”。
- 当前无值仍显示 WIFI -- DBM；需求中的 WIFI -- 简写尚未统一。
- 客户端列表日志仍每 10 秒输出一次。

## 11. 恢复出厂设置（R5）

| 级别 | 动作 | 之后 |
| --- | --- | --- |
| 只热点（all=false） | 删除 wifi_ap/cfg、恢复默认值及显示开关；网络字段变化时按第 7 节重启热点 | 不重启设备，保留相机身份；默认配置已生效时无需重启热点 |
| 全部（all=true） | 取得相机维护占用、等待最多 5 秒停止 / 排空；删除 wifi_ap/cfg 并清除 sony_remote | 成功后等待 500 ms 再 esp_restart；需要重新配对相机，ATOM 手柄绑定保留 |

- 全部恢复出厂只清除本项目的命名空间，不调用 `nvs_flash_erase()`，避免误删 PHY 校准等其他数据。
- 失败不重启并释放维护占用；保存 / 身份清除失败会尝试恢复热点记录，身份清除可能部分完成，不能宣称全部状态已回滚。
- 二次确认在入口一侧完成（串口 `factory confirm`、界面 `PRESS AGAIN`），`wifi_ap_request_reset()` 本身不再确认。
- 无法进入界面和串口时，按 [故障排查与恢复](../user-guide/troubleshooting.md) 擦除 NVS（R5.3）。

## 12. 并发

| 数据 | 写者 | 读者 | 保护 |
| --- | --- | --- | --- |
| 当前配置 | wifi_config 任务 | 串口、界面、显示 | config_mux 短临界区，读取方拿拷贝 |
| 请求队列 | 串口任务、界面 | `wifi_ap` 任务 | FreeRTOS 队列 |
| 目标 MAC | 相机模块 | `wifi_ap` 任务 | 临界区，6 字节拷贝 |
| RSSI | `wifi_ap` 任务 | 绘制 | 原子变量（沿用现有 `wifi_rssi`） |

Wi-Fi 驱动调用（`esp_wifi_stop/set_config/start`）只在 `wifi_ap` 任务中进行；`wifi_ap_start()` 在创建任务之前完成首次启动。

## 13. 测试

### 主机单元测试 `test_wifi_config`

- SSID：0、1、32、33 字节；含 `0x1F`、`0x7F`；多字节 UTF-8 恰好 32 字节。
- 密码：7、8、63、64 个字符；含 `0x1F`、`0x7F`、非 ASCII；含空格。
- 信道：0、1、`max_channel`、`max_channel + 1`；`max_channel` 为 11 和 13 两种情况。
- 默认配置为 `easycamctrl` / `00000000` / 信道 6 / 显示密码，且自身能通过校验。
- 随机密码：长度 12，只含字母表字符；用固定随机序列验证拒绝采样（输入 ≥ 248 的字节被跳过）。
- 记录编解码往返；长度不为 100、版本为 0 或 2、长度字段越界、内容不合法时解码失败。

### 实机测试

验收以 [需求文档](../request/wifi-ap-request.md#验收测试) 为准，另外补充：

- 用 `nvs_set_blob` 写入 50 字节的 `cfg`，确认以默认配置启动、日志为 `WARN` 且损坏的记录已删除；再次重启不再报警。
- 开机过程中观察连接页：先显示 `IP: --`，热点启动后变为 `IP: 192.168.4.1`；`wifi set channel 11` 重启热点期间短暂显示 `--` 后恢复。
- 改信道 12、13 后确认 ZV-E10 能否加入（待验证）。
- 应用配置过程中连续发送第二个请求，确认按顺序执行或返回队列满，不交错。
- 全部恢复出厂后确认 `ds4_host`（ATOM 侧）和 PHY 数据不受影响，相机重新出现配对确认。

## 14. 实施步骤

1. 新增 `wifi_config.c/.h` 和主机测试。
2. `wifi_ap` 改为从 NVS 加载配置，删除 `AP_SSID` / `AP_PASSWORD` 宏；`board_7b_init()` 改用加载后的配置。
3. 新增 `wifi_ap` 任务、请求队列和 RSSI 按 MAC 查找，主循环中移除客户端查询。
4. 随 [UART 调试控制台](uart-debug-design.md) 注册 `wifi` 和 `factory` 命令。
5. 界面设置菜单实现后增加热点页。
6. 更新根目录 README 中的热点说明、[当前系统架构](architecture-design.md#10-持久化) 的持久化表，以及 [故障排查与恢复](../user-guide/troubleshooting.md) 中的恢复出厂方法。

## 当前请求接口与验证边界（2026-10-02）

手柄热点页由 `wifi_menu` 纯 C 草稿状态机与 `wifi_menu_ui` 低优先级任务接入。主参数页追加第八项 `WI-FI >`，A（DS4 叉）或右方向进入，B（圈）返回。SSID 编辑有可选结束符，用于删尾和追加；空 SSID 不能完成。显示开关单独提交当前配置，不会连带应用其他草稿。相机参数方向键仍在输入边沿捕获行号，热点输入只拷贝到队列，不在 I²C 任务写 NVS 或绘制。相机离线时仍允许设置导航；手柄缺口 / 断开、退出设置和取景切回连接页会取消草稿。已提交请求不因页面退出而中断。`RESET ALL (reboot)` 已接入，3 秒二次确认后请求全部重置；实际重置与重新配对尚待验收。热点页视觉和实际手柄操作仍待验收。

当前 wifi_ap_request_apply(config, token) 将配置拷贝入深度 2 的队列并立即返回；wifi_ap_request_result(token, result) 查询完成结果。8 槽历史只淘汰已完成项，不覆盖待执行请求，调用方超时也不会留下栈指针。只有 Wi-Fi 任务执行保存和运行时驱动重启；Wi-Fi 事件只发布文本元数据，连接页刷新由工作任务执行。只改密码显示开关不重启热点。

保存 / 重启 / 回滚策略为纯 C wifi_apply，主机覆盖失败次序，但 NVS / 射频 / 连接页本身需实机验证。NVS 初始化失败不再自动擦除或终止启动：使用默认热点，配置保存会明确失败，相机身份保存可能不可用，保留原数据供恢复。并未实现 NVS 分区修复。

串口接收按行，支持双引号与反斜杠转义，wifi 写命令先返回 OK queued，完成后 DONE / FAIL。factory wifi / factory all 均需 10 秒内 factory confirm；全部重置成功后重启。后续共享控制台仍需请求号、版本 / 状态 / log、模拟、脚本和 ATOM 端接入。
