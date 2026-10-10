# ジンバル制御設計

[English](../../en/design/gimbal-design.md) · [简体中文](../../design/gimbal-design.md) · **日本語**

本番は `gimbal_link`、純 `gimbal_control`、`gimbal_proto_rs3`、有界 `gimbal_tx`、共有 `ble_clients` です。旧generic ops/configと50ms task案は将来・履歴で、現構造ではありません。link20ms workerがevent/GATT/NVS、dispatcherが唯一GAP/GATTCと直列BLE scanを所有します。

実Classic DS4だけを同esp_timer、接続epoch、age<200msで読みます。SIM中は遮断。再接続/epoch変更はneutral/disarmし、新鮮な中央stickとL3解除後rearmします。I²C/LCDはmotion判定に関与しません。

offset差引き、±127 clamp、deadzone13、残り正規化二乗と各spanです。初期120/120、20–400、Tilt反転1。protocol偏差であり角度/秒ではありません。実機120/240を保存。手動200msで最新だけ、neutralは間隔制限なし。

L3押下で固有center、保持中stick誤差無視。解除後pushまたは5sでneutral後manual。移動中はneutralを先送し、TX空きまで押下edge保持。実取消/停止時限は未検証です。

TXは普通一個＋neutral予約一個、旧目標backlogなし。500ms完了なしでclose/fault。連続失敗・notify初期化・有効RX・無通信期限は[Mini](rs3-mini-protocol.md)。ready3とfeature/faultをI²Cへ公開し、LCDからmotion設定しません。

NVS v1 cfgはtarget/enable/Pan/反転/offset、`tilt_span` は別u16で欠落時旧span継承。校正は新鮮な±32以内現在値で、旧1秒平均menuではありません。任意零点、角度閉loop、soft limit、本体menuは未実装。固有centerはジンバル自身の基準です。[要件](../request/gimbal-request.md)と[検証](../development/current-status.md)を参照してください。
