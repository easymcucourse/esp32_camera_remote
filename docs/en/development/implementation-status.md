# Implementation and validation history

**English** · [简体中文](../../development/implementation-status.md) · [日本語](../../ja/development/implementation-status.md)

Read [current status](current-status.md) for the latest baseline. This page explains how to interpret implementation history; the [dated Chinese ledger](../../development/implementation-status.md) retains per-batch detail and original evidence links.

| Area | Current implementation | Remaining proof |
| --- | --- | --- |
| Camera | DHCP identity selection, PTP/Sony backend, properties, targets, S1/S2, recording/zoom | Complete camera-mode/action matrix, fault recovery and stronger FPS goals. |
| Input/UI | Physical/SIM providers, typed actions, menus, information levels, release safety | Physical Ultimate 2 mapping, all layouts/latencies and cold-start behavior. |
| Wi-Fi/maintenance | Owner/backend split, startup-only no-auth Web, settings/reset and dual-slot LCD OTA | New split firmware browser, isolation, flash/power-loss and stop timing. |
| ATOM | Classic DS4, shared BLE scans, I²C v2, Matrix, Mini control | Visual fault patterns, full restart/disconnect matrix and 30-minute concurrency. |
| RS 3 Mini | User confirmed stick/L3, independent speed and gimbal power-cycle recovery | ≤100ms stop, center cancel, arbitrary zero/soft limits and real ATOM cold startup. |

2026-10-01 camera connection tests, 2026-10-03 OTA/recording-border tests, 2026-10-04 EV direction tests, 2026-10-05/06 module migration and 2026-10-07/08 live-view work each validate their own image and scope. Old PIN, editable hotspot menus, UART `u`, UNKNOWN lens and excluded gimbal statements are superseded by current behavior.

Code commit `11266fe` was locally validated with 267 host tests and four fresh firmware builds, then pushed. Those new binaries were not flashed. Current physical ATOM results belong to the earlier flashed Mini-control image; LCD visual/camera outcomes cannot be inferred from an ATOM flash.

Acceptance requires explicit evidence for each dimension: source, host, build, flash, physical effect, performance and stability. A token/ACK proves only the corresponding stage. An interrupted run remains partial. See [module acceptance](module-split-checklist.md) and [historical index](../records/README.md).
