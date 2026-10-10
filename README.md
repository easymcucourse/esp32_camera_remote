# ESP32 Camera Remote

**English** · [简体中文](README.zh-CN.md) · [日本語](README.ja.md)

A Sony ZV-E10 camera remote and live-view display for the Waveshare ESP32-S3-Touch-LCD-7B. An M5Stack ATOM Matrix receives controllers, supplies camera input over I²C, and independently controls a DJI RS 3 Mini over BLE.

## Current state

The baseline is 2026-10-10. [Current implementation and validation](docs/en/development/current-status.md) separates source, builds, flashed firmware and physical results.

- LCD: 1024×600 RGB565, 18MHz pixel clock, no touch initialization; LIVE, SETTINGS, MORE, confirmed parameter targets, recording indicators and information levels.
- Camera: dynamic discovery, normal on-camera pairing confirmation, command/event PTP/IP channels, JPEG live view, properties and control state machines. The current lens is declared power zoom by the user; there is no automatic lens identification.
- ATOM: Classic+BLE, DS4 and a descriptor-specific Ultimate 2 parser, batteries, Matrix status and I²C v2. Ultimate 2 physical mapping/camera control remains pending.
- RS 3 Mini: real Classic DS4 left stick, native L3 recenter, first-device selection/saved-target reconnection, independent Pan/Tilt tuning and stop gates. The user confirmed stick/L3 and gimbal power-cycle recovery. Custom recorded zero, soft limits and an on-board settings menu are not implemented.
- All 267 host tests and four LCD/ATOM Debug/Release builds passed. Exact stop latency, full 30-minute concurrency and LCD gimbal-fault visuals remain unverified; builds do not prove hardware acceptance.

## Hardware

| Item | Configuration |
| --- | --- |
| Display | ESP32-S3-Touch-LCD-7B, 16MB Flash, 8MB Octal PSRAM |
| Extension | ATOM Matrix, classic ESP32; this firmware is not for S3/C3 |
| I²C | LCD SDA8/SCL9 → ATOM SDA26/SCL32, common GND |
| Bus | 100kHz, 3.3V pull-ups, slave address 0x42 |
| Targets | Sony ZV-E10 / DJI RS 3 Mini |
| SDK | ESP-IDF5.5.1; manifests/lock files pin dependencies |

With separate USB power connect only SDA/SCL/GND, leaving Grove5V disconnected. See [hardware](docs/en/design/hardware-design.md).

## Build and update

From the repository root in an exported ESP-IDF5.5.1 environment:

```sh
python tools/ci_build.py lcd debug --build-tag local
python tools/ci_build.py atom debug --build-tag local
python tools/ci_build.py lcd release --build-tag local
python tools/ci_build.py atom release --build-tag local
python tools/ci_build.py lcd debug --profile stable --build-tag local
```

Artifacts are in `build/ci-<board>-<flavour>[-stable]-local`. Release excludes simulator implementations. ATOM requires BTDM/BLE/GATTC; defaults do not overwrite an existing sdkconfig. Default experimental clocks and Stable 80MHz use separate configurations, without a stability guarantee.

Follow [build/flashing](docs/en/development/build-and-flash.md) after checking the actual port, current partitions and artifacts. COM8/COM6 are historical examples; ATOM flashing at 115200 has been verified. Prefer Web OTA for deployed LCDs: full initial flashing can initialize OTA metadata and is not an application-only update. An I²C v1→v2 upgrade requires both boards.

## Operation

1. Join the displayed LCD access point from the camera, enable PC Remote/Wi-Fi access-point connection, and confirm first pairing on the camera. DHCP discovery supplies its address; do not hard-code a capture IP.
2. Pair DS4 with SHARE+PS until its light bar flashes rapidly; use PS to wake a saved controller. Valid reports indicate readiness; the Matrix LCD-link light does not prove controller connectivity.
3. Activate RS 3 Mini with the official app, balance it, unlock axes and disconnect Ronin App. Without a saved target ATOM selects one matching Mini; with a saved target it reconnects only that unit. Enter `gimbal pair` on ATOM UART to replace it. Center the stick and release L3 after connection.

| Input | Action |
| --- | --- |
| Options/Start | LIVE ↔ SETTINGS |
| L1/R1 | Wide/Tele; both shoulders stop and latch until released |
| Square/X, Triangle/Y | Focus mode, exposure Mode; once per press |
| R2/RT | Half: S1 focus; full: S2 shutter |
| L2/LT | Half: no action; full: recording target toggle |
| D-pad | SETTINGS navigation/editing; right increases EV, left decreases |
| Cross/A, Circle/B | Confirm/back; MORE extra properties, Wi-Fi information only |
| Touchpad click | No action; save information level through startup Web |
| Left stick, L3 | ATOM-local gimbal Pan/Tilt, native recenter |

Release triggers after connection; unknown recording state is not guessed. Camera source selection is independent of the gimbal's real Classic DS4 input. UART simulation does not move a physical gimbal. See [controllers](docs/en/user-guide/controller.md) and [camera](docs/en/user-guide/camera.md).

```text
gimbal status
gimbal speed pan 120
gimbal speed tilt 240
gimbal off
gimbal on
```

These are ATOM UART commands. The 20..400 range is a protocol offset, not degrees/second. `gimbal speed 120` sets both axes. Factory defaults are 120/120; this hardware was confirmed at 120/240. Settings persist in NVS.

LCD UART: `j` starts/resumes, `s` stops, `S` toggles SETTINGS, `p` runs pairing/reconnection diagnostics while stopped. See [serial commands](docs/en/user-guide/serial.md).

## Maintenance and tests

Only the startup connection page exposes the HTTP maintenance trigger. Visiting the displayed IP claims exclusive MAINTENANCE and drains ordinary camera/controller services. Web settings cover Wi-Fi, preferences, resets and OTA. There is no PIN/login; AP clients can maintain the device. Successful save/exit/OTA restarts the LCD rather than returning directly to live view. Normal controller/UART paths do not edit Wi-Fi or run factory reset. See [quick start](docs/en/user-guide/quick-start.md).

```sh
cmake -S tests/host -B build/host
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
python tools/check_doc_links.py
python tools/check_module_boundaries.py
```

Host Web tests need cJSON (`libcjson-dev` on Linux, or an SDK source directory on Windows). Tests verify software boundaries, not Flash, radio, pixels or camera actions. Raw logs, captures and local agent memory stay Git-ignored.

## Documentation and source

[Documentation](docs/README.md) is ordered English, Chinese, Japanese. Current guides, development notes, requirements and designs are separate from dated [historical evidence](docs/records/README.md).

`main/` is the LCD entry; `components/` contains Core, routing, network, input, camera, UI and board modules; `common/` has shared pure protocols; `m5_atom_matrix/` is the independent extension; `tests/host/` contains regressions; `tools/` covers builds, serial capture and analysis. Font licenses are in `components/app_ui/fonts/`; protocol reference licenses are in `third_party/rs3-protocol/`.

This is an unofficial project. Vendor names identify compatibility targets, not endorsement. See protocol/validation limits and [evidence publication rules](docs/README.md#通信记录的公开范围).
