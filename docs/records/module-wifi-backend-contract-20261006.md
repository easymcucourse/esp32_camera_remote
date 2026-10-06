# 2026-10-06 真实 Wi-Fi ESP32 factory 契约测试

验收矩阵V30要求分别测试wifi_esp32、PTP client和Sony backend。原主机CMake只编译facade、jobs/TCP/storage/bridge，没有编译wifi_esp32.c；Core factory也是stub。本次新增test_wifi_esp32_backend.c直接编译实际wifi_esp32.c并链接真实app_wifi.c/network_config/async_token，SDK、NVS primitive、jobs与TCP子模块为边界fake。

通过公共factory/facade验证单实例、factory仅建锁/jobs而不初始化radio、两个mutex及jobs allocation失败清理；九个SDK初始化阶段失败都清理已取得的netif/driver/event/自建event loop，后续factory可重建。已有event loop不删除，自己创建的loop销毁配对。SDK配置检查driver禁NVS、storage RAM、JP/AP/WPA2/SSID长度/信道/连接容量；不证明SDKradio物理行为。

实际初始化后的storage lock被占时saved read/write拒绝；lifecycle lock占用时start拒绝。jobs start失败停止已启动radio，下一次成功启动；AP event与地址/online/generation正确，未知event不改变状态；reconfigure换generation。clients只返回有DHCP地址的peer，容量不足不返回partial count。jobs stop超时或SDK stop超时保留对象/locks/radio、拒绝destroy，重试成功后offline/代际变化再释放factory。归还检查全部fake资源配对，无强制删除。

首轮fixture与实际wifi_saved_read单参数、typed fake semaphore声明不符，Werror编译拒绝；按真实header修正，不改变生产代码或告警。新增目标C11/-Wall/-Wextra/-Werror。完整263/263主机通过（原54保留），日志build/module-wifi-backend-host-build.log与build/module-wifi-backend-host.log，单目标初验module-wifi-backend-contract-host.log。

本测试不覆盖真实任务SMP/radio/DHCP/NVS写或SDKcleanup失败；jobs/TCP/storage真实子模块已有独立fixture，不能把新stub结果冒称完整硬件启动安全。生产源未改，不重复固件构建，沿用最近Sony exposure LCD三配置与API批ATOM两配置。无烧录/实机/remoteCI/提交/推送。
