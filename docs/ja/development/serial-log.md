# シリアルログ

[English](../../en/development/serial-log.md) · [简体中文](../../development/serial-log.md) · **日本語**

`python -m pip install -r tools/requirements.txt` でpyserialを導入するかIDF Pythonを使います。`tools/serial_log.py` は115200でraw byteを保存し、無効UTF-8を置換して表示します。開く前にDTR/RTSを無効にし、`--reset` 指定時だけRTSで再起動します。記録終了は取景を停止しません。

```sh
python tools/serial_log.py --port COM8 --reset --seconds 30 --output build/boot.log
python tools/serial_log.py --port COM8 --command j --seconds 30 --output build/live.log
python tools/serial_log.py --port COM6 --seconds 60 --output build/atom.log
```

初期値は `--port COM8`、25秒、`build/serial-boot.log` です。既存出力を上書きします。`--command` は再起動なしでASCII一行送信、`--until` は文字列一致で終了します。実ポートに置き換え、他monitorを閉じます。二台の記録は別terminal、一ポート一ownerで行います。

`tools/test_camera_connection.py --port COM8 --connect-wait 120 --steady 30 --output captures/connection-test` は停止・開始、初フレーム、継続、保存識別による再接続を検査します。開始時にカメラtaskが動作している必要があります。再起動や登録消去はせず、終了後も取景を継続します。カメラ未準備やDHCP未取得は前提条件の失敗です。実再起動の永続性は別検証です。

`SESSION VERIFIED`、`LIVEVIEW RUNNING`、`LIVEVIEW frames=… fps=…`、`ATOM v2 online`、入力ready、overflowを確認します。旧AP/init/vendorログは現在存在しない場合があり、UART `status` の意味的snapshotを使います。`s` 応答は受付でありtask終了ではありません。現在heapだけでなく最低値と最大連続空きも保存します。

全画面とSETTINGSを分離して解析します。

```sh
python tools/analyze_liveview.py build/live.log --expect-settings 0 --output build/live-summary.json
```

SETTINGSは `--expect-settings 1`。混在・欠落metadataは統計を残して失敗終了します。FPSは有効windowで加重し、reset/断流間隔と最初の短区間を除外します。read/display/JPEG分位は約5秒ごとの最後の一フレームの標本であり、全フレームの分布ではありません。600秒windowには615秒以上保存します。自動で性能・安定性合格を宣言するツールではありません。版、環境、モード、目視確認を別記し、rawはignored `build/` / `captures/` へ保存します。[コマンド](../user-guide/serial.md)も参照してください。
