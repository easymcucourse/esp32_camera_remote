# 2026-10-06 当前架构与维护设计同步

按当前源码重写 architecture-design.md 的 LCD 组件表、资源/队列边界、显示与持久化/双OTA说明；任务数值统一链接 module-resource-ownership.md，保留独立ATOM任务表及现有协议事实。删除主架构中已退休的main维护实现、wifi compat/editor、prefs worker、普通factory及旧factory分区描述。

maintenance-design.md改为当前不可逆Core模式、启动页trigger、无认证权限、独立ops注入、15 routes、Web配置/偏好/factory、OTA和Core60秒健康确认。按真实 missing_route/dispatch说明404/405也经过trigger；35秒exclusive等待不是SDK HTTP stop的严格预算。保留现有双OTA分区结构及首次USB更新说明，未执行烧录。

修正wifi-ap-design.md与testing.md的四个失效章节链接，并替换链接附近的PIN/手柄/串口维护入口文字。Wi-Fi设计其余大量旧编辑器/保存/菜单描述以及用户/开发文档尚待系统同步，不能称全部文档完成；正式旧需求由新拆分计划取代部分条目，原日期记录不改写历史。

文档链接141文档/572links/0issues，diff检查通过。纯文档修改未额外构建；最近源码门禁为module-symbol-owner批次252主机及LCD三构建通过，详见 [记录](module-symbol-owner-20261006.md)。无硬件/远程CI/提交/推送，完整目标active，下一步Wi-Fi/用户/开发当前文档同步和全S/V/A38证据审计。
