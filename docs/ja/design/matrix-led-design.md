# Matrix描画設計

[English](../../en/design/matrix-led-design.md) · [简体中文](../../design/matrix-led-design.md) · **日本語**

純C `matrix_model` は時刻とコピーstateから25pixel、`matrix_status` がGPIO27/RMTと限定retryを所有します。BT/I²C ownerはstateだけ公開。button39 active-low、logical index y*5+x/左上原点、実四隅は目視検証です。

起動5列・通常電量/link/faultは[要件](../request/matrix-led-request.md)。上3行Classic/BLE/gimbal電量、4行目LCD待ち、最下LCD/予約/Classic/BLE/gimbal。既知0は低電量、不明255/offlineは消灯。Mini電量は厳密DUML、15sで期限切れ、値を捏造しません。

HID非同期初期化3s、起動完了300ms。LCD待ち起動から10s、heartbeat1500ms。探索125ms/2s、接続500ms50%、ready常灯。不正request実時刻2s内3回でI²C error、合法3回で解除。overflow5s延長。優先はBT B→I²C感嘆符→overflow横条→normal。

共有 `ble_clients` だけがBLE callback/scanを登録しapp/interface/peer配信、ClassicはHIDを維持。BTDM/BLE/GATTCと適切cache/notify/connection数が必要、旧BR/EDR-only設定を再生成します。BLEコントローラー/標準電量/Miniは実装済み。不明inputは電量だけ取得可能でもCamera制御しません。

Debug led testは四隅、forced faultは表示だけでLCDへ偽linkを送りません。RAMのみ、Release除外。RMT失敗はlog/retryで他ownerをresetしません。model/FIFO fixture/buildは実方向・RMT/SMP・30分を証明しません。[hardware](hardware-design.md)、[I²C](i2c-protocol-design.md)、[現状](../development/current-status.md)を参照してください。
