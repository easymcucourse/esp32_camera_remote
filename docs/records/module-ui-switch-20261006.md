# 2026-10-06 JPEG renderer 连续模式切换

`tests/host/test_ui_jpeg_renderer.c`继续直接编译生产renderer，增加Debug/Release各32次LIVE/SETTINGS交替。每次保存原front全帧64位校验值，成功发布后检查原front保持不变、新front切换、canvas writer/mutex/render user退出、jpeg_pixels归零。所有既有解码失败用例也增加整帧校验，保留原采样断言。

连续帧仅创建一次4096字节workspace和一次fast decoder，workspace地址不变；reset后malloc/free与decoder open/close配对。LIVE检查12行上/下黑边和576行图像边界，SETTINGS检查768×432缩图边界、右/下清零。fake codec输出固定绿色，overlay仅检查back地址；本测试不证明真实JPEG像素或文字效果。

源码观察：生产fast decoder直接写canvas.pixels的纵向偏移；SETTINGS使用同一canvas原地image_shrink。项目renderer只有4096字节work allocation，无新增第三个全屏buffer或全屏memcpy。清黑边/清栏的memset仍存在；SDK codec内部allocation/copy不由本测试证明，真实RGB扫描、cache-off/SMP与稳定性仍待实机。

验证：host build与262/262 CTest通过（原54保留），日志build/module-ui-switch-host-build.log、build/module-ui-switch-host.log。本批只改测试与文档，未改变生产实现，沿用UI vendor清理批LCD三配置和此前ATOM两配置编译证据；无烧录、实机、提交、推送或remote CI。
