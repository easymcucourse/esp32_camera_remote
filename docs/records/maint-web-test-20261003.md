# 2026-10-03 维护网页基础与启动栈回收

本轮接入维护基础入口，不改变分区表，没有 OTA 上传。热点 / 相机身份 / UI 偏好没有主动修改，常规 USB 烧录保留 NVS。ATOM 未重烧。

## 实现范围

- `maint_mode`：常驻控制任务、异步串口 on / on stop / off / status、十分钟活动计时、相机 lease 暂停 / 恢复、OpenSession 通知。
- `maint_auth`：六位拒绝采样 PIN、五次错误换 PIN并锁定六十秒、128 位随机单会话令牌、定长比较及 token 校验。
- `maint_web`：内置 gzip 单页、登录 / 登出、设备信息、网页退出；除页面和登录外需要 Bearer，JSON 请求最多 512 字节。
- LCD 只提交文字和页面切换，由现有 PSRAM 显示任务绘制。停止后不会因最后一帧保留而隐藏 PIN。
- 开发 `maint probe` 通过 AP 的本地 IP / TCP 使用实际 HTTP 服务；不输出 PIN、令牌或客户端身份。关闭模拟时不编译探测任务。
- `app_main` 完成初始化后返回，由 IDF 删除其 32 KiB 字体初始化栈；4 KiB 内部 RAM 的 `health` 任务接管内存日志和耗尽后排空 / 重启。

## 验证

| 层级 | 证据 |
| --- | --- |
| 主机 | 43/43 CTest；新增 maint_auth 覆盖拒绝采样、锁定 / 时间回绕、正确 PIN、旧 token 失效、登出与非法十六进制 |
| LCD 构建 | 开发 / 关闭模拟两种最终构建成功，`build/maint-health-build.log`、`build/maint-health-release.log` |
| 烧录 | COM8 / 460800，最终 `build/flash-maint-health.log` 校验通过；NVS 未擦除 |
| HTTP / 控制任务 | `maint-web.uart` 13 条串口命令通过，两轮各 14 个真实 HTTP 请求通过；最终 `build/maint-health-web-test.log` |
| 显示 / 重启 | `lcd-display-recovery.uart` 5 条命令通过；一次故障恢复、持续三次耗尽后 camera_drained=1 / NVS preserved / READY；`build/maint-health-display-test.log` |
| I²C 回归 | `pair-ui-info.uart` 11 条命令通过，恢复 full / 退出模拟；`build/maint-health-pair-ui.log` |
| 页面内置 | gzip 1403 字节，解压后与 HTML 源文件逐字节相同 |

14 个 HTTP 请求覆盖页面 / gzip 头、未登录 401、错误 PIN、正确登录 / 设备信息、新登录使旧 token 401、新 token 可用、登出后 401、非法 JSON 400、513 字节请求 413、重新登录与网页退出。网页退出关闭服务并释放相机 lease；随后相机任务重新等待已配对相机。

首轮探测没有通过。最初使用未建立独立 loopback netif 的 127.0.0.1，且接收端等待连接关闭；最终改为读取 AP 本地地址并按 Content-Length 收完整响应。两项改动一起复测通过，未单独隔离哪一项导致首轮失败，不把该失败归咎于已证明的服务错误。

## 内部 RAM

同一新增维护固件，回收启动栈之前：空闲约 24935 字节，维护服务开启约 15971 字节，探测日志曾采到 12179 字节，余量偏低。

回收之后：空闲 55775 字节，维护开启 / 相机 lease 已取得时 46835 字节；两轮退出后的样本 52679 / 52459 字节。这里只是窗口样本，不是长期最低值或泄漏结论，也不是未来 OTA 上传负载结果。

## 尚未证明 / 尚未实现

- 回环经过设备 TCP / HTTP，但没有经过手机的 Wi-Fi 空口；浏览器视觉、手机访问与多个外部客户端待验收。
- 六十秒锁定 / PIN 变化和十分钟自动关闭仅有相应纯逻辑 / 源码，尚无完整实机计时验收。
- 本轮相机 session=0，未证明在线相机停止 / OpenSession 自动关闭时延、实际 held S1/S2/zoom 释放及取景恢复。
- 手柄维护入口、自动关闭提示三秒消失、热点网页修改 / 重新登录、网页重启、OTA 分区 / 校验 / 上传 / 回滚继续实施。
- LCD / Matrix 的物理视觉与至少三十分钟稳定性仍待验收。最终维护关闭、camera auto-start、full 信息、SIM0；不标记全需求完成。
