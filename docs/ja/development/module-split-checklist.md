# モジュール検証一覧

[English](../../en/development/module-split-checklist.md) · [简体中文](../../development/module-split-checklist.md) · **日本語**

[中国語チェックリスト](../../development/module-split-checklist.md)の **133件の要件・検証項目** を分野別に説明します。個別IDと証拠リンクは元一覧に保持します。source/Host/build済みでも実機合格は別で、最新件数は[現在の状態](current-status.md)に従います。

| 分野 | 確認する契約 |
| --- | --- |
| 境界 | MainはCore開始のみ。Consoleは機能componentへ依存せず、UI/surface/board、Wi-Fi/backend、Camera/PTP/Sonyのheader所有範囲を守る。 |
| router/message | 照合、絶対期限、世代、遅延reply、queue満杯、control優先、lease回収。 |
| Input | 実機/SIMの正規化action比較、古いepoch拒否、gap/offline/切替/停止の完全release、ReleaseのSIM除外。 |
| frame/display | Camera readonly leaseを全出口で回収、UIはback canvasのみ取得、scan中front不変、bench共通renderer。 |
| Wi-Fi/PTP | ネットワーク世代で旧channel無効、唯一PTP session/transaction、wifi_esp32外のsocket禁止。 |
| maintenance | 起動HTTP claimだけ、不可逆排他mode、固定画面、認証なし、AP限定HTTP、通常設定writerなし。 |
| storage/factory/OTA | Coreの限定write owner、reset範囲と部分失敗、image検証・restart予約・rollback保持。 |
| lifecycle | 冷起動、各部分初期化失敗、有界排出、未完message、実再起動。 |

実source fixtureは多くの局所動作を確認します。Camera→router→UI統合fixtureはlease/controlを強化しますが、協調schedulerとfake hardwareです。symbol/dependency検査は直接経路だけで、過去のチェック済みは現在の実時限の証明ではありません。

SMP下RGB、入力から実カメラrelease、blocked HTTP停止、AP/他netif隔離、Flash/cache-off、初回起動安全、実再接続、30分runが残ります。source、Host、build、flash、目視、時限、安定性を分けます。[所有権](../design/module-resource-ownership.md)、[message](../design/module-message-contracts.md)、[依存graph](../design/module-dependency-graph.md)も参照してください。
