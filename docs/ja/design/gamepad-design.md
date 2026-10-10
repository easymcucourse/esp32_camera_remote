# 入力の所有権と制御

[English](../../en/design/gamepad-design.md) · [简体中文](../../design/gamepad-design.md) · **日本語**

実ATOMとDebug SIMは各protocol/providerを所有し、16entry固定ringへ正規化reportをコピーします。Input ownerだけがsource/epoch/IDを検証し `gamepad_input` を実行、Camera/UIへ型付き要求します。

API v1は非0opaque handle、非0source_epoch/report_id、button/axis/trigger/battery/link/SIM/gap/eventの値コピーです。重複ID・旧epoch拒否、ID後退は新epochまで隔離。ring満杯は該当source backlog削除と優先disconnect。再登録は旧handle無効。切替/gap/再接続はheld baselineだけ、新press前に完全releaseを確認・retryします。

Inputは内部4096byte/priority4、50ms。通常100ms期限、menuはUI JPEG待ちで500ms。menu replyは意味的route/property、Camera調整は別要求です。source受付は物理release完了ではありません。Coreはprovider/Camera前にInputを停止し、timeout活動ownerを強削除しません。

RT半77/51、全230/204、LT全230/204だけ。両trigger解除後arm。S1→S2押下、S2→S1解除。LTはRT focusへ影響しません。一回buttonはevent、snapshotはanalog/repeat/release。repeat400/150msで追送なし。両肩は停止lock、X取消。目標/読み戻しと安全世代で旧commandを防ぎます。

現レンズは申告POWER_ZOOM、L1Wide/R1Tele。非電動+MF代替は明示確認が必要です。focus点/拡大は計画。純kernelのtouchpad actionは `input_service` が破棄し、表示設定はWebのみです。

Ultimate2は113byte HID descriptor一致・一意notify33byte入力だけ対応します。不明layoutでCamera制御しません。DS4優先、BLEは1sでstale。gimbalは別の実Classic DS4だけを使い、aggregate/SIMを使いません。[操作](../user-guide/controller.md)、[menu](camera-menu-design.md)、[I²C](i2c-protocol-design.md)、[検証](../development/module-split-checklist.md)を参照してください。
