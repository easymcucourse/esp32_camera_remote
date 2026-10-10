# 抓包工具

[English](../en/tools/capture.md) · **简体中文** · [日本語](../ja/tools/capture.md)

本文说明抓包和离线分析脚本的用法。各轮实测数据见 [通信分析与实测记录](../records/protocol-analysis.md)。

## 依赖

安装 Wireshark（含 `dumpcap`、`tshark`；Windows 需要可用的抓包驱动）。`extract_liveview_sample.py` 可选使用 Pillow 验证 JPEG 尺寸。

## 抓包：`tools/capture.ps1`

先用 `dumpcap -D` 查询网卡编号，然后执行：

```powershell
.\tools\capture.ps1 -CameraIP 192.168.110.46 -Interface 1 -Seconds 120
```

| 参数 | 说明 |
| --- | --- |
| `-CameraIP` | 必填，相机 IP |
| `-Interface` | 必填，`dumpcap -D` 列出的网卡编号或名称 |
| `-Seconds` | 抓包时长，10–600，默认 120 |

过滤条件为相机 IP 的全部流量加 SSDP（UDP 1900）和 mDNS（UDP 5353）发现流量，输出到 `captures/remote-<时间>.pcapng`。抓包期间建议：重新连接 Remote，等待 10 秒，每次只执行一个操作并记下时间，最后断开。

抓包范围和不得提交的内容见 [通信记录的公开范围](../README.md#通信记录的公开范围)。

## 分析：`tools/analyze.py`

```sh
python tools/analyze.py captures/your-capture.pcapng
python tools/analyze.py captures/your-capture.pcapng --tshark "D:\Wireshark\tshark.exe"
```

用 tshark Follow TCP 重组每个方向的数据，再按 PTP/IP 的 length/type 拆包，输出同名 `.ptpip.csv`。CSV 按方向排列而非时间线，时序以原始抓包为准。

## 历史取景样本：`tools/extract_liveview_sample.py`

首轮抓包的复现脚本，固定读取 `captures/remote-20260927-193841.pcapng` 的 TCP stream 0、事务 11，导出一个取景对象和其中的 JPEG。原始抓包不随仓库提供。修改清单 P3 计划让它接受抓包文件、stream 和事务号参数，用于导出测试样本。


## 属性样本提取：`tools/extract_property_sample.py`

该工具从指定抓包的成功 0x9209 事务导出完整属性数据，--stream 必填；--transaction 可指定事务号，省略时导出该流中首个完整且成功的匹配数据集；--tshark 可指定程序路径，--max-packets 默认 50000。输出目录自动创建，找不到对应属性事务会报错。

```powershell
python tools/extract_property_sample.py captures/remote-20261001-200950.pcapng captures/props-200950.bin --stream 4 --transaction 9 --tshark "C:/Program Files/Wireshark/tshark.exe"
```

完整输出可能含未知属性，应保留在已忽略的 captures/。仓库 fixtures 是另外裁剪出的整数属性样本；来源、裁剪规则和提交范围见 [样本说明](../../tests/host/fixtures/README.md)。
