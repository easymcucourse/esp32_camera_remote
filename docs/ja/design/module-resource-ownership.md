# リソース所有権

[English](../../en/design/module-resource-ownership.md) · [简体中文](../../design/module-resource-ownership.md) · **日本語**

安全停止はmodule名だけでなくlifetimeで決まります。[詳細task/queue台帳](../../design/module-resource-ownership.md)に日付付きpriority/stack/pathがあります。現main24576byteが旧32768を置き換えます。設定値であり実stack/SMP証明ではありません。

| resource | owner/lifetime |
| --- | --- |
| router |16waiter/32lease。endpoint queueは再起動まで保持。admission閉鎖・owner取消・最終ref待ち。 |
| Input |16コピーreport/handle/epoch、解除・停止・source再arm前release。 |
| Camera | 唯一backend/PTP、起動512KiB二slotを保持、lease/result両完了後再利用。 |
| UI | CPU1endpoint/renderer、起動decoder/work4096byte、back pixel借用。 |
| display | RGB565二画面/内部bounce40960、surface一writer・旧世代拒否、maintenance固定画面用保持。 |
| network | TCP二lane、job/control/cancel channel、I/O/lease完了後free。AP/config履歴維持。 |
| HTTP/OTA | 内部6144stack、PSRAM4096chunk、OTA end/abort。SDK join全体厳密上限なし。 |
| storage | wifi_ap mutex、Sony内部worker、Web/Core phase隔離。namespace横断原子なし。 |

Coreは通常受付を閉じrelease/completionを許可。UART/Input/provider/bench →Camera物理→endpoint→通常Wi-Fi config/bridge→prefs/UI→renderer→System/router。前段失敗は依存owner保持しrestart、強削除・通常復帰なし。1000/3000msは個別budgetで全体期限ではありません。

PTPはCamera producerの唯一Sony client。timeout後もchannel bufferはcancel/完了まで生存。surfaceはobject/lease/generation/address/size検証、正ownerだけ返却、active writer中recover拒否。lease callbackは最後holderのtaskで実行し得ます。

LCD NVS直接writeはsource gateで三primitive限定、backend read修復はあり。語彙gateはalias/pointer/Flash SMPを証明しません。cache-off/blocked HTTP/RGB/全排出時限は実測待ち。[message](module-message-contracts.md)、[graph](module-dependency-graph.md)、[検証](../development/module-split-checklist.md)を参照してください。
