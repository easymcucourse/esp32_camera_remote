# LCD 字体资源

英文界面使用 Inter 4.1 Regular，中文回退使用思源黑体 Source Han Sans SC 2.005 Regular，参数中的数字及 `./+-` 使用 JetBrains Mono 2.304 Regular。普通界面的数字（例如密码）仍使用 Inter。界面当前保持英文，中文支持用于 UTF-8 文本及后续中文标签，并不自动翻译界面。

三个嵌入文件均为原字体的裁剪版本，总计 1,949,944 字节：

| 文件 | 原字体 | 字符覆盖 |
| --- | --- | --- |
| `inter_ui.ttf` | [Inter](https://github.com/rsms/inter/releases/tag/v4.1) | Latin-1 及常用符号 |
| `han_ui.otf` | [Source Han Sans SC](https://github.com/adobe-fonts/source-han-sans/releases/tag/2.005R) | GB2312 字符集及 Latin-1，共 7621 个字符 |
| `mono_ui.ttf` | [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono/releases/tag/v2.304) | Latin-1 及常用符号 |

超出覆盖范围的字符显示 `?`。三个子集依旧遵循 OFL 1.1，并随本目录附带原始版权和许可证。修改后的内部字体名称为 `Camera UI Sans`、`Camera UI Han`、`Camera UI Mono`，以遵守保留字体名称要求。随固件分发字体时应同时提供这些许可证及本说明。

FreeType 2.14.3 通过 ESP-IDF 组件管理器获取，固定版本见 `../idf_component.yml` 和根目录 `dependencies.lock`。FreeType 的许可和版权说明位于下载的组件中：`managed_components/espressif__freetype/freetype/docs/FTL.TXT`。本固件包含 FreeType：Portions of this software are copyright © 2026 The FreeType Project (www.freetype.org). All rights reserved.

## 渲染方式

`../ui_fonts.c` 直接用 FreeType 对轮廓字体按目标像素字号进行灰度抗锯齿渲染，并将覆盖率混合到 RGB565 帧缓冲。接口字号范围为 8–96 px；越界值会限制到该范围。无须引入 LVGL。

首次使用某个字体、字符、字号组合时生成字形，随后重用。LRU 缓存最多 384 个字形，灰度像素内存不超过 256 KiB，缓存及 FreeType 工作内存优先使用 PSRAM。缓存不包含嵌入字体本身；字体文件作为只读资源嵌入应用。现有 `CONFIG_SPIRAM_RODATA` 会把只读数据映射到 PSRAM。

绘制与测量必须持有app_ui 渲染互斥锁。文字支持 UTF-8、中文回退、统一基线、行高、换行及左右/帧缓冲边界裁剪。布局按实际字宽计算；窄参数面板自动减小字号，连接页的 SSID 和密码各用一行并按宽度缩小字号。

FreeType 灰度光栅器有 16 KiB 的栈内工作区，因此调用绘字的主任务、配对任务和 JPEG 解码任务均配置为 32 KiB 栈；不能使用原先 3.5–8 KiB 的栈。

## 重新生成字体

日常构建直接使用本目录内已生成资源，不必安装字体工具。需要调整覆盖范围时，在根目录执行：

```powershell
python -m pip install -r tools/requirements-fonts.txt
python tools/prepare_fonts.py
```

脚本从固定上游版本下载源字体，原始文件缓存到忽略提交的 `.reference/fonts/`，并生成 `manifest.json` 中的来源、字符数量和 SHA256。随后重新执行 `idf.py build`。

## Windows 本机预览与验证

安装 `tools/requirements-fonts.txt`，并先构建一次 ESP-IDF 工程以下载 FreeType 头文件。需要本机 GCC（例如 MinGW-w64）。`tools/font_preview_host/preview.c` 使用同一份固件 `ui_fonts.c`，验证数字对齐、UTF-8 错误处理、缓存淘汰、帧缓冲边界、裁剪和抗锯齿，输出连接页、参数面板和字号样例。

```powershell
$ftDll = python -c "import freetype, pathlib; print(pathlib.Path(freetype.__file__).parent / 'libfreetype.dll')"
gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter `
    -Itools/font_preview_host/compat -Icomponents/app_ui/include -Icomponents/app_ui/private `
    -Imanaged_components/espressif__freetype/freetype/include `
    components/app_ui/ui_fonts.c components/app_ui/camera_settings.c `
    tools/font_preview_host/preview.c `
    $ftDll -o build/font-preview.exe
$env:PATH = "$(Split-Path $ftDll);$env:PATH"
.\build\font-preview.exe
python -c "from PIL import Image; from pathlib import Path; [Image.open(p).save(p.with_suffix('.png')) for p in Path('build').glob('font-*.ppm')]"
```

预览是电脑端运行固件绘字代码生成的图像，不是实物 LCD 拍摄照片。FreeType DLL 版本可能与固件不同，最终显示以设备为准。
