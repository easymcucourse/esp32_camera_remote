# UARTデバッグ要件

[English](../../en/request/uart-debug-request.md) · [简体中文](../../request/uart-debug-request.md) · **日本語**

両ボードに115200の改行console、help/version/status/log、厳密line/argument、uint32 request ID、非同期token、script再生を提供します。DebugはRAMのみでSIM表示、Releaseから除外します。

不正・過長・非ASCII行はprefixを実行せず破棄し、引用符・escapeを解析します。完整行を直列出力、受付とDONE/FAILを区別し、独立player中もreaderを応答可能にします。read/driver失敗はconsole ownerだけを退場させCamera/Inputを停止しません。

LCDは型付きCamera/UI/Input/Wi-Fi/System要求を使い、private業務APIを呼びません。`status` は必要snapshot欠落でerror。UART `s` は停止受付、Core停止は物理排出完了待ちです。永続設定/factory/maintenanceはWebで、旧 `u/factory/maint` は動作しません。

Debugはpad action、source/connect/battery/gap、I²C fail/CRC/timeout/delay/raw、Matrix校正・障害、表示障害・共通renderer benchmarkを注入します。SIMカメラ入力は実カメラを動かし得るため警告します。SIM中実gimbalは遮断します。有界queue、絶対player期限、cancel/release、新しいscript期待で旧actionを防ぎます。

CRLF/backspace/error、ID overflow、新ACK/token、二台script、遅延/満杯/取消、held SIM解除、正規化実機/SIM同等、Release除外、実UART/I²Cを検証します。Hostだけでserial/SMP時限を証明しません。[操作](../user-guide/serial.md)、[設計](../design/uart-debug-design.md)、[ログ](../development/serial-log.md)を参照してください。
