# 2026-10-06 HTTP SoftAP监听隔离

本批完成独立维护应用的监听前置条件，尚未实现完整S3.1/S3.2。ESP-IDF 5.5.1的HTTP配置没有监听地址选项，当前SDK使用双栈ANY TCP监听。项目组合根仅对esp_http_server target替换lwip_bind为wifi_esp32_http_bind，并显式增加该SDK target到所选Wi-Fi后端的链接边；没有修改SDK文件，其他socket调用和ATOM独立工程不受此配置影响。

私有wifi_esp32/wifi_http_scope.c在监听前查询已启动的WIFI_AP_DEF和当前IPv4地址，以SO_BINDTODEVICE绑定实际SoftAP接口，IPv4绑定具体AP地址；双栈套接字使用IPv4-mapped地址，SDK lwIP bind会解映射。保留请求端口，不修改原sockaddr。内部控制UDP仅允许IPv4回环地址，不绑定无线接口。AP不存在、未up、地址不可用、接口名查询或setsockopt失败均拒绝监听，没有ANY回退。私有socket钩子不进入app_wifi或应用层公共API，无新任务、队列、帧缓冲。

新增IPv4-only和IPv6实际后端源码fixture，覆盖具体地址/端口/原参数不变、IPv6映射、回环UDP、错误传播和失败不调用native bind。123/123主机测试通过，原54注册与旧断言保留。主机fake不证明真实网络接口可达性或SDK/SMP行为。

LCD Default 0x35c1c0、Stable 0x35b3a0、Release 0x3501d0当前源码构建成功，均小于5MiB/6MiB分区。三个compile_commands各7个HTTP SDK源均带局部alias，Wi-Fi钩子自身及所有非HTTP源无alias。三构建实际httpd_main/ctrl_sock对象引用wifi_esp32_http_bind、不引用lwip_bind；后端对象引用原生lwip_bind，生产ELF存在hook，证明局部调用路由且无递归。Release禁止的SIM/bench/Debugfault/encoder/维护UART符号无命中。日志build/module-http-scope-{host-build,host,default,stable,release}.log。

上述链接边是在project()之后添加，生成的project_description不一定枚举它；最终依赖图必须根据实际CMake target/对象/ELF将HTTP SDK→Wi-Fi backend边列入，不能以旧manifest宣称完整无环审计完成。当前旧HTTP同样经过该限制；独立app_maintenance、启动触发、认证删除、不可逆Core模式及旧compat/WHOLE_ARCHIVE清理仍待完成。

ATOM本批未重建。无烧录、实机、提交或推送。所有本批构建会话终态，无本批后台串口/构建，其他聊天未检查。真实STA/其他netif不可访问、SoftAP请求/双栈SDK监听效果待实机验证；完整目标保持active。

最终边界/文档/diff门禁通过：122文档、526本地链接、0问题。
