# Matrix LED要件

[English](../../en/request/matrix-led-request.md) · [简体中文](../../request/matrix-led-request.md) · **日本語**

ATOMが5×5 LEDを所有し、LCDは色を送信しません。論理原点は左上 `(0,0)`。低輝度、固定位置、異なる障害形状で、serialなしでも判読できることを要求します。

起動は白5列：LED、周辺、storage、Bluetooth、HID Host。完了列は常灯、現在列は250ms点滅。全列300ms保持後normalへ。同期初期化失敗はlog/resetし、最終frameが段階を示します。LED自体の失敗では表示を保証できません。

| normal位置 | 意味 |
| --- | --- |
| 行0/1/2 | Classic電量青、BLE青緑、gimbal紫。 |
| 行3 | LCD起動待ち、黄一点125ms移動、起動から最大10s。 |
| `(0,4)` | LCD黄待機／緑online／赤切断。 |
| `(1,4)` | 予約・消灯。 |
| `(2,4)/(3,4)/(4,4)` | Classic青／BLE青緑／gimbal紫。 |

電量は20%ごと切上げ最大5点、既知≤20%（0含む）は最低赤一点250ms点滅。不明・切断は消灯。無線offは消灯、探索2sごと125ms、接続500ms周期50%、ready常灯。readyは有効入力または制御・通知初期化で、接続開始だけではありません。

優先度はBluetooth紫B → I²C橙感嘆符 → overflow黄行0/2/4 → normalで、全25点を覆います。I²Cは2s内不正3回で表示、合法3回で解除。heartbeat切断はprotocol障害ではありません。overflowは5s、再発で延長。LED更新失敗はlog/retryし、Bluetooth/I²Cを停止しません。

BLE/コントローラー/gimbalのsourceは実装済みですが、電量fieldは実確認が必要です。四隅、起動失敗、同時接続、不明・stale電量、障害優先・解除、約1500ms LCD切断、30分RMT/BT/I²Cを検証します。[描画](../design/matrix-led-design.md)と[現状](../development/current-status.md)を参照してください。
