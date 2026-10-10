# UART debug requirements

**English** · [简体中文](../../request/uart-debug-request.md) · [日本語](../../ja/request/uart-debug-request.md)

Provide a reproducible newline console on both boards at 115200: common help/version/status/log commands, strict line and argument parsing, uint32 request IDs, asynchronous tokens and script playback. Debug features are RAM-only and visibly marked SIM; Release excludes them.

Required behaviors: reject malformed/overlong/non-ASCII input without executing a prefix; parse quotes/escapes; serialize complete output lines; acknowledge admission separately from DONE/FAIL; keep readers responsive while independent players run. A read/driver failure retires only its console owner, not camera/input services.

LCD commands use typed Camera/UI/Input/Wi-Fi/System requests; they do not call private business APIs. `status` reports an error if any requested snapshot is missing. UART `s` accepts a stop request, while Core lifecycle stop waits for physical drain. Web owns persistent configuration/factory/maintenance; retired `u/factory/maint` commands must not act.

Debug injection covers normalized pad actions, source/connection/battery/gap, I²C failure/CRC/timeout/delay/raw requests, Matrix calibration/faults, display failure and shared-renderer benchmark. Synthetic camera inputs can operate a real camera and must warn. Physical gimbal control is blocked during SIM. Queue bounds, absolute player deadlines, cancel/release and fresh script expectations prevent stale actions.

Acceptance includes CRLF/backspace/errors, request-ID overflow, fresh ACK/token matching, two-board scripts, late/full/cancelled work, SIM exit with held inputs, same normalized physical/SIM actions, Release symbol exclusion and a real UART/I²C pass. Host fixtures alone do not prove serial or SMP timing. See [commands](../user-guide/serial.md), [design](../design/uart-debug-design.md) and [logging](../development/serial-log.md).
