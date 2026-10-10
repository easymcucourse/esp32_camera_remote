# Controllers and gimbal

**English** · [简体中文](../../user-guide/controller.md) · [日本語](../../ja/user-guide/controller.md)

ATOM receives Classic DualShock 4 and publishes camera input over I²C v2. Its BLE client includes a descriptor-specific Ultimate 2 parser; connection/battery results do not establish complete physical mapping or camera control. See [current status](../development/current-status.md).

## Wiring and pairing

Connect LCD SDA8/SCL9/GND to ATOM SDA26/SCL32/GND,100kHz with 3.3V pull-ups at 0x42. With separate USB supplies, do not connect Grove5V. Upgrade both boards for I²C v1→v2.

First DS4 pairing: hold SHARE+PS until the light bar flashes rapidly. Saved pairing: wake it with PS. Matrix LCD connectivity and Classic controller connectivity are separate; valid input reports establish readiness.

## Controls

X/Y below use Xbox layout names: DS4 Square/Triangle.

| Input | Action |
| --- | --- |
| Start/Options | Toggle LIVE/SETTINGS |
| L1/R1 | Wide/Tele; near/far focus only for an explicitly non-power-zoom lens in MF |
| Y/Triangle | Next exposure Mode, no held repeat |
| X/Square | Next focus mode, no held repeat; cancel an active shoulder action |
| RT/R2 | Half:S1; full:S2; leaving full releasesS2, full release releasesS1 |
| LT/L2 | Half:no action; each full-press edge requests start/stop from confirmed recording state; release changes neither focus nor recording |
| D-pad up/down | Cycle seven settings, MORE and Wi-Fi information |
| D-pad left/right | Edit; enumeration wraps, EV right increases/left decreases |
| A/Cross | Confirm; open MORE; Wi-Fi is information only |
| B/Circle | Return from MORE; no AP editor |
| Touchpad click | No action; save information level in startup Web |
| Left stick/L3 | ATOM-local RS 3 Mini Pan/Tilt/native recenter |

Camera shoulder/X/Y/trigger rules are the same in LIVE and SETTINGS. Both shoulders stop and latch until both are released. D-pad repeat starts after 400ms, then every 150ms; late polling does not replay missed repeats. Held directions must be released after page/source/session changes or an event gap.

## Settings and confirmation

The main menu contains Shutter,F-Number,ISO,EV,WB,Focus,Metering. Missing/read-only/disabled capabilities are grey. Camera enumeration determines valid values; input coalesces around the latest target and waits for read-back before sending another target.

Shutter/aperture use adjacent enumeration values when available; otherwise a valid writable current value allows a step command, one step per confirmed change. Rejection/timeout clears pending steps. These non-enumeration step commands still require ZV-E10 physical protocol validation. No valid current value means no write.

`TO ...` shows the target while the property row retains the camera's actual value. `PENDING` lasts until read-back, followed briefly by `APPLIED`, `REJECTED` or `TIMEOUT`. Request acceptance is not physical application. MORE displays additional capture properties; Wi-Fi only displays current network information. Changes/reset/preferences belong to startup maintenance Web, not a controller editor.

## Gimbal

Activate/balance/unlock RS 3 Mini and disconnect Ronin App. Without a saved target, ATOM selects one matching Mini; multiple candidates are not selected. Saved targets do not switch automatically: use ATOM UART `gimbal pair` to replace one. Center the left stick and releaseL3 after reconnect.

Only fresh real Classic DS4 input drives the gimbal; LCD/source selection is independent, and UART SIM blocks physical movement. `gimbal speed pan 120` / `gimbal speed tilt 240` tune axes independently; `gimbal speed 120` sets both. Range20..400, protocol offsets rather than degrees/second, persisted in NVS. User confirmed stick/L3 and gimbal power-cycle recovery. Native recenter is not a custom recorded zero; soft limits/menu are absent. Exact stop timing, recenter cancel and 30-minute concurrency remain pending. See [protocol and limits](../design/rs3-mini-protocol.md).

## Limits

The current lens declaration is POWER_ZOOM, not automatic detection; non-power-zoom MF fallback is inactive. Release triggers after connection. Unknown recording state is not guessed. Disconnect/gap/session change releases old actions; reconnect does not replay them. Select focus-point mode, right-stick placement andR3 focus-point reset remain unimplemented. History and software/physical evidence are separate.
