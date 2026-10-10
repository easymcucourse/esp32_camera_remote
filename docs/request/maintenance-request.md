# 维护页面需求

[English](../en/request/maintenance-request.md) · **简体中文** · [日本語](../ja/request/maintenance-request.md)

2026-10-10按当前源码核对。旧PIN/登录、可逆维护、手柄/串口维护入口和十分钟自动退出已被取代。当前为LCD启动页HTTP触发、无认证、排他维护，退出依靠重启。ATOM仍通过USB更新；无互联网自动更新。

## 需求

1. STARTUP在SoftAP端口80提供trigger，首个HTTP请求取得独占维护；进入NORMAL后关闭，须重启才能重新进入。
2. Core先关闭普通admission、完整释放输入、排空Camera/JPEG/普通服务、取消token并冻结固定MAINTENANCE画面，再启用完整Web。失败只重启，不能恢复部分停止的普通服务。
3. 任意加入热点的客户端拥有维护权限，没有PIN/token/cookie/login。Web资源内置，提供版本/build/分区/IDF/运行诊断、AP设置、DS/Xbox与显示偏好、两级重置、LCD OTA、退出/重启。
4. AP配置按 [Wi-Fi要求](wifi-ap-request.md)校验。staged prepare成功发送回执后才commit，Core独立追踪完成后重启；回执失败取消stage。UI偏好保存后重启加载，正常UART/手柄不写入。
5. 恢复出厂必须显式scope及confirm:true。wifi仅重置AP并保留相机；all另清LCD相机和UI偏好，但保留ATOM绑定和无关NVS。多个namespace失败可能部分修改，不宣称原子回滚。
6. OTA仅接受LCD application bin，镜像门禁5MiB，写非当前6MiB槽，完整校验后才选择启动分区。错误chip/project、过大、损坏、中断不得成为启动镜像。正常ready后60秒确认pending镜像；未确认重启保留rollback。退出、保存或OTA成功均重启LCD。

## 验收测试

- 首请求竞争、NORMAL拒绝、AP与其他netif隔离、手机/电脑实际browser。
- 全输入释放、各owner排空、固定画面及普通JPEG/菜单/bench拒绝。
- AP/偏好保存、ACK丢失、两级reset成功/部分失败、重启/重新连接/绑定保持。
- 预检与完整上传、错chip/过大/截断拒绝、上传断线/掉电、pending回退与健康确认。
- 内置资源在无外网热点可用。HTTP SDK join并无严格项目级3000ms保证，须实测阻塞请求停止。

当前源码/主机/构建已有证据；新拆分版完整实机维护验收仍未完成。历史设备回环OTA通过属于当时镜像，不覆盖当前全部场景。详见 [维护设计](../design/maintenance-design.md)、[当前状态](../development/current-status.md)。

被取代的原稿见 [历史归档](../records/maintenance-requirements-superseded-20261010.md)。

<!-- Preserve existing historical inbound fragments. -->
<a id="1-维护模式"></a>
<a id="2-登录"></a>
<a id="3-设备信息"></a>
<a id="4-热点设置"></a>
<a id="5-固件更新ota"></a>
<a id="6-其他"></a>
<a id="范围"></a>
