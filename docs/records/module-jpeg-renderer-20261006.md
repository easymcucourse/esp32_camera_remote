# 2026-10-06 真实 UI JPEG renderer 主机故障矩阵

新增test_ui_jpeg_renderer.c直接编译当前生产ui_jpeg_renderer.c，Debug/Release两个独立目标保留-Wall/-Wextra/-Werror。stub仅替代FreeRTOS、allocation、ROM/fast encoder/decoder库和surface/renderer协作，不替代被测app_ui_show_jpeg或内部成功/失败分支。

覆盖null/短输入、关闭gate、surface未就绪/acquire失败、4096 workspace allocation失败、坏ROM header/不支持尺寸、fast open失败、parse失败/尺寸不匹配/outbuf长不匹配/NO_MEM、fast部分输出后失败及NO_MEM、ROM输出部分tile后失败、fast→ROM切换close、每次失败后good frame。所有decode失败断言publish计数不变、front两采样值不变、无overlay、canvas归还、mutex/user退出、JPEG pixels清空；invalid-response/no-mem后decoder关闭，成功创建与close计数最终相等。publication失败只触发失败提交、不换front，禁写直到fake readiness恢复并reset；settings真实image_shrink/清右栏下方断言通过。

Debug另外测试真实app_ui_test_jpeg：encoder NO_MEM时length=0且归还画布、不publish；成功生成后同app_ui_show_jpeg decode/overlay/publish入口处理。Release同接口NOT_SUPPORTED。这不证明真实codec字节正确、encoder画质、cache-off/SMP或RGB硬件，仍需实机；surface所有权规范由独立真实surface测试补充。

首轮链接缺ui_render_surface_ready stub，补正确依赖；随后fixture把失败open尝试算成成功handle造成计数assert失败，改为仅成功open计数，未修改生产代码或放宽失败/释放断言。完整255/255通过，原54保留。日志build/module-jpeg-renderer-{host-build,host}.log。无生产实现行为变化，不额外固件构建；最近三LCD仍module-input-contract，ATOM未涉及，remote CI未跑，无烧录/硬件/提交/推送/运行handle。

UI/testing/checklist更新直接renderer覆盖，前批module-ui-docs识别缺口的记录保留历史范围。本批补齐主机control-flow缺口而非宣称V2/A6硬件验收。完整目标active，下一步Sony/开发文档、public/contracts/indirect ops、所有owner错误退出及全S/V/A38/five-config最终验证；硬件证据未完成。

最终boundary、doclinks146文档578链接0issues、diff均通过。
