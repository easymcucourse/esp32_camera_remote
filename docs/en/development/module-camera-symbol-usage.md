# Camera symbol audit

**English** · [简体中文](../../development/module-camera-symbol-usage.md) · [日本語](../../ja/development/module-camera-symbol-usage.md)

The 2026-10-06 [detailed symbol ledger](../../development/module-camera-symbol-usage.md) records **39 production global PTP/Sony functions**, 37 present in its Default ELF and two retained for parser fixtures. The earlier 47-function and 40-function snapshots are historical local artifacts. These counts are dated, not a fresh count of every symbol in the latest binary.

Sony-specific parsing, formatting and wire-value encoding belong in `camera_backend_sony/private`; the backend embeds one `ptpip_client_t`. Shared PTP wire/session logic belongs in `ptpip`; generic runtime/control belongs in `app_camera`. Old fd and exposure forwarding wrappers with fixture users were moved to test-only support rather than deleting their tests. Constants/helpers require their own source-reference review.

Retain a symbol when it has a production caller, a requirement-supported API or a meaningful fixture dependency. Remove/merge only after all three checks and preserve byte-level assertions. ELF absence alone can reflect dead stripping, not lack of source use. Direct archive edges and public/private include gates supplement the ledger, but do not observe indirect callbacks.

Rebuild current configurations through `tools/ci_build.py`, inspect their ELF/MAP and `module-symbol-edges.json`, and run host contracts before changing protocol entry points. The 2026-10-10 four-build audit passed; a real camera smoke test is still required for behavior affected by protocol cleanup. See [dependencies](../design/module-dependency-graph.md) and [validation](current-status.md).
