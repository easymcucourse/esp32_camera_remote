# M5Stack ATOM Matrix

[English](README.md) · [简体中文](README.zh-CN.md) · **日本語**

ATOM Matrixの従来ESP32用、独立ESP-IDF5.5.1ファームウェアです。I²Cスレーブ0x42、コントローラー受信、RS 3 Miniのローカル制御を担います。LEDは独立状態タスクが所有し、LCDからRGBコマンドを送りません。前面ボタンは状態/累積回数を報告し、色変更は行いません。

## ビルド

IDF環境を有効にした端末で実行します。

```sh
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash monitor
```

COM6は実ポートに変更します。115200はローカル確認済み、monitor終了はCtrl+]です。defaultsはClassic/Bluedroid/HID Host、BTDM/BLE/GATTC、v2 I²Cスレーブ/IRAM安全ISRを有効化しSPPを無効化します。既存BR/EDR-only sdkconfigは手動再生成/設定が必要で、defaultsは上書きしません。BLE HID/ジンバルの接続容量を維持してください。[独立ビルド手順](../docs/ja/development/build-and-flash.md)を参照してください。ESP32-S3/C3用ではなく、Arduinoは不要です。

## 基板リソース

| リソース | GPIO | 注記 |
| --- | ---: | --- |
|5×5 WS2812|27|25灯、GRB、IDF RMT|
|前面ボタン|39|Lowで押下、外部プルアップ|
|Grove SDA/SCL|26/32|黄/白|

通常1/2/3行はDS/BLE/ジンバル電池を左から最大5灯で示し、20%以下は赤点滅、不明/切断は消灯します。5行目は接続、起動/故障パターンは優先表示です。電池解析と実LED方向は別々に検証します。

## DualShock 4

1. USBを抜きSHARE+PSをライトバー高速点滅まで長押しします。
2. ATOMがWireless Controllerを探索/認証し、有効入力を待ちます。 `DualShock 4 connected; input ready` が準備完了で、HID open受理だけではありません。
3. 成功対象はNVS、bondはスタックに保存します。PSで保存コントローラーを起動し、自動再接続/再試行します。同時にペアリングする対象は1台だけにします。

Sony VID/PID054C:05C4と054C:09CCをSDPで検証します。同名第三者機器は自動対応しません。振動、ライトバー出力、タッチ座標は未実装です。探索は約10秒、open期限は約20秒です。

スティック-128..127、トリガー0..255、電池0..10/不明255です。切断で入力を消します。ds4_hostはDEBUG、通常タグはINFOです。初期レポート抜粋/10秒統計は診断であって合格証拠ではなく、実識別情報はローカル保持します。

bit0..17はShare,L3,R3,Options,上,右,下,左,L2,R2,L1,R1,△,○,×,□,PS,タッチパッド押下です。LCDにはイベントキャッシュを渡し、ローカルL3を除去します。Optionsは1押下で1切り替え、左スティックはLCDへ送りません。

カメラ操作はL1 Wide/R1 Tele、△Mode、□Focus、RT半/全S1/S2、LT全押し録画です。レンズはユーザー申告POWER_ZOOMで自動検出ではありません。[操作ガイド](../docs/ja/user-guide/controller.md)で実装と実機確認を区別します。

## RS 3 Mini

公式Roninでアクティベーション、バランス調整、軸解除後にアプリを切断します。未保存時は名称一致の唯一Miniを選び、制御/通知を確認してready後に保存します。複数候補は選びません。保存済みなら元のアドレスのみ再接続し、 `gimbal pair` でジンバル対象を交換します。DS4ペアリングは変えません。

実Classic DS4左スティックがPan/Tilt、L3が本体原点復帰です。接続後は中央/L3解放が必要です。LCD切断/入力源選択はローカル入力に影響せず、UART SIMは実移動を禁止します。

```text
gimbal status
gimbal speed pan 120
gimbal speed tilt 240
gimbal speed 120
gimbal invert 1
gimbal calibrate
gimbal stop
gimbal off
gimbal on
gimbal pair
```

20..400は角速度ではなくプロトコル幅です。共通speedは両軸、名前付きは個別。初期120/120、ユーザー確認済み120/240を保持します。校正は新鮮な中央実入力、偏移32以内が条件です。on/off,pair,調整/invert/校正は保存し、stopは保存しません。入隊は実行確認ではありません。

基本スティック/L3と実電源再投入復帰はユーザー確認済みです。任意原点、ソフトリミット、本体設定は未実装です。正確な停止/中止/切断時間、30分同時動作は未検証です。[通信と制約](../docs/ja/design/rs3-mini-protocol.md)、[現在の状態](../docs/ja/development/current-status.md)を参照してください。

## LCD I²Cリンク

LCD SDA8/SCL9/GND → ATOM26/32/GNDです。USB個別給電ではGrove5Vを接続せず、3.3Vプルアップを使います。0x42はLCD拡張0x24を避けます。

v2はv1と非互換で、更新は両基板に必要です。HELLOはversion/features/boot_id確認、offline探索1秒、online POLL50ms/書き込み後15ms待ちです。失敗はseq/ACKを保持して再試行、連続3回でoffline、version不一致は5秒ごとに再試行します。

| コマンド | 要求 | 成功応答 | 内容 |
| --- | --- | --- | --- |
|HELLO0x01|9バイト、param0x0202+入力modeバイト|19バイト|boot_id/version/features/capacity/local_mask|
|POLL0x10|9バイト、param=ack_id|35バイト|機器状態/故障/電池/ボタン/入力/イベント|

CRC-8/SMBUS `123456789`→0xF4、エラーはpayload0/全7バイトです。共通atom_protocol/atom_clientがframingを行います。スレーブはゴミを再同期、20msで半端フレームを破棄します。core0応答アダプターはIDF5.5.1内部に依存してsoftware/FIFOを置換するため、SDK更新時に再確認します。

128件RAMイベントはローカルL3除去/重複排除します。overflowはgapを立て、ACKで対応故障を解除し、後のoverflowを消しません。再接続/再起動で古いイベントを捨て、再送しません。ボタン回数は65535後に回ります。ジンバル0/1/2/3は無効/切断、探索、接続中、制御readyです。fault bit3は別です。[全offset](../docs/ja/design/i2c-protocol-design.md)を参照してください。

## BLE HIDと検証

ble_clientsが共有GAP/GATTC/探索を所有しClassicは独立します。appearance/対応名称で候補を絞り、接続後にHIDを検証し、唯一候補だけ選びます。No-IO bonding、電池0x180F/0x2A19は約10秒更新、不正/切断は255です。

Ultimate 2は観測113バイト記述子と唯一notifyが条件です。33バイト報告を共有入力発行/I²Cへ正規化し、DS4優先、BLE1秒無入力で解放します。 `ble map` はキャッシュ記述子を表示します。過去電池88%は実測済みですが、実ボタン/トリガー方向、カメラ操作、再起動復帰/安定性は未検証です。

リポジトリのcmake/ctestホスト試験を使います。[日付付き記録](../docs/ja/records/README.md)は過去のI²C再試行/シミュレーション結果を保持し、現在の30分合格ではありません。UARTは1プロセスだけが所有します。captures/builds/backups/sdkconfig/memoryは管理対象外です。
