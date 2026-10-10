# ESP32 Camera Remote

[English](README.md) · [简体中文](README.zh-CN.md) · **日本語**

Waveshare ESP32-S3-Touch-LCD-7BでSony ZV-E10のライブビューを表示し、Wi-Fi/PTP/IPでカメラを操作するプロジェクトです。M5Stack ATOM Matrixがコントローラー入力を受信してI²CでLCDに送り、DJI RS 3 MiniはATOMからBLEで直接操作します。

## 現在の状態

2026-10-10のソースを基準にしています。[実装と検証状況](docs/ja/development/current-status.md)では、ソース、ビルド、書き込み済みファームウェア、実機結果を区別しています。

- LCD：1024×600 RGB565、ピクセルクロック18MHz。タッチは初期化しません。LIVE/SETTINGS/MORE、設定値の読み戻し確認、録画表示、情報表示レベルを実装しています。
- カメラ：動的探索、カメラ上での通常ペアリング確認、PTP/IPのコマンド/イベント接続、JPEGライブビュー、属性/操作状態機械。現在のレンズはユーザー申告の電動ズームとして扱い、自動識別は行いません。
- ATOM：Classic+BLE、DS4と特定のHID記述子に限定したUltimate 2の解析、電池/Matrix状態、I²C v2。Ultimate 2の全実機マッピングとカメラ操作は未検証です。
- RS 3 Mini：実Classic DS4の左スティック、L3による本体の原点復帰、初回対象選択/保存済み対象への再接続、Pan/Tilt個別調整、停止/再接続ゲート。スティック/L3とジンバルの電源再投入後の復帰はユーザー確認済みです。任意位置の原点記録、ソフトリミット、本体ボタンの設定メニューは未実装です。
- ホスト267テストとLCD/ATOMのDebug/Release計4構成が成功しました。正確な停止時間、30分の同時接続、LCDのジンバル故障表示は未検証です。ビルド成功は実機合格ではありません。

## ハードウェア

| 項目 | 構成 |
| --- | --- |
| 表示基板 | ESP32-S3-Touch-LCD-7B、Flash16MB、Octal PSRAM8MB |
| 拡張 | ATOM Matrix、従来のESP32。S3/C3用のファームウェアではありません |
| I²C | LCD SDA8/SCL9 → ATOM SDA26/SCL32、GND共通 |
| バス | 100kHz、3.3Vプルアップ、スレーブアドレス0x42 |
| 対象 | Sony ZV-E10 / DJI RS 3 Mini |
| SDK | ESP-IDF5.5.1、manifestとlockファイルで依存関係を固定 |

USBで個別給電する場合はSDA/SCL/GNDのみ接続し、Grove5Vは接続しません。[ハードウェア詳細](docs/ja/design/hardware-design.md)を参照してください。

## ビルドと更新

ESP-IDF5.5.1を有効にした端末で、リポジトリのルートから実行します。

```sh
python tools/ci_build.py lcd debug --build-tag local
python tools/ci_build.py atom debug --build-tag local
python tools/ci_build.py lcd release --build-tag local
python tools/ci_build.py atom release --build-tag local
python tools/ci_build.py lcd debug --profile stable --build-tag local
```

生成物はbuild/ci-<board>-<flavour>[-stable]-localにあります。Releaseはシミュレーター実装を除外します。ATOMにはBTDM/BLE/GATTCが必要です。既存sdkconfigをdefaultsで自動上書きすることはありません。Defaultの実験的クロックとStable80MHzは独立した構成であり、長期安定性を保証しません。

実際のポート、パーティション、生成物を確認してから [ビルド/書き込み手順](docs/ja/development/build-and-flash.md)に従ってください。COM8/COM6は過去のローカル例です。ATOMは115200で書き込み確認済みです。運用中のLCDはWeb OTAを優先します。初回用の全体書き込みはOTAメタデータを初期化する場合があり、アプリだけの更新とは異なります。I²C v1→v2では両基板を更新します。

## 操作

1. カメラをLCDに表示されたアクセスポイントに接続し、PCリモート/Wi-Fiアクセスポイント接続を有効にします。初回はカメラ上でペアリングを許可します。IPはDHCPから探索し、過去のキャプチャIPを固定指定しません。
2. 初回DS4はSHARE+PSを長押ししてライトバーを高速点滅させます。保存済みの場合はPSで起動します。有効な入力レポートが準備完了の根拠です。MatrixのLCD接続灯だけではコントローラー接続を判断できません。
3. RS 3 Miniは公式アプリでアクティベーションし、バランス調整/軸ロック解除後、Ronin Appを切断します。未保存時は唯一のMiniを自動選択します。保存済みの場合は元の機体だけに再接続し、交換時はATOM UARTで `gimbal pair` を実行します。接続後はスティックを中央に戻し、L3を離します。

| 入力 | 操作 |
| --- | --- |
| Options/Start | LIVE ↔ SETTINGS |
| L1/R1 | Wide/Tele。両方同時押しで停止し、両方を離すまで解除しません |
| □/X、△/Y | フォーカスモード、露出Mode。1回の押下で1段階 |
| R2/RT | 半押しS1フォーカス、全押しS2撮影 |
| L2/LT | 半押しは無動作、全押しで録画目標を切り替え |
| 方向キー | SETTINGSの移動/変更。右でEV増加、左で減少 |
| ×/A、○/B | 決定/戻る。MOREは追加属性、Wi-Fiは情報表示のみ |
| タッチパッド押下 | 現在は無動作。表示レベルは起動Webで保存 |
| 左スティック、L3 | ATOM内でPan/Tilt、ジンバル本体の原点復帰 |

接続後はトリガーを一度完全に離してください。録画状態が不明な場合は目標を推測しません。カメラの入力源選択とジンバルの実Classic DS4入力は独立しています。UARTシミュレーションは実ジンバルを動かしません。[コントローラーガイド](docs/ja/user-guide/controller.md)と [カメラガイド](docs/ja/user-guide/camera.md)を参照してください。

```text
gimbal status
gimbal speed pan 120
gimbal speed tilt 240
gimbal off
gimbal on
```

ATOM UART用コマンドです。20..400はプロトコルの偏移幅であり、度/秒ではありません。 `gimbal speed 120` は両軸を設定します。初期値は120/120、本機では120/240を確認済みです。設定はNVSに保存します。

LCD UART：`j` 開始/再開、`s` 停止、`S` SETTINGS切り替え、`p` 停止中のペアリング/再接続診断。[シリアルコマンド](docs/ja/user-guide/serial.md)に詳細があります。

## メンテナンスとテスト

HTTPメンテナンス入口は起動接続画面でのみ開きます。表示IPにアクセスすると通常のカメラ/コントローラーサービスを停止して専有MAINTENANCEに移行します。WebでWi-Fi、表示設定、初期化、OTAを操作します。PIN/ログインはなく、APクライアントはメンテナンス可能です。保存/終了/OTA成功後はLCDを再起動し、その場でライブビューには戻りません。通常のコントローラー/UARTではWi-Fi編集やfactory resetを行いません。[クイックスタート](docs/ja/user-guide/quick-start.md)を参照してください。

```sh
cmake -S tests/host -B build/host
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
python tools/check_doc_links.py
python tools/check_module_boundaries.py
```

ホストWebテストにはcJSONが必要です（Linuxはlibcjson-dev、WindowsはSDKソースパス指定）。テストはソフトウェア境界を検証し、Flash、無線、画素、カメラ操作を証明しません。生ログ、キャプチャ、ローカルagent memoryはGitの管理対象外です。

## 文書とソース

[文書一覧](docs/README.md)は英語、中国語、日本語の順です。現在の利用/開発/要件/設計と、日付付き [過去の証拠](docs/records/README.md)を分けています。

mainはLCD入口、componentsはCore/ルーター/ネットワーク/入力/カメラ/UI/基板モジュール、commonは共有純プロトコル、m5_atom_matrixは独立拡張、tests/hostは回帰、toolsはビルド/シリアル/解析です。フォントのライセンスはcomponents/app_ui/fonts、公開プロトコル参照のライセンスはthird_party/rs3-protocolにあります。

非公式プロジェクトです。メーカー名は対応対象を示し、承認を示しません。検証範囲と [通信証拠の公開規則](docs/README.md#通信记录的公开范围)を確認してください。
