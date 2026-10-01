# Matrix LED 状态显示设计

本文是 [Matrix LED 状态显示需求](../request/matrix-led-request.md) 的实现设计，定义物理映射、颜色取值、状态模型、状态来源、渲染时序和模块接口。图案、闪烁节奏、异常优先级等显示规则以需求文档为准。

本设计不实现 BLE 手柄或 BLE 云台连接功能，只为它们预留状态入口。

## 设计约束

- LED 渲染器独占 RMT，其他模块只提交状态，不直接刷新灯阵。
- 状态提交不得阻塞，可以从 Bluetooth 回调或 I²C 主循环调用。
- 显示优先级集中在一处决定：启动进度 > 运行期异常 > 普通状态。

## 坐标与物理映射

需求文档中的图案使用逻辑坐标 `(x,y)`，左上为 `(0,0)`。

ATOM Matrix 的 WS2812 按行顺序串联，物理索引通常为 `y * 5 + x`。实现时仍增加 `logical_to_physical[25]` 映射表，使业务图案不依赖串联顺序和安装方向。首次上板使用四角校准图依次点亮左上、右上、右下、左下，确认外壳安装方向（USB-C 口朝向）后再固定映射。

## 颜色与亮度

颜色分量范围继续限制为 `0..20`，建议默认值如下：

```text
熄灭 OFF      (.) = ( 0,  0,  0)
白色 WHITE    (w) = ( 6,  6,  6)
绿色 GREEN    (g) = ( 0, 10,  0)
黄色 YELLOW   (y) = (10,  8,  0)
橙色 ORANGE   (o) = (12,  4,  0)
红色 RED      (r) = (12,  0,  0)
蓝色 BLUE     (b) = ( 0,  0, 12)
青色 CYAN     (c) = ( 0, 10, 10)
紫色 MAGENTA  (m) = (10,  0, 10)
```

## 启动阶段

上电后 `matrix_status_init()` 最先执行，完成 RMT 初始化并启动渲染任务，随即清除 WS2812 中残留的上一帧，然后显示启动进度。各阶段的完成条件：

| 列 | 阶段 | 完成条件 |
| --- | --- | --- |
| 0 | LED 就绪 | RMT 和渲染任务创建成功 |
| 1 | 外设就绪 | 按键 GPIO 和 Grove I²C 从机初始化成功 |
| 2 | 存储就绪 | NVS 初始化并读取配对信息 |
| 3 | 蓝牙栈就绪 | Bluetooth 控制器和 Bluedroid 启用 |
| 4 | HID Host 就绪 | 收到 `ESP_HIDH_INIT_EVT` 且状态为成功，连接任务已创建 |

`matrix_status_init()` 本身失败时灯阵无法显示任何内容，只能依赖串口日志。

## 状态模型

无线设备统一使用以下状态：

```c
typedef enum {
    MATRIX_LINK_DISCONNECTED,
    MATRIX_LINK_SEARCHING,
    MATRIX_LINK_CONNECTING,
    MATRIX_LINK_CONNECTED,
} matrix_link_state_t;
```

异常单独保存，不能把错误混入连接状态。异常按位存储，多个异常可以同时有效：

```c
typedef enum {
    MATRIX_FAULT_EVENT_OVERFLOW = 1u << 0,
    MATRIX_FAULT_I2C_PROTOCOL   = 1u << 1,
    MATRIX_FAULT_BLUETOOTH      = 1u << 2,
} matrix_fault_t;
```

启动阶段：

```c
typedef enum {
    MATRIX_BOOT_LED,
    MATRIX_BOOT_PERIPHERALS,
    MATRIX_BOOT_STORAGE,
    MATRIX_BOOT_BLUETOOTH,
    MATRIX_BOOT_HID_HOST,
    MATRIX_BOOT_DONE,
} matrix_boot_stage_t;
```

状态快照至少包含：

```c
typedef struct {
    matrix_boot_stage_t boot_stage;      // 当前正在执行的阶段
    TickType_t boot_tick;                // 上电时刻，用于 LCD 等待超时
    TickType_t boot_done_tick;           // 进度完成时刻，用于 300 ms 收尾
    bool lcd_seen;
    TickType_t lcd_last_valid;
    matrix_link_state_t classic_gamepad;
    matrix_link_state_t ble_gamepad;
    matrix_link_state_t ble_gimbal;
    uint32_t fault_mask;
    TickType_t overflow_until;
    TickType_t i2c_window_start;
    uint8_t i2c_bad_count;
    uint8_t i2c_good_streak;
} matrix_status_snapshot_t;
```

所有时间比较使用 FreeRTOS 可回绕的差值，例如 `(int32_t)(now - deadline) >= 0`，不直接比较绝对 tick 大小。

## 状态来源

### LCD 链路

- 每次收到帧头、版本和命令号均合法的 I²C 命令时更新 `lcd_last_valid`，并置 `lcd_seen`。任何合法命令都可以刷新 LCD 心跳，不要求额外发送“保持连接”命令。
- `now - lcd_last_valid <= 1500 ms` 表示已连接。
- 从未收到命令且 `now - boot_tick <= 10 s` 时显示 LCD 启动等待动画。
- 超过 1500 ms 没有心跳，或首次等待超时，显示静态断开。

当前 LCD 在线时每轮为“发送 → 等待 30 ms → 接收 → 等待 20 ms”，约 50 ms 查询一次，1500 ms 阈值可以容忍调度延迟和短暂总线繁忙。

LCD 端在链路断开后只做地址探测（不带数据），ATOM 不会把探测视为合法命令；恢复后 LCD 首先发送 `ATOM_CMD_RGB`，该命令按心跳处理（见下文“迁移”）。

### 经典蓝牙手柄

`ds4_host` 新增 `ds4_host_get_link_state()`，在内部锁中读取标志并映射：

| 内部条件 | 状态 |
| --- | --- |
| `!ready` | `MATRIX_LINK_DISCONNECTED` |
| `ready && !active && !connecting`（包括扫描中和两次尝试之间的间隔） | `MATRIX_LINK_SEARCHING` |
| `connecting`，或 `active` 但尚未 `approved`，或尚未收到有效输入 | `MATRIX_LINK_CONNECTING` |
| `active && approved && state.connected` | `MATRIX_LINK_CONNECTED` |

扫描与两次尝试之间的 3 秒间隔都归为“后台搜索”，避免灯在扫描间隙熄灭。

渲染任务每个 tick 主动轮询该函数，DS4 模块不调用 LED 接口，避免在 Bluetooth 回调中引入额外依赖或遗漏状态变化。

`ESP_HIDH_INIT_EVT` 是异步事件。若其状态为失败，或 `ds4_host_init()` 返回后 3 秒内仍未收到该事件，此时系统已进入运行期，提升为 Bluetooth 运行期错误，不复位。`ds4_host_init()` 内的同步失败由调用方 `ESP_ERROR_CHECK` 复位。

### BLE 手柄

后续 BLE HID 模块按以下规则通过 setter 提交状态：

- 周期扫描、尚未选定目标：后台搜索。
- 已选定目标，建立连接、发现服务或订阅通知：连接中。
- 目标 HID 服务确认且收到有效输入：已连接。
- 功能未就绪或用户禁用：未连接。
- BLE Host/GATT 异步初始化失败或连续恢复失败：Bluetooth 运行期错误。

### BLE 云台

后续云台模块按以下规则通过 setter 提交状态：

- 周期扫描、尚未选定目标：后台搜索。
- 已选定目标，建立连接、发现服务、协商 MTU 或订阅通知：连接中。
- 控制服务和必要特征已确认，写入通道可用：已连接。
- 功能未就绪或用户禁用：未连接。
- 仅建立 BLE 链路但控制服务不可用时不得显示为已连接。

### I²C 协议错误

当前 8 字节命令帧格式为 `A5 01 seq cmd p0 p1 p2 p3`，没有校验字段。以下情况计为非法命令：

- 帧头不是 `0xA5`；
- 版本不是 `1`；
- 命令号不是 `0..3`。

接收端必须能从错位中恢复，否则一旦丢失一个字节，后续每帧都会错位，异常永远无法清除：

- 当前缓冲第 0 字节不是 `0xA5` 时，丢弃该字节并在剩余数据中搜索下一个 `0xA5`；
- 半帧数据超过 20 ms 没有新字节到达时清空缓冲；
- 若后续需要更强的完整性保证，在协议版本 2 中增加校验字段，不在版本 1 中隐式修改帧格式。

### 事件缓存溢出

溢出检测由渲染任务周期读取各模块的 `dropped` 计数，发现变化即触发，不依赖 LCD 是否正在发送事件查询命令，也不在 `ds4_events_push()` 所在临界区内调用 LED 接口。

`ds4_events` 溢出时丢弃最旧事件，事件 ID 连续递增（跳过 0），因此 LCD 端可在不改协议的前提下检测丢失：收到的事件 ID 不等于 `ack + 1`（跳过 0）时视为发生过丢失，此时只用该事件的按键快照同步 `event_buttons`，不调用 `report_buttons()` 生成边沿动作。该检查需要在 `main/atom_link.c` 中实现。

### LED/RMT 自身错误

初始化阶段 RMT 创建失败按初始化失败处理，直接复位。

运行期 RMT 发送失败时无法保证灯阵能够正确显示异常，因此不尝试递归刷新错误图案，也不因灯阵问题中断 Bluetooth 和 I²C 服务。应：

- 记录错误码；
- 保留最后一帧；
- 延迟 1 秒后重新初始化 RMT 通道；
- 连续 5 次恢复失败后停止重试并持续记录日志。

## 渲染时序

- 渲染在独立任务中执行，优先级低于 I²C 命令处理，避免 RMT 等待影响 LCD 读取回复。LCD 发送命令后只等待 30 ms 即读取，I²C 处理路径中不得有阻塞渲染或长延时。
- 动画 tick 为 125 ms：启动进度闪烁为 2 个 tick 亮、2 个 tick 灭；连接中闪烁同为 250 ms 亮灭；后台搜索为每 16 个 tick 亮 1 个 tick；LCD 等待动画每 tick 前进一步。
- 闪烁相位按绝对时间计算，例如 `((now - boot_tick) / 250 ms) & 1`，不依赖累计计数，避免调度抖动积累。
- 只在动画步进、状态变化或异常变化时生成并发送新帧；静态画面另外每 2 秒低频重发一次，防止干扰导致灯珠状态错乱后长期停留。
- 渲染前复制一次完整状态快照，避免同一帧混合两个时刻的数据。
- 状态变化与渲染通过临界区同步；setter 只修改快照字段，不调用 RMT。
- 图案优先级集中在一个函数中决定：启动进度 > 运行期异常 > 普通状态。业务模块不得绕过优先级直接写像素。

## 建议模块接口

将现有 `m5_atom_matrix/main/app_main.c` 中的 RMT、颜色和 `matrix_show()` 拆到 `matrix_status.c/.h`。建议接口：

```c
esp_err_t matrix_status_init(void);
void matrix_status_boot_stage(matrix_boot_stage_t stage);
void matrix_status_note_lcd_command(bool valid);
void matrix_status_set_ble_gamepad(matrix_link_state_t state);
void matrix_status_set_ble_gimbal(matrix_link_state_t state);
void matrix_status_raise_fault(matrix_fault_t fault);
void matrix_status_clear_fault(matrix_fault_t fault);
```

`app_main` 的初始化顺序调整为：

```c
ESP_ERROR_CHECK(matrix_status_init());                 // 阶段 0 完成
matrix_status_boot_stage(MATRIX_BOOT_PERIPHERALS);
button_init();
ESP_ERROR_CHECK(grove_i2c_init());
matrix_status_boot_stage(MATRIX_BOOT_STORAGE);
ESP_ERROR_CHECK(ds4_host_init());                      // 内部依次推进 STORAGE、BLUETOOTH、HID_HOST
// HID_HOST 在 ESP_HIDH_INIT_EVT 成功时推进到 DONE
```

实现约束：

- `matrix_status` 是唯一允许调用 RMT 发送函数的模块。
- setter 可以从 Bluetooth 回调或 I²C 主循环安全调用，不得阻塞。
- 清除某个异常只清除对应位，不影响其他异常。
- I²C 异常按健康帧计数清除；事件溢出按截止时间清除；Bluetooth 运行期错误在当前实现中保持到重启。
- 初始化阶段保留 `ESP_ERROR_CHECK` 复位行为；运行期 LED 刷新失败按“LED/RMT 自身错误”处理，不调用 `ESP_ERROR_CHECK`。

### 迁移

- LCD 的 `ATOM_CMD_RGB` 不再覆盖灯阵。继续接受该命令，但忽略颜色载荷，只作为心跳；回复格式不变。
- ATOM 按键不再切换灯阵颜色，只保留按下状态和按下计数上报。
- 按键去抖改为非阻塞方式（记录首次按下时刻，下一轮循环确认），去掉主循环中的 `vTaskDelay(30)`，避免延误 I²C 回复。

## Bluetooth 双模限制

当前 `ds4_host_init()` 调用 `esp_bt_controller_mem_release(ESP_BT_MODE_BLE)`，并且 `sdkconfig.defaults` 中配置为 `CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=y`、未启用 `CONFIG_BT_BLE_ENABLED`。要同时支持经典蓝牙手柄、BLE 手柄和 BLE 云台，后续必须：

- 不再释放 BLE 内存；
- 将控制器改为 BTDM 双模；
- 启用 BLE Host/GATT Client 所需配置；
- 明确 BLE 手柄和云台能否同时保持连接；
- 重新测量内部 RAM、任务栈、扫描期间延迟和 DS4 稳定性；
- 验证 Classic Bluetooth 与两个 BLE 连接并发至少 30 分钟。

这些改动不属于本显示方案的实现范围，但 LED 状态接口已经为它们预留独立入口。

## 主机渲染测试

放在 `m5_atom_matrix/tests` 中，与现有 `test_ds4_report.c`、`test_ds4_events.c` 同样以主机程序运行：

- 验证每个静态图案生成准确的 25 个逻辑像素。
- 验证启动进度各阶段的常亮列、闪烁列和完成后 300 ms 收尾。
- 验证启动进度期间不显示异常，结束后立即显示已存在的异常。
- 验证左下 LCD 灯及右下三个无线设备灯的位置和颜色固定。
- 验证 LCD 和三个无线设备可以同时显示已连接。
- 验证后台搜索、连接中两种闪烁节奏，且只影响对应设备灯。
- 验证 LCD 启动等待动画沿预定路径循环，10 秒后转为断开。
- 验证静态帧不重复提交，仅按 2 秒周期低频重发。
- 验证多个异常同时存在时的优先级，以及清除低优先级异常不影响高优先级异常。
- 验证 I²C 接收在丢失 1 字节、插入多余字节后能重新对齐。
- 验证 TickType 回绕附近的 LCD 超时、启动等待和溢出截止时间。
- 验证 LCD 端事件 ID 跳号检测（含跨越 0 的情况）不产生按键边沿。

实机测试和验收标准见需求文档。
