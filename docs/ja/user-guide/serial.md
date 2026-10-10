# シリアルコマンド

[English](../../en/user-guide/serial.md) · [简体中文](../../user-guide/serial.md) · **日本語**

両コンソールは **115200 baud**、改行で実行、最大255バイトです。引用符とバックスラッシュによるエスケープを使用できます。共通コマンドは `help`、`version`、`status`、`log <tag|*> <none|error|warn|info|debug|verbose>` です。`#123` は同期応答の照合番号、非同期処理にはtokenもあります。受付応答は完了ではありません。

| LCDコマンド | 動作 |
| --- | --- |
| `j` / `s` | 開始・再開／停止要求と排出。`s` の応答は受付だけを示します。 |
| `S` | LIVE/SETTINGS切り替え。 |
| `p` | 停止中のペアリング・再接続診断。 |
| `wifi show` / `wifi show password` | 設定照会。後者だけがパスワードを明示表示します。 |
| `ui info` / `ui pad` | 起動時に読み込んだ設定を照会。 |
| `extra status` | MOREの値、書き込み可否、目標状態を照会。 |
| `i2c log on|off|changes`、`i2c stats`、`i2c stats reset` | 実バスの監視と統計。 |

設定保存、リセット、メンテナンス開始は **起動時のWeb** で行います。UART `u`、`factory`、`maint on/off/status/probe`、AP編集、設定書き込みは廃止しました。現在のInput serviceはタッチパッドの情報切り替えを無視します。Webで保存し、再起動後に読み込みます。

| ATOMコマンド | 動作 |
| --- | --- |
| `gimbal status` | 状態、保存対象の有無、速度、入力鮮度、送受信、障害を照会。 |
| `gimbal on` / `off` | 有効状態を保存。offは制御停止と切断。 |
| `gimbal pair` | ローカルのジンバル対象だけを解除し再探索。DS4登録は保持。 |
| `gimbal stop` | 中立・安全停止を要求。 |
| `gimbal calibrate` | 新鮮で中央付近の実DS4入力を校正。不適切な入力は拒否。 |
| `gimbal speed 20..400` | 両軸の値を変更。 |
| `gimbal speed pan 20..400` / `tilt 20..400` | 一軸だけを変更。 |
| `gimbal invert 0|1` | Tilt反転を保存。 |

Debugは `pad sim`、接続・切断、電量、tap/hold/release、stick/trigger、有界 `seq` を提供します。LCDには `atom sim`、online/offline/reboot/version、fail/crc/timeoutがあります。ATOMには `i2c drop/corrupt/delay`、9バイトの `i2c req`、LED校正・障害表示があります。`display fault` と `display bench` は対応Debug設定が必要です。Releaseからこれらを除外します。SIMは接続中の実カメラを操作できますが、実ジンバルは動かせません。

例：`python tools/uart_script.py --port lcd=COM8 --port atom=COM6 --script tools/uart_scripts/pair-i2c-monitor.uart --log build/i2c.log`。実際のポートに置き換えます。`@lcd/@atom` は対象、`wait`、`expect`、`expect-any`、`!` は待機・新しい出力・想定拒否を指定します。再起動や障害注入を含むスクリプトは実行前に内容を確認します。

一つのポートを同時に開けるのは一プロセスです。[ログ](../development/serial-log.md)と[UART設計](../design/uart-debug-design.md)を参照してください。`record known/recording/pending` は実状態と要求を区別します。extra状態は0待機、1確認待ち、2反映、3拒否、4期限切れ、5受付です。
