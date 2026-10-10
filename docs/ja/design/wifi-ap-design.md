# Wi-Fi所有権

[English](../../en/design/wifi-ap-design.md) · [简体中文](../../design/wifi-ap-design.md) · **日本語**

Coreが唯一 `app_wifi` objectを作り、`wifi_esp32` がSDK/netif/socket/NVS、message bridgeが通常探索/RSSI/statusとTCP二laneを所有します。PTPは裸fdを借りません。maintenanceは通常停止後に隔離facadeを直接使います。driverはRAM、app configだけ永続です。

SSID1–32byte（control/DEL拒否、UTF-8可）、printable ASCII password8–63、country範囲内channel1–13、show_password bool。WPA2/PMF必須なし/4client固定。random12文字は拒否sample、生成だけでは保存しません。

`wifi_ap/cfg` v1固定100byte：0version/1channel/2flags(bit0表示)/3SSID長/4–35SSID/36password長/37–99password。zero padding/flags/長さ/文字を厳密検査、未知版は不正。defaultはcfgだけ削除commit、全NVSではありません。

Core initは自動全消去しません。backend readは不正・過大recordをdefault writeで修復し得るため、Core writerなしは全起動Flash-writeなしを意味しません。storage mutexで直列。country不正でcodec合法のfallbackはRAMだけの場合があります。namespace開失敗は診断し消去しません。

Webはrestart予約→prepare→ACK→commit。queue2/history8は未完保持。workerはvalidate→save→必要network変更だけAP再起動、失敗は旧record/AP復元試行。Coreは結果追跡1500ms再起動。保存+radioや複数namespaceは非原子。通常write messageはNOT_SUPPORTEDです。

実IP/password/default labelを値stateでUIへ。RSSIは選択Camera MAC約2sでphone順序ではない。network世代で旧PTP channelと遅reply無効、lease終了までbuffer保持。factoryは[maintenance](maintenance-design.md)。実channel12/13、Flash、AP再起動/ACK消失、reset再登録は別検証。[要件](../request/wifi-ap-request.md)と[所有権](module-resource-ownership.md)を参照してください。
