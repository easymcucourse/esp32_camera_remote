# UART console design

**English** · [简体中文](../../design/uart-debug-design.md) · [日本語](../../ja/design/uart-debug-design.md)

LCD Console owns independent router and UART gateway. Common debug_console/debug_line/debug_args/async_token implement line reading, parsing, output and correlation; gateway encoders use only public message contracts and pure values. Maintenance does not depend on Console. ATOM is a separate firmware whose local console routes to its own owners.

UART0 at 115200, RX512, reader internal4096-byte priority 2,20ms read loop. Content limit255 bytes; CR/LF terminates once for CRLF; backspace removes a byte. Invalid control/non-ASCII/overlong lines are discarded through terminator, never partially executed. Quotes/escapes are parsed by pure C, not esp_console/linenoise. Optional #uint32 tags sync responses; nonzero tokens identify asynchronous completion.

Core registers UART inbox8 control/1 bulk before starting gateway and freezes routing after startup. Reader exit retires only its endpoint, never joins itself/stops business services. External quiesce closes admission, cancels requests, joins reader then deletes driver; failure retains cleanup for retry without a duplicate reader. `status` collects snapshots and fails if any are absent. `s` admits stop; Core stop physically drains.

Production help/version/status/log, camera j/s/S/p, readonly wifi/ui, I²C monitor remain. Removed factory/u/maint and config writers must not act. Debug SIM/player/bench/fault are compile-gated; Release retains an empty IDF SIM component registration but no implementation/symbols.

SIM uses the same Input report API while physical polling continues. Source selection first releases. Player10ms uses absolute deadlines, queue4 jobs/8 completions; cancel removes queued and held state. Camera SIM can have real effects and warns; gimbal SIM is blocked. Bench gets a canvas only through UI. See [command guide](../user-guide/serial.md), [logging](../development/serial-log.md), [messages](module-message-contracts.md) and [tests](../development/testing.md). Host fixtures prove parser/owner contracts, not real UART or SMP timing.
