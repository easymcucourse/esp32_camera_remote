# 現在の実装と検証状況

[English](../../en/development/current-status.md) · [简体中文](../../development/current-status.md) · **日本語**

2026-10-10のソースと検証を基準とします。要件には未完了の目標を残し、日付付き記録の「現在」は記録当時を意味します。

## 実装

| 分野 | 現在のソース | 検証範囲 |
| --- | --- | --- |
| LCD構造 | Core構成ルート、typed Consoleルーター、Wi-Fi/Input/Camera/UIの個別owner、UI→display_surface→board_7b、廃止APIはtests/support/legacyのみ | 全停止/エラー経路、長期動作の実機検証は未完了 |
| カメラ | ZV-E10、AP DHCP探索/カメラ上のペアリング、PTP/IP二経路、0x9209属性、0xFFFFC002 JPEG、目標読み戻し/安全解放 | 過去の一部操作実測があり、全レンズ/列挙値/カメラは保証しません |
| 入力 | ATOM/I²C経由のClassic DS4と限定Ultimate 2 BLEレポート、現在のレンズ申告POWER_ZOOM | 自動レンズ識別なし、非電動ズームMF代替は無効、BLE実機マッピング/カメラ操作未検証 |
| UI/保守 | LIVE/SETTINGS/MORE、情報レベル、録画表示、起動HTTPから専有Web保守、Wi-Fi/設定/初期化/OTA後に再起動 | 通常UART/コントローラーはWi-Fi編集/初期化不可、WebにPIN/ログインなし、携帯端末/故障検証未完了 |
| ATOM | Classic+BLE、共有BLE探索、独立Matrix、電池、I²C v2、新鮮な実DS4入力 | BLE接続は実入力の証明ではなく、Matrix/LCD表示は別途実機検証 |
| RS 3 Mini | 初回唯一候補/保存対象、通知、左スティック、L3本体原点復帰、停止ゲート、Pan/Tilt個別NVS幅 | スティック/L3、120/240調整、電源再投入をユーザー確認済み、任意原点/リミット/本体設定メニュー未実装 |

## ソフトウェア検証

コード11266feはorigin/mainへpush済みです。CTest267/267が成功しました（CシナリオとPythonテスト群であり、267個の単独assertではありません）。新規cleanup20261010タグのLCD/ATOM Debug/Releaseが成功し、LCD依存グラフ/直接symbol owner、Releaseシミュレーター除外も合格しました。LCDイメージは5MiB以下です。文書整理で実機書き込みは行っておらず、新ビルドを稼働ファームウェアと混同しません。

## 実機と未完了事項

本機FFF4はNotify専用でCCCDから初期化します。endpoint4へのheartbeat後、スティック/L3の動作が確認されました。Pan120/Tilt240はRTSソフトリセット後も保持され、初期値は120/120です。幅は角速度ではありません。ジンバルの実電源再投入ではDS4入力を維持して自動復帰し、ユーザーは正常と回答しました。ATOMの実電源コールドスタートは未検証です。

アクティベーション前は04/66に三軸TLVがありましたが、その後の観測19バイト包にはなく、strict pose_rawはinvalidのままです。単位未校正のrawを閉ループ/リミットに使いません。離した後100ms以内の正確な停止、原点復帰の中止、異常切断、30分同時接続は未検証です。約14分の正常部分記録は30分合格ではありません。LCDのジンバル故障表示は実装/ビルド/ホスト検証済みですが、未書き込み/未視覚検証です。

ライブビューの測定段階設定を維持します：Wi-Fi/lwIPはPSRAM優先、内部予約32768、static TX6、cache32、mainスタック24576。Default実験クロックとStable80MHzは独立です。2026-10-08長時間記録はNO_MEMなしでしたが平均約4.76FPSで性能基準未達です。最適化タスクは停止済みで、今回のビルドは性能合格を示しません。

## 証拠

- [RS 3 Miniプロトコル](../design/rs3-mini-protocol.md)、[実測](../../records/rs3-mini-test-20261010.md)、[監査](../../records/rs3-mini-acceptance-20261010.md)。
- [ライブビュー記録](../../records/liveview-execution-20261008.md)、[性能監査](../../records/liveview-acceptance-checklist-20261008.md)。
- [モジュール状態](module-split-status.md)、[検証一覧](module-split-checklist.md)。

生UART、キャプチャ、画像、実識別情報は管理対象外です。公開記録には匿名化した事実と統計だけを含めます。
