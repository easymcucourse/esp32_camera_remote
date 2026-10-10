# Serial commands

**English** · [简体中文](../../user-guide/serial.md) · [日本語](../../ja/user-guide/serial.md)

Both consoles use **115200 baud**, newline-terminated commands, a 255-byte limit, quoted arguments and backslash escapes. Common commands are `help`, `version`, `status`, and `log <tag|*> <none|error|warn|info|debug|verbose>`. Prefixing `#123` correlates a synchronous response; asynchronous operations also have a token. A queue acknowledgment is not completion.

| LCD command | Meaning |
| --- | --- |
| `j` / `s` | Start/resume; request cancellation and drain. `s` acknowledgment only means admission. |
| `S` | Switch LIVE/SETTINGS. |
| `p` | Pair/reconnect diagnostic while stopped. |
| `wifi show` / `wifi show password` | Query configuration; the latter explicitly prints the password. |
| `ui info` / `ui pad` | Query preferences loaded at startup. |
| `extra status` | Query MORE property values, writable state and target status. |
| `i2c log on|off|changes`, `i2c stats`, `i2c stats reset` | Physical bus monitoring and statistics. |

Configuration writes, factory reset and maintenance entry are handled by the **startup website**. UART `u`, `factory`, `maint on/off/status/probe`, hotspot editors and preference setters are retired. The current Input service ignores touchpad information-next; display levels are saved on the website and loaded after reboot.

| ATOM command | Meaning |
| --- | --- |
| `gimbal status` | State, saved-target presence, speed, input freshness, TX/RX and faults. |
| `gimbal on` / `off` | Persist enabled state; off stops control and closes the link. |
| `gimbal pair` | Clear only the local gimbal target and scan again; DS4 binding remains. |
| `gimbal stop` | Request neutral/safety stop. |
| `gimbal calibrate` | Calibrate centered, fresh physical DS4 input; invalid input is rejected. |
| `gimbal speed 20..400` | Set both axes. |
| `gimbal speed pan 20..400` / `tilt 20..400` | Set one axis independently. |
| `gimbal invert 0|1` | Persist Tilt inversion. |

Debug builds register `pad sim`, connect/disconnect, battery, tap/hold/release, stick/trigger and bounded `seq` playback. LCD also has `atom sim`, online/offline/reboot/version and fail/crc/timeout injection; ATOM offers `i2c drop/corrupt/delay`, raw nine-byte `i2c req`, and LED calibration/fault overlays. `display fault` and `display bench` require the relevant debug options. Release excludes these functions. SIM can operate a real connected camera; it cannot drive the physical gimbal.

For scripts, use `python tools/uart_script.py --port lcd=COM8 --port atom=COM6 --script tools/uart_scripts/pair-i2c-monitor.uart --log build/i2c.log`, replacing ports. `@lcd/@atom` select an endpoint; `wait`, `expect`, `expect-any` and `!` express timing, fresh output and expected rejection. Some scripts intentionally reboot or inject faults: read the script before using it.

Only one process may own a port. See [logging](../development/serial-log.md) and [UART design](../design/uart-debug-design.md). `record known/recording/pending` distinguishes readback from a request; extra status codes are 0 idle, 1 pending, 2 applied, 3 rejected, 4 timeout, 5 accepted.
