# 実装と検証の履歴

[English](../../en/development/implementation-status.md) · [简体中文](../../development/implementation-status.md) · **日本語**

最新基準は[現在の状態](current-status.md)です。本ページは履歴の読み方を説明します。[日付付き中国語台帳](../../development/implementation-status.md)には各回の詳細と原証拠リンクを保持しています。

| 分野 | 現在の実装 | 残る検証 |
| --- | --- | --- |
| カメラ | DHCP識別、PTP/Sony backend、property/target、S1/S2、録画・ズーム | モード・動作全組合せ、障害復旧、より高いFPS目標。 |
| Input/UI | 実機/SIM provider、型付きaction、menu、情報量、release | Ultimate 2実ボタン、全表示・遅延、冷起動。 |
| Wi-Fi/maintenance | owner/backend、起動限定認証なしWeb、設定/reset、二slot OTA | 新分割版browser、隔離、flash/電源断、停止時限。 |
| ATOM | DS4、共有BLE scan、I²C v2、Matrix、Mini制御 | 実表示、再起動・切断全組合せ、30分並行運転。 |
| RS 3 Mini | stick/L3、独立速度、ジンバル再起動復帰をユーザー確認 | ≤100ms停止、回中取消、任意零点・soft limit、実ATOM冷起動。 |

10月1日の接続、3日のOTA/録画枠、4日のEV、5–6日のモジュール移行、7–8日の取景は、それぞれ当時のbinと範囲の証拠です。旧PIN、AP編集menu、UART `u`、UNKNOWNレンズ、ジンバル除外は現在の規則に置き換わっています。

コードcommit `11266fe` はHost267件と新規四構成を検証してpushしました。新binは書いていません。ATOMの物理結果は以前書いたMini制御版に属し、ATOM書き込みからLCD画面やカメラ効果を推定しません。

合格にはsource、Host、build、flash、物理効果、性能、安定性を個別に確認します。token/ACKはその段階だけを証明し、中断runは部分結果です。[モジュール検証](module-split-checklist.md)と[履歴](../records/README.md)を参照してください。
