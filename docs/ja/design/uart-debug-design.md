# UARTコンソール設計

[English](../../en/design/uart-debug-design.md) · [简体中文](../../design/uart-debug-design.md) · **日本語**

LCD Consoleは独立router/UART gatewayを所有。common debug_console/debug_line/debug_args/async_tokenが読行・parse・出力・照合、encoderは公開messageと純値だけ。maintenanceはConsole非依存。ATOMは別firmwareで本端ownerへrouteします。

UART0 115200、RX512、内部4096byte priority2、read20ms。255byte、CR/LF/CRLF一回、backspace一byte削除。不正control/非ASCII/過長は終端まで破棄してprefix実行なし。純C quotes/escapeでesp_console/linenoiseなし。#uint32は同期、非0tokenは非同期です。

Coreはinbox8control/1bulk登録後gateway、起動後route freeze。reader退出は自身endpointだけでself join/business stopなし。外quiesceはadmission/cancel/join/driver削除、失敗はcleanup保持し重複reader禁止。status欠落はerror、`s` は受付、Coreは物理排出です。

本番help/version/status/log、j/s/S/p、readonly wifi/ui、I²Cを保持。factory/u/maintとwriterは廃止。Debug SIM/player/bench/faultをgateし、ReleaseはIDF空SIM登録だけで実装/symbolなし。

SIMは同Input API、物理poll継続、切替release先。player10ms絶対期限、4job/8completion、cancelでqueue/held消去。Camera実操作は警告、gimbal SIM禁止。bench canvasはUIだけ。[操作](../user-guide/serial.md)、[ログ](../development/serial-log.md)、[message](module-message-contracts.md)、[テスト](../development/testing.md)を参照。Hostは実UART/SMPを証明しません。
