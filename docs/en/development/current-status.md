# Current implementation and validation

**English** · [简体中文](../../development/current-status.md) · [日本語](../../ja/development/current-status.md)

Baseline: source and checks on 2026-10-10. Requirements retain unfinished targets; “current” in dated records means the record's date.

## Implementation

| Area | Current source | Validation boundary |
| --- | --- | --- |
| LCD architecture | Core composition root; typed Console router; independent Wi-Fi/Input/Camera/UI owners; UI→display_surface→board_7b; retired APIs only in tests/support/legacy | Full stop/error paths and long operation are not all hardware-accepted |
| Camera | ZV-E10, AP DHCP discovery/on-camera pairing, two PTP/IP channels,0x9209 properties,0xFFFFC002 JPEG, target read-back and safe release | Historical results cover some actions, not every lens/enumeration/camera |
| Input | Classic DS4 and descriptor-specific Ultimate 2 BLE reports through ATOM/I²C; current lens declaration POWER_ZOOM | No automatic lens identification; non-power-zoom MF fallback is inactive; BLE physical mapping/camera control pending |
| UI/maintenance | LIVE/SETTINGS/MORE, information levels, recording status; startup HTTP claims exclusive Web maintenance for Wi-Fi/preferences/reset/OTA, then restarts | Normal UART/controller do not edit Wi-Fi/reset; Web has no PIN/login; mobile/fault coverage pending |
| ATOM | Classic+BLE, shared BLE scanning, independent Matrix, batteries, I²C v2, fresh real DS4 reports | BLE connection is not proof of physical input; Matrix/LCD visual checks are separate |
| RS 3 Mini | First unique candidate/saved target, notifications, left stick, native L3 recenter, stop gates, independent NVS Pan/Tilt offsets | User confirmed stick/L3,120/240 tuning and gimbal power cycle; custom zero/soft limits/on-board menu absent |

## Software checks

Code commit11266fe was pushed to origin/main. CTest267/267 passed (C scenarios and Python containers, not267 individual assertions). Fresh cleanup20261010 LCD/ATOM Debug/Release builds passed, including LCD component graph/direct-symbol ownership and release simulator-symbol gates; LCD images meet the5MiB budget. Documentation work did not flash hardware: new builds are not the running firmware.

## Hardware and unfinished work

This Mini exposes notify-only FFF4, initialized through CCCD. The endpoint4 heartbeat restored user-confirmed stick/L3 motion. Pan120/Tilt240 survived an RTS software reset; factory defaults remain120/120. Offsets are not angular velocity. Real gimbal power cycling recovered automatically with fresh DS4 input; the user reported normal operation. A real ATOM cold power cycle is still pending.

Before activation,04/66 carried parseable three-axis TLVs. The observed post-activation19-byte packet lacks them, so strict pose_raw stays invalid. Units are uncalibrated; raw data does not drive closed-loop movement or limits. Exact release-to-stop≤100ms, recenter cancellation, abnormal disconnect and full30-minute concurrency remain pending. Healthy partial sampling for about 14minutes is not a30-minute pass. LCD gimbal-fault propagation is implemented/built/host-tested, but unflashed and visually unverified.

Measured live-view memory settings remain: PSRAM preference for Wi-Fi/lwIP, internal reserve32768, static TX6, cache32, main stack24576. Default experimental clocks and Stable 80MHz are independent. The 2026-10-08 long run had no NO_MEM but roughly4.76FPS average still missed its performance gate. That optimization task stopped; these builds do not establish performance acceptance.

## Evidence

- [RS 3 Mini protocol](../design/rs3-mini-protocol.md), [test record](../../records/rs3-mini-test-20261010.md), [acceptance audit](../../records/rs3-mini-acceptance-20261010.md).
- [Live-view record](../../records/liveview-execution-20261008.md), [performance audit](../../records/liveview-acceptance-checklist-20261008.md).
- [Module status](module-split-status.md), [acceptance checklist](module-split-checklist.md).

Raw UART, captures, images and real identities remain ignored; published records contain redacted facts/statistics.
