# ジンバル要件

[English](../../en/request/gimbal-request.md) · [简体中文](../../request/gimbal-request.md) · **日本語**

ATOMは実DS4の左X/YとL3から **RS 3 Mini** を直接制御します。LCDは接続・障害状態だけを受け取ります。画像追跡、経路、timelapseは範囲外です。公式有効化とClassic+BLE同時運転が前提です。

| 要件 | 合格条件・現境界 |
| --- | --- |
| R1接続 | 保存Miniへ自動再接続。未登録時は一致候補一台だけを選び、複数を無作為に選ばない。制御・通知初期化後にready。 |
| R2手動 | X→Pan、Y→Tilt、約10%deadzone、二次応答、最新目標約5Hz。松stickから100ms以内停止は未実測。 |
| R3回中 | L3で平滑回中。保持中の誤stickを無視、解除後pushで取消。固有回中は動作、任意記録零点と実取消は未完。 |
| R4安全 | DS4切断、report age≥200ms、無効化、再接続で停止後arm。LCD/I²C切断は独立DS4制御へ影響させない。連続write失敗を障害表示。 |
| R5本体設定 | target、enable、反転、中央offset、軸速度を保存。本体button menu、任意零点、soft limitは目標。信頼できる角度なしに保護を宣言しない。 |

初期spanは120/120、範囲20–400のprotocol単位であり度/秒ではありません。実機はTilt速度不足の申告後Pan120/Tilt240を保持します。pairingはローカルapp登録です。`gimbal pair` はジンバルだけを交換し、DS4やSMP bondを消すと説明しません。

初回一意選択、冷起動再接続、四方向・deadzone・速度、≤100ms停止、L3/取消/timeout、入力stale/offline、I²C抜線、電源再投入、limit・NVS、Matrix/LCD障害、**30分** の無暴走・無切断を検証します。ユーザーはstick/L3とジンバル再起動正常を確認しました。約14分並行観測は部分結果です。実ATOM冷起動、全障害・時限は未完です。

[protocol](../design/rs3-mini-protocol.md)、[設計](../design/gimbal-design.md)、[検証](../development/current-status.md)を参照します。要件は実装完了宣言ではありません。
