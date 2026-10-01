# 编译与烧录工具

本文说明两个固件工程的编译、烧录方法，Windows 包装脚本 `tools/idf.ps1` 的用法，以及主机测试和字体工具的构建方式。

## 环境

| 项目 | 版本 / 位置 |
| --- | --- |
| ESP-IDF | 5.5.1 |
| 默认 SDK 路径 | `C:\Espressif\frameworks\esp-idf-v5.5.1` |
| 默认 IDF Python | `C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe` |
| 组件依赖 | `espressif/esp_new_jpeg` 1.0.2 等，由组件管理器下载，版本固定在 `dependencies.lock`；首次构建需要网络 |
| 主机 GCC | 主机测试和字体预览需要，例如 MinGW-w64 |

## LCD 主工程（ESP32-S3）

在已激活 ESP-IDF 的终端中，于仓库根目录执行：

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

- 默认配置在 `sdkconfig.defaults`：全局 `-O2`、Flash 120MHz（实测 80MHz）、Octal PSRAM 120MHz（实验性）。
- 分区表 `partitions.csv`：`nvs`、`phy_init`、12MiB `factory`、剩余空间为 `data`（SPIFFS），没有 OTA。
- 常规烧录保留 NVS 中的配对身份；`idf.py erase-flash` 会清除它，相机需要重新配对。
- 本地 `sdkconfig` 不提交。修改 `sdkconfig.defaults` 后需要删除本地 `sdkconfig` 或执行 `idf.py reconfigure` 才会生效。

### `tools/idf.ps1`

在未激活 ESP-IDF 的 PowerShell 中直接使用。脚本设置 `IDF_PATH`、加载 `export.ps1`，然后在仓库根目录执行 `idf.py -p <Port> <Action>`。

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `-Action`（第一个位置参数） | `build` | `build`、`flash`、`monitor`、`menuconfig`、`size`、`reconfigure` 之一 |
| `-Port` | `COM8` | 串口 |
| `-IdfPath` | 见上表 | ESP-IDF 根目录，必须包含 `export.ps1` |
| `-IdfPython` | 见上表 | IDF Python 解释器 |

```powershell
.\tools\idf.ps1 build
.\tools\idf.ps1 flash -Port COM8
.\tools\idf.ps1 monitor -Port COM8
.\tools\idf.ps1 size
.\tools\idf.ps1 build -IdfPath D:\esp\esp-idf-v5.5.1 -IdfPython D:\esp\python\python.exe
```

限制：

- 只作用于根目录的 LCD 工程，不能编译 ATOM 子工程。
- 每次只能执行一个动作；不支持 `flash monitor` 连写，需要分两次调用。
- `-Port` 对 `build` 等动作也会传给 `idf.py`，不影响结果。

## ATOM 子工程（经典 ESP32）

```powershell
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash monitor
```

- ATOM 高波特率烧录曾失败，115200 已验证可用。
- 已有旧 `sdkconfig` 时，在 `idf.py menuconfig` 中确认启用 Bluetooth、Bluedroid、Classic Bluetooth、HID Host，选择 BR/EDR Only，关闭 SPP。

## 哪些改动需要烧录哪一端

| 改动 | LCD | ATOM |
| --- | --- | --- |
| 相机协议、界面、Wi-Fi | ✓ | |
| DS4 连接、灯阵、ATOM 按键 | | ✓ |
| LCD ↔ ATOM I²C 协议 | ✓ | ✓（必须同时升级） |

## 主机测试

在仓库根目录、有主机 GCC 的环境中执行：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I m5_atom_matrix/main m5_atom_matrix/main/ds4_report.c m5_atom_matrix/tests/test_ds4_report.c -o m5_atom_matrix/build/test_ds4_report.exe
./m5_atom_matrix/build/test_ds4_report.exe

gcc -std=c11 -Wall -Wextra -Werror -I m5_atom_matrix/main m5_atom_matrix/main/ds4_events.c m5_atom_matrix/tests/test_ds4_events.c -o m5_atom_matrix/build/test_ds4_events.exe
./m5_atom_matrix/build/test_ds4_events.exe
```

`m5_atom_matrix/build/` 目录需先存在（编译过一次 ATOM 工程即可）。计划新增的测试和 CI 见 [测试与持续集成设计](testing.md)。

### 相机解析回归测试

重构后的 DeviceInfo 和 Sony 属性解析模块不依赖 ESP-IDF，可在主机上测试。样本在测试源码中构造，不含设备标识或抓包数据。

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I components/ptpip/include -I components/sony_camera/include components/ptpip/ptp_dataset.c components/sony_camera/sony_props.c tests/host/test_camera_parsers.c -o build/test_camera_parsers.exe
./build/test_camera_parsers.exe
```

先创建 `build/` 目录，或使用已编译的 LCD 工程目录。也可用 CMake：

```powershell
cmake -S tests/host -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

测试覆盖正常信息、截断字符串 / 数组、Mode 枚举和回调顺序，以及原属性搜索算法的部分输入行为；不表示目标设计中的结构化解析已经实现。

## 字体工具

| 工具 | 用途 |
| --- | --- |
| `tools/prepare_fonts.py` | 下载原始字体到 `.reference/fonts/`，裁剪后写入 `components/board_7b/fonts/`；依赖 `tools/requirements-fonts.txt` |
| `tools/font_preview_host/` | 用固件同一份 `ui_fonts.c` 在 PC 上渲染连接页、参数面板和字号样例 |

具体命令见 [字体说明](../../components/board_7b/fonts/README.md)。

## 待改进

修改清单 P3 中与本工具相关的项：

- 默认端口不再写死 COM6、COM8；
- 自动查找 ESP-IDF 和 Python，或通过环境变量配置；
- 为 ATOM 工程提供同样的包装脚本，或给 `idf.ps1` 增加 `-Project` 参数。
