# Module migration status

**English** · [简体中文](../../development/module-split-status.md) · [日本語](../../ja/development/module-split-status.md)

The production module split is present in source. `main` starts Core; Core owns composition, modes, startup barrier, health, restart, lifecycle and factory coordination. Normal services use typed messages through Console/router. See [architecture](../design/architecture-design.md).

UI → display surface → board separates presentation, canvas ownership and hardware. Camera owns its runtime/backend/session/control/JPEG slots; Sony owns vendor interpretation and embeds one PTP client. PTP uses Wi-Fi message channels without raw sockets. Input owns reports and actions; physical ATOM and Debug SIM are separate providers. UART encodes messages without business ownership.

Startup maintenance is exclusive and irreversible until restart. Its first HTTP claim stops/drains normal owners and freezes a fixed MAINTENANCE screen before enabling full Web. There is no authentication, normal-mode maintenance reentry, UART configuration editor or returning to live view without reboot. Core coordinates narrow storage callbacks; app_maintenance does not control camera/UI directly.

Migration fixtures and four current builds passed; [current status](current-status.md) has the latest count. Historical five-profile audits include Stable, but are dated evidence, not a fresh five-profile claim. The [Chinese batch ledger](../../development/module-split-status.md) links each migration step and original fixture evidence.

Hardware migration is **not fully accepted**. Earlier real tests exposed internal RAM pressure, router SMP lock contention and input/deadline issues; fixes and short tests are recorded. Later live-view tests remained below final performance targets. Real startup order, all-owner drain timing, HTTP isolation, storage/power-loss, full visual behavior and long stability still need their own tests. See [acceptance checklist](module-split-checklist.md).
