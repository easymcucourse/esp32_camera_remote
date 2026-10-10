# Wi-Fi AP要件

[English](../../en/request/wifi-ap-request.md) · [简体中文](../../request/wifi-ap-request.md) · **日本語**

LCDはWPA2-PSK AP、DHCP、最大4client、PMF必須なしです。初期値は `common/network_config.h`、保存済み `wifi_ap/cfg` が優先します。通常gatewayは `192.168.4.1/24`、channel6。表示は実netif IPで、Camera IPを固定しません。

| 要件 | 動作 |
| --- | --- |
| R1初期値 | factory SSID/password/channelへ戻す。現在は `easycamctrl`、`00000000`、6。 |
| R2編集 | 起動maintenance Webだけで変更・password生成。SSID1–32byteでcontrol文字なし、password8–63 printable ASCII、channel1–13かつcountry上限内。不正は保存前に拒否。 |
| R3永続 | 電源再投入と通常app更新で保持。欠落・不正recordは診断付きdefault。全NVSを自動消去しない。 |
| R4表示 | SSID、password表示可否、LCD実IP。非表示は `********`。RSSIは選択Camera MACで、先頭phoneではない。 |
| R5reset | WebでAP/全scopeを確認。APはCamera識別保持、両方ともATOM登録保持。成功後再起動。 |

通常UART `wifi show` とコントローラーWi-Fi menuは照会だけです。`wifi show password` は明示的にpasswordを表示します。default表示はpassword値で判定し、SSID/表示可否とは独立です。12文字の生成passwordは適用まで保存しません。

旧UART/コントローラーeditorではなく現Webで、保存・再起動・再接続、7byte password/33byte SSID/channel14拒否、永続、不正record、非表示・IP、phone+Camera RSSI、両resetを検証します。country設定だけでCameraのchannel12/13対応を推定しません。[設計](../design/wifi-ap-design.md)と[検証](../development/current-status.md)を参照してください。
