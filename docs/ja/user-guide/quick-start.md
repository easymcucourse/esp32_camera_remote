# クイックスタート

[English](../../en/user-guide/quick-start.md) · [简体中文](../../user-guide/quick-start.md) · **日本語**

Waveshare LCD-7B、ATOM Matrix、Sony ZV-E10、DS4を使い、基板はUSB個別給電します。LCDはESP32-S3、ATOMは従来ESP32です。[ビルドと書き込み](../development/build-and-flash.md)に構成/更新詳細があります。

## LCD初回導入

ESP-IDF5.5.1有効端末でルートから実行し、COM8を実ポートに置き換えます。

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

monitor終了はCtrl+]です。NVSを消さなければペアリング識別は残ります。全体USB flashはパーティション/初期OTAデータも書き、運用機の起動slotを変える場合があります。Web OTA、または活動slot確認後のアプリ更新を使います。全体消去後は識別がなくなり再ペアリングが必要です。

## ATOM導入

```sh
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash
```

COM8/COM6は過去の例で自動検出ではありません。実ポートを確認します。ATOM115200はローカル確認済み、I²C v1→v2は両基板更新です。既存ATOM sdkconfigは実際にBTDM/BLE/GATTCを有効にする必要があり、defaultsだけでは変わりません。

## 配線とカメラ

LCD SDA8/SCL9/GND → ATOM SDA26/SCL32/GND。USB個別給電でGrove5Vを接続しません。100kHz、3.3Vプルアップです。

1. 起動画面または `wifi show` のAP設定を確認します。保存値がcommon/network_config.hの初期値より優先されます。
2. カメラをAPに接続し、PCリモート/Wi-Fiアクセスポイント接続を選びます。
3. カメラの確認画面でESP32-Camera-Remoteを許可します。
4. 最初のフレームが正常にデコードされたら連続表示します。復旧可能な失敗は状態画面/再試行へ戻ります。

[探索/カメラ交換](camera.md)、[コントローラー/ジンバル操作](controller.md)、[問題解決](troubleshooting.md)を参照してください。

## Webメンテナンス

Wi-Fi/表示設定/初期化/OTAはLCDを再起動し、起動接続画面の間にAPへ接続して表示IP（通常 http://192.168.4.1/）へアクセスします。通常モードはport80を閉じるため、移行済みなら再起動します。

最初の要求で専有保守を取得しホームへredirectします。通常サービスを排空しLCDはMAINTENANCEを表示します。カメラ/コントローラー/UARTは通常操作を続けられず、その場でライブビューに戻りません。PIN/ログインはなく、APクライアントは保守可能です。保存/終了/再起動/OTA成功後はLCDを再起動し、AP変更時は再接続します。

AP初期化はカメラペアリングを残します。全初期化はLCDのカメラ/表示設定も消し、ATOMの绑定は残します。失敗時は一部変更の可能性があり、ページエラー/起動ログを確認します。アップロードはLCDアプリbinだけで、ATOM、bootloader、mergedを選びません。二重OTA分区の初回導入はUSBで分区表を書きます。現在の携帯/ブラウザ/画面/OTA故障検証は未完了です。[状態](../development/current-status.md)を確認してください。
