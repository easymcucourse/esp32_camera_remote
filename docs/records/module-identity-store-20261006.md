# 2026-10-06 配对身份共享存储所有权

配对 GUID/peer 持久化实现从 app_camera/camera_identity.c 移至 common_runtime/camera_identity_store.c，值类型及 primitive contract 从 Camera private header 移至 common/camera_identity.h。没有增加转发实现。sony_remote namespace、guid[16]、peer[22]、旧 GUID 保留、孤立 peer/错误长度拒绝、完整相机初始化后确认和清除失败语义不变；存储格式未变化，因此本次不引入 schema 迁移。

正常 Camera 仍通过 camera_identity_work.c 的原 4096 字节内部 RAM 栈、priority4 一次性 worker 访问存储，避免 PSRAM 栈执行 cache-off flash 写入。common_runtime 只提供持久化 primitive，不依赖 Camera runtime、backend 或 message bus。Core 必须先停止正常写入者，再从内部 RAM HTTP 栈执行维护清除；本次尚未接入 Web 恢复出厂，也未删除原正常恢复出厂 worker/入口。错误日志删除旧 UART 重置提示，指向计划要求的维护网页。

Camera CMake 删除旧存储源及直接 nvs_flash/esp_hw_support 依赖，改用 common_runtime；shared component 注册唯一实现与 SDK 依赖。原 camera_identity 和 identity_work 测试目标仅调整源/头路径，测试断言未改。边界检查新增 sony_remote NVS 访问只允许 common_runtime 的规则。

验证：主机构建成功，CTest 204/204；包括旧记录读取、配对确认、损坏拒绝、失败提交/清除与内部 worker 完成同步。LCD Default0x35bdf0、Stable0x35afd0、Release0x34fed0 三构建终态成功，均小于5MiB/6MiB应用分区。实际三套 compile_commands 均只有一个 common_runtime 配对存储对象，无旧 Camera 存储源。日志 build/module-identity-store-{host-build,host,default,stable,release}.log。ATOM未涉及，未重新构建；没有烧录、实机或 flash/cache-off 验证。完整计划仍在进行。
