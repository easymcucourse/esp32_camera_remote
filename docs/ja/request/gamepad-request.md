# コントローラー要件

[English](../../en/request/gamepad-request.md) · [简体中文](../../request/gamepad-request.md) · **日本語**

標準開発対象はClassic Bluetooth DualShock 4です。Ultimate 2 BLEには限定descriptor/report parserがあり、実ボタン対応は未検証です。起動WebでDS/Xbox互換入力を保存します。任意のBLEコントローラーが自動対応するわけではありません。

LIVE/SETTINGS共通の要件と現在の操作です。

| 入力 | 動作 |
| --- | --- |
| Options/Start | LIVE/SETTINGS切替。 |
| Square/X、Triangle/Y | Focus/露出modeを次へ。一押し一回、読み戻し確認。Xは肩ボタン制御取消。 |
| L1/R1 | 申告済み電動ズームでWide/Tele。非電動ズーム確認済みかつMFだけNear(+1)/Far(−1)。 |
| RT | S1半押し、S2全押し。S2を先にrelease。 |
| LT | 半押しは無動作。全押しedgeで既知の録画目標を反転。不明・確認待ちでは推測しない。 |
| D-pad、A/B | menu移動・step、確定・戻る。400ms後150ms間隔、遅延分を追送しない。 |
| 左stick/L3 | ATOM内Pan/TiltとMini固有回中。 |
| タッチパッドクリック | kernelは識別するが現serviceは無視。表示設定はWebのみ。 |

RTは77で半押し、230で全押し、51未満で半押し解除、204未満で全押し解除です。LTは230/204の全押しだけです。接続・source・session変更後は両trigger解除を見てからarmします。両肩同時押しは停止し、両方離すまでlock。ズーム不可からレンズ種類を推定しません。

gap、旧epoch/report、offline、source変更、停止ではCamera actionをreleaseし、旧命令を拒否します。再接続時のheld入力は新edgeを生成しません。録画・設定は実読み戻しで確認します。Select/右stick/R3のfocus点、拡大は未実装です。touchpadの現在動作は旧切替要件を置き換えます。

全ボタン・閾値、同時/held/gap、再接続、実Camera効果、Ultimate対応、P95遅延、30分を検証します。Miniは別[要件](gimbal-request.md)です。[操作](../user-guide/controller.md)、[設計](../design/gamepad-design.md)、[現状](../development/current-status.md)を参照してください。
