# 2026-10-06 独立无认证维护Web与共享偏好存储

实际HTTP路由、gzip页面、JSON形状/热点patch helper已从main迁入app_maintenance；main只剩entry和过渡maint_mode控制器。生产maint_auth.c/.h删除，原认证fixture原样移至tests/support/legacy以保留原54回归。网页不再含PIN、登录、会话、Authorization/Bearer/cookie请求；/api/info明确authentication:none，页面说明热点任意客户端拥有全部维护权限。没有CORS通配或外部资源。HTTP仍通过前批SoftAP-only bind hook，任务priority3/6144内部栈/socket3、10秒收发与原限制保持，无新增HTTP任务/队列/画布。

Web只直接调用app_wifi与HTTP/OTA/JSON/SDK基础库；配置/重启通过Core注入available/touch/restart/settings回调。删除相机状态读取和对UI偏好运行时setter的调用。正常应用配置POST校验type与INFO、拒绝未知/重复/非整数字段，写成功后1500ms重启，即使最终ACK丢失仍commit；重启/退出均要求confirm:true，退出不恢复正常功能。原Wi-Fi prepare→回执成功commit1500ms、回执/commit失败cancel保留；token结果跨AP generation变化保留供重连查询，而非清空历史。patch新增show_password布尔，随机密码/SSID/密码/信道校验保留，当前密码不在GET响应中暴露。

Core配置回调只持久化、不调用UI运行时。最初直接Core读写ui_prefs触发单一所有者门禁；已将所有ui_prefs NVS实现统一为common_runtime/preferences_store.c（公共纯值record/common API），Core与原UI worker都调用相同存储层。schema=1，缺失标签视为legacy v1，load不改NVS，未知schema拒绝并保留；Core三个key通过同一handle/一次commit，原UI异步worker3072/prio2/queue4+8/请求结果/stop保持。正常启动从该层加载，未知schema使用完整INFO/DS默认值，新增回归覆盖。NVS真实断电原子性/失败物理回滚未经验证，不能把fake set/commit证明当实机存储保证。

仍是阶段性实现：新Web生产ELF可达，但正常startup仍启动旧maint_ctl、尚未提供startup :80 trigger，不能以链接可达证明可从启动页进入。旧Core上传/维护兼容回调及Main WHOLE_ARCHIVE仍在。旧控制器仍相机预约/可恢复/LCD菜单和超时逻辑，fixed MAINTENANCE尚未Core绑定；当前Core settings_write先quiesce正常偏好worker以避免迁移期双owner并行写，最终必须用完整独占切换替代局部关闭。恢复出厂Web、启动trigger/不可逆Coremode及全部旧控制器清理仍未完成。页面所述仅startup进入是目标流程，暂不得作为已实现用户操作指南。

167/167主机通过，原54/旧断言保留。新12场景编译实际maintenance_web/JSON/patch及实际SDK cJSON，SDK/Wi-Fi/HTTP生命周期均fake：无认证路由/info、settings校验/保存失败/丢ACK、available拒绝、重复key、Wi-Fi回执/commit失败cancel、generation后结果可查、start/注册/stop失败保留及API version拒绝。另8场景真实存储层/Core adapter覆盖正常/legacy/missing/future/read/open/set/commit错误、输出不变与close；原UI实际worker加入未来schema重启加载默认测试。OTA/header/auth原回归仍执行，独立Web目标不链接Core/Camera/Input/UI/Console实现，资源仅host两字节假gzip，非视觉/浏览器/SDK网络证明。

主机Web测试新增parser依赖：Linux CI配置安装libcjson-dev；本机使用未修改ESP-IDF 5.5.1 cJSON，通过MODULE_CJSON_SOURCE_DIR配置，build/host缓存保存路径；支持CMake3.16并在缺parser时明确报错。Linux CI/ASan尚未在本机执行，不宣称绿。网页脚本已node --check通过，gzip构建打包通过，未浏览器视觉/真实请求。早期CoreSettings fixture NVS mode类型及一次Python编辑脚本没有匹配返回点的问题已修正/重新应用；没有放宽原测试。

最终LCD Default0x35bc90 Stable0x35ae70 Release0x34fce0成功，均<5MiB/6MiB分区；ATOM双模Debug0x1043f0/Release0x1017f0构建成功。compile graph新Web/helper/sharedstore单一owner、无main旧Web/auth/helper，生产ELF新API与SoftAP hook可达、auth/旧Web符号不存在；实际Web对象不引用Core/UI/Camera/Console/oldWiFi/oldmode；Release无SIM/bench/fault/encoder/maintUART。最终日志build/module-maintenance-web-{host-config,host-build,host,default,stable,release,atom-debug,atom-release}.log。

所有本批构建会话终态，无本批后台串口/构建，其他聊天未检查。未烧录、实机、提交或推送。完整目标active，下一步Core startup trigger与不可逆独占切换、恢复出厂Web、旧maint_mode/compat/WHOLE_ARCHIVE清理、固定UI画面和全项审计。

最终边界/文档/diff门禁通过：124文档、536本地链接、0问题。
