# Module acceptance checklist

**English** · [简体中文](../../development/module-split-checklist.md) · [日本語](../../ja/development/module-split-checklist.md)

This groups the **133 requirements and validation entries** in the [Chinese checklist](../../development/module-split-checklist.md). Its individual IDs and evidence links remain the detailed audit source. Requirements marked source/host/build complete may still need hardware acceptance; [current status](current-status.md) governs the latest totals.

| Group | Review and acceptance obligation |
| --- | --- |
| Boundaries | Main only starts Core; Console has no functional-component dependency; UI/surface/board, Wi-Fi/backend and Camera/PTP/Sony headers stay within owners. |
| Router/messages | Correlation, absolute deadlines, generation, late replies, full queues, control priority and lease retirement must be validated. |
| Input | Compare normalized physical/SIM actions; reject stale epochs; fully release on gap, offline, source switch and stop; no SIM symbols in Release. |
| Frames/display | Camera leases are readonly and reclaimed on every exit; UI holds only a back canvas; front buffer is never modified during scan; bench shares renderer. |
| Wi-Fi/PTP | Network generation invalidates old channels; one PTP session/transaction engine; no socket bypass outside wifi_esp32. |
| Maintenance | Startup HTTP claim only, irreversible exclusive mode, fixed screen, no authentication, AP-scoped HTTP, no normal settings writer. |
| Storage/factory/OTA | Core coordinates narrow write owners; factory scopes and partial failure are explicit; OTA validates image, reserves restart and preserves rollback. |
| Lifecycle | Cold start, each partial-init failure, bounded drain, outstanding messages and real restart must be exercised. |

Production-source fixtures prove many local behaviors. Actual Camera→router→UI integration fixtures strengthen lease/control evidence; they still use cooperative scheduling and fake hardware. Symbol/dependency scans prove direct paths only. A historical green checkbox is not proof of current physical timing.

Remaining cross-system tests include true RGB scanning under SMP, input-to-camera release, blocked HTTP stop, AP/other-netif isolation, flash/cache-off operations, first-start safety, real reconnect and 30-minute runs. Keep code, host, build, flash, visuals, timing and stability results separate. See [ownership](../design/module-resource-ownership.md), [messages](../design/module-message-contracts.md) and [dependency graph](../design/module-dependency-graph.md).
