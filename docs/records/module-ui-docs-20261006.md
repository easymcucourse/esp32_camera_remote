# 2026-10-06 UI 设计与测试覆盖核对

UI设计更新生产17行/九项导航、Wi-Fi只信息、移除维护入口、偏好boot-load-only/Web重启；显示生命周期补Core依赖排空→model clear/freeze→固定MAINTENANCE，保留display/font对象和generation。camera-menu设计把ATOM业务直接调用改为Input→typed UI route→Camera message，删除旧维护/热点确认。

testing.md顶部更新253当前条目，保留原54基线表并区分legacy-only旧auth/editor/factory/fd/reservation辅助与当前生产实现；补真实被测组件、fake边界和CI component/symbol gates。日期记录保持历史范围，不把旧计数/烧录状态当当前事实。

覆盖审计发现：surface真实测试确有duplicate acquire、两处mutex timeout、copy/stale拒绝、cancel/reacquire、refresh失败禁写与recover新generation。UI frame/endpoint/bench fixtures的app_ui_show_jpeg均为stub，ui_render_stop编译真实ui_renderer但stub ui_jpeg_reset。因此没有直接ui_jpeg_renderer的坏header、中途fast/ROM decode失败、不发布半帧、缓存reset及后续good frame测试，不能称V2/A6整体通过。该缺口已在UI/testing及checklist显式记录，下一步添加真实renderer故障矩阵；源码只有成功decode路径publish不能代替测试。

重跑display_surface/UI model/frames/bench/endpoint/renderer stop相关7/7通过，日志build/module-ui-docs-host.log；docs-only未额外构建。最新全host253/三LCD仍module-input-contract批次，无烧录/实机/remote CI/提交/推送，无live handles。当前doclinks144文档574链接0issues/diff通过，追加record后最终计数另记。完整目标active，尚需JPEG直接测试、Sony/剩余开发文档及全S/V/A38/五配置/hardware证据。
