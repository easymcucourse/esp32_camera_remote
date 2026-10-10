# 改善の優先順位

[English](../../en/request/improvement-request.md) · [简体中文](../../request/improvement-request.md) · **日本語**

現在の優先順位です。[中国語優先順位](../../request/improvement-request.md)から旧一覧の履歴へ進めます。過去checkはsource/Hostで全面実機合格ではありません。次の変更前に[現在の状態](../development/current-status.md)を確認します。

| 優先 | 残る作業 |
| --- | --- |
| P0安全・安定 | 実冷起動、全owner排出、表示復旧、入力release、NVS失敗。活動ownerを強制削除しない。 |
| P1カメラ・network | 探索/再接続/許可全組合せ、実parameter/録画/撮影、raw意味、継続復旧。 |
| P1gimbal | ≤100ms停止・取消、ATOM冷起動・異常切断、正しい電量/角度、30分。任意零点/limitは信頼poseが前提。 |
| P2性能 | 段階PSRAM/TCP/FPS、全/SETTINGS分離、heap/stack、Stable、再起動再接続。前段合格なしに第三slotを追加しない。 |
| P2maintenance/UI | 実Web排他・AP隔離、save/reset/OTA/電源断、全表示・障害、実Ultimate対応。 |
| P3可搬性・文書 | 明示port/SDK、一般capture抽出、匿名fixture、英中日navigation。 |

構造化Sony descriptor、DHCP、取消TCP、目標合成、I²C v2/gap、owner/lease gate、Debug/Release CI、二slot OTA、Stable、heap解析、基本Mini制御は実装済みです。未実装扱いに戻しません。全面実機・性能合格は別です。

touch代替、設定可能mapping、focus点/拡大、広いBLE互換は将来範囲です。remote CI/branch protectionは確認していません。整理時に既存fixtureと過去captureを保持します。[テスト](../development/testing.md)と[検証](../development/module-split-checklist.md)を参照してください。
