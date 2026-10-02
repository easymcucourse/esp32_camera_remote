# 编译与烧录工具

两个固件工程使用 ESP-IDF 5.5.1。Windows 包装脚本为 `tools/idf.ps1`，主机回归统一由 `tests/host/CMakeLists.txt` 注册。

## 环境

| 项目 | 版本 / 默认位置 |
| --- | --- |
| ESP-IDF | 5.5.1，`C:\Espressif\frameworks\esp-idf-v5.5.1` |
| IDF Python | `C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe` |
| 组件依赖 | `espressif/esp_new_jpeg` 1.0.2 等，版本固定于 manifest 和 `dependencies.lock`；首次构建需要网络 |
| 主机测试 | CMake、CTest、GCC / Clang，例如 MinGW-w64；与交叉编译器分开 |

## LCD 主工程（ESP32-S3）

在已激活 ESP-IDF 的终端、仓库根目录执行，将串口替换为实际端口：

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

- `sdkconfig.defaults`：全局 `-O2`、Flash 120MHz（当前设备启动报告 80MHz）、Octal PSRAM 120MHz（实验配置）。
- `partitions.csv`：NVS、PHY、12MiB factory、剩余 SPIFFS data；没有 OTA 分区。
- 常规烧录保留 NVS 中的热点配置和相机身份；擦除整片 Flash 后需要重新配置 / 配对。
- 本地 `sdkconfig` 不提交。已有配置需用 `idf.py menuconfig` 修改；若要重新采用默认配置，先备份本地 `sdkconfig`，移开后重新生成。仅执行 `reconfigure` 不会覆盖已有配置值。
- 国家码由 `main/Kconfig.projbuild` 的 `APP_WIFI_COUNTRY` 设置，默认 `JP`。

## Windows 包装脚本

脚本设置 `IDF_PATH`、加载 `export.ps1`，在 `-ProjectDirectory` 下执行 `idf.py -p <Port> <Action>`，失败返回错误。

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `-Action`（第一个位置参数） | `build` | `build`、`flash`、`monitor`、`menuconfig`、`size`、`reconfigure` |
| `-Port` | `COM8` | 串口，构建等动作也会传入 |
| `-ProjectDirectory` | 仓库根目录 | 可以指定 `./m5_atom_matrix` |
| `-IdfPath` | 上表 SDK 路径 | 必须包含 `export.ps1` |
| `-IdfPython` | 上表 Python 路径 | IDF Python 解释器 |

```powershell
./tools/idf.ps1 build
./tools/idf.ps1 flash -Port COM8
./tools/idf.ps1 monitor -Port COM8
./tools/idf.ps1 build -ProjectDirectory ./m5_atom_matrix -Port COM6
./tools/idf.ps1 build -IdfPath D:/esp/esp-idf-v5.5.1 -IdfPython D:/esp/python/python.exe
```

每次调用只执行一个动作，不能连写 `flash monitor`。脚本没有烧录波特率参数；ATOM 的 115200 烧录按下节在已激活终端执行。

## ATOM 子工程（经典 ESP32）

```powershell
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash monitor
```

ATOM 高波特率烧录曾失败，115200 已验证可用。旧 `sdkconfig` 需确认 Classic Bluetooth / Bluedroid / HID Host、BR/EDR Only、关闭 SPP，以及 `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y`、`CONFIG_I2C_ISR_IRAM_SAFE=y`。以子工程默认配置为准。

## 哪些改动需要烧录哪一端

| 改动 | LCD | ATOM |
| --- | --- | --- |
| 相机协议、界面、Wi-Fi | ✓ | |
| DS4 连接、灯阵、ATOM 按键 | | ✓ |
| LCD ↔ ATOM I²C 协议 | ✓ | ✓（必须同时升级） |

当前协议为 v2，与 v1 不兼容。源码构建通过与实际烧录版本应分别记录，实机证据见 [记录索引](../records/README.md)。

## 主机测试

在仓库根目录、有主机编译器的终端中运行。MinGW 示例：

```powershell
cmake -S tests/host -B build/host -G "MinGW Makefiles"
cmake --build build/host -j 4
ctest --test-dir build/host --output-on-failure
```

其他环境选择已安装的 CMake generator；同一个构建目录不混用 generator。统一 CTest 当前有 28 项，包括 DS4 报告 / 事件、v2 协议、发送适配、灯阵模型、相机连接 / 属性 / 写入 / 菜单、输入状态机、热点配置 / 菜单及全部重置。测试清单及边界见 [测试文档](testing.md)。

合成输入在测试源码中；四份脱敏属性裁剪样本位于 [fixtures](../../tests/host/fixtures/README.md)，由属性提取工具生成，不包含原始网络包。主机回归不证明真实 NVS、射频、相机写入或界面视觉效果。

## 字体工具

| 工具 | 用途 |
| --- | --- |
| `tools/prepare_fonts.py` | 下载字体到 `.reference/fonts/` 并裁剪；依赖 `tools/requirements-fonts.txt` |
| `tools/font_preview_host/` | 用固件同一份 `ui_fonts.c` 渲染连接页、参数面板和字号样例 |

字体工具命令见 [字体说明](../../components/board_7b/fonts/README.md)。

## 待改进

默认端口、SDK / Python 路径仍带本机默认值，自动发现尚未实现；抓包样本工具的可移植性及双工程 CI 也待完善。`-ProjectDirectory` 已支持 ATOM，不再作为待实现功能。
