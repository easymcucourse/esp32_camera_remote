# Troubleshooting

**English** · [简体中文](../../user-guide/troubleshooting.md) · [日本語](../../ja/user-guide/troubleshooting.md)

Save the running version, mode, status and preceding failure logs before changing configuration. [Current status](../development/current-status.md) separates code, builds, flashing, physical actions and stability.

| Symptom | Check / recovery |
| --- | --- |
| Waiting for camera | Camera has joined the LCD AP, has a DHCP lease and PC Remote enabled; saved identity matches. |
| Multiple cameras | Keep only the intended TCP 15740 candidate connected for first pairing. |
| Pairing rejected or identity changed | Confirm on the camera; use startup Web full reset only when replacement/re-pairing is intended. |
| `0x200F` | Preserve the complete response and mode. It may be a temporary refusal; do not treat every refusal as permanent disconnection. |
| Recording request accepted but no recording | Inspect actual recording readback and camera mode. An accepted request is not proof of recording. |
| Wrong/unknown settings | Check descriptor validity, writable flag, target state and readback. Raw/unknown values must not be guessed. |
| Display sync failure | Stop writing an unsafe buffer; firmware attempts bounded recovery and drains before restart. Save failure and heap logs. |
| ATOM offline or protocol mismatch | Check GPIO8/9 ↔ 26/32, common ground, 3.3V pullups, address `0x42`, and v2 firmware on both boards. |
| DS4 searching | First pairing uses SHARE+PS; a saved controller uses PS. HID opening alone is not valid input. |
| BLE controller connected but input absent | Current parser supports the captured Ultimate 2 descriptor/report; arbitrary Xbox BLE controllers are not certified. |
| Gimbal searching | Power on and activate the Mini, release the Ronin App connection, and inspect `gimbal status`. Saved targets are not replaced automatically. |
| New gimbal not selected | `gimbal pair` clears the gimbal target; leave only the intended Mini powered on. It preserves DS4 binding. |
| Gimbal linked but motion absent | Wait for service/notification/neutral initialization, press DS4 PS, release the stick and L3, then push. SIM and stale input are blocked. |
| Gimbal too slow | Set `gimbal speed pan N` or `gimbal speed tilt N`, range 20–400; local tested values are 120/240. |
| Activation prompt | Complete activation with official Ronin App, disconnect it and reconnect ATOM. |

Restart into the startup connection page to access maintenance at the LCD AP address, normally `http://192.168.4.1/`. Normal mode closes that trigger. There is no PIN or UART factory command. AP-only reset preserves camera identity; full reset clears LCD identity/preferences and AP settings, but keeps ATOM bindings. Confirmation is required; failures may leave partial changes. NVS erasure is a last recovery operation, not a routine update.

Use one serial owner per port, 115200 baud. Logger exit does not stop firmware. `--reset` deliberately reboots. Keep raw logs/captures in ignored directories and publish redacted conclusions. See [logging](../development/serial-log.md), [quick start](quick-start.md) and [gimbal protocol](../design/rs3-mini-protocol.md).
