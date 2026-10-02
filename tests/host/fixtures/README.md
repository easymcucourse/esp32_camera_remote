# Sony 属性回归样本

这些二进制样本由本地原始抓包的成功 `0x9209` 事务裁剪而来，只保留 `sony_codes.h` 已命名的整数标量属性条目，重新写入条目计数。保留默认值、当前值、读写标志和两组枚举列表的原始字节；不包含 GUID、MAC、序列号、字符串、未知属性、网络包或 JPEG。目标相机为 Sony ZV-E10 固件 2.00。

| 样本 | 来源抓包 | TCP stream / transaction | 原始长度 | 裁剪长度 |
| --- | --- | --- | --- | --- |
| `sony-props-193841.bin` | `remote-20260927-193841.pcapng` | 0 / 9 | 2811 | 1182 |
| `sony-props-195145.bin` | `remote-20261001-195145.pcapng` | 2 / 9 | 2661 | 1040 |
| `sony-props-200352.bin` | `remote-20261001-200352.pcapng` | 2 / 9 | 2672 | 1085 |
| `sony-props-200950.bin` | `remote-20261001-200950.pcapng` | 4 / 9 | 3210 | 1500 |

`test_property_fixtures` 验证完整快照的曝光枚举、当前值、Focus 和 ZoomEnableStatus，并验证所有截断点不会发布属性或授权写入。原始数据的完整遍历和逐字节截断也已在本地主机测试中运行；这些裁剪样本不能代替未知属性、实际镜头识别或实机写入效果的验收。

用 `tools/extract_property_sample.py` 可从本地抓包提取完整属性数据，例如：

```powershell
python tools/extract_property_sample.py captures/remote-20261001-200950.pcapng captures/props-200950.bin --stream 4 --tshark 'C:\Program Files\Wireshark\tshark.exe'
build/host/test_focus_caps.exe captures/props-200950.bin
```

完整数据含未知相机属性，保留在已忽略的 `captures/`，不提交到仓库。
