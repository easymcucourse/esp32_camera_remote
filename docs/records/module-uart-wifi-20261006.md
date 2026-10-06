# 2026-10-06 UART Wi-Fi 消息编码

`main/wifi_console.c` 移除 `wifi_ap.h` 及所有 Wi-Fi/Core 业务函数调用。配置和信道范围来自 Wi-Fi STATUS，修改使用 PREPARE/COMMIT/RESULT，关联客户端数量来自发现快照；factory wifi 写入原默认纯值，factory all 使用 System FACTORY_RESET/RESULT。参数检查、十秒确认、异步 token、新密码仅成功后显示及清除暂存密码保持。

PREPARE 和 COMMIT 共用绝对期限。COMMIT 传输失败可能已经执行，因此保留 token，尝试取消仍暂存的事务，并查询最终结果；RESULT 传输失败继续等待。UART 或目标 endpoint 换代后清除旧事务，避免错误展示旧密码。消息 payload 按实际 union 成员初始化，避免残留配置字节成为提交延时。

新增 `uart_wifi_messages` 主机测试覆盖信道限制、提交/查询超时、成功/失败密码展示、端点换代、出厂确认过期及两个目标、普通 show 不泄露密码。主机 100/100，通过 `-Wall -Wextra -Werror`；原 99 项保持。LCD Default/Stable/Release 编译成功，尺寸分别 `0x35b4c0` / `0x35a6a0` / `0x34e920`，均小于 5 MiB。日志为 `build/module-uart-wifi-{host-build,host,default,stable,release}.log`。本批未修改或重新构建 ATOM，前批双模构建证据保留；未烧录或运行实机脚本。

这是完整 UART gateway 迁移的前置步骤：编码器暂仍在 main，UART 独立生命周期、其他业务命令消息化、基准归 UI、启动独占维护、Core 完整停止编排和最终验收继续待完成，不构成完整计划完成。
