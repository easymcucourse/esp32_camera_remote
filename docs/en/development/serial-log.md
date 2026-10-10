# Serial logging

**English** · [简体中文](../../development/serial-log.md) · [日本語](../../ja/development/serial-log.md)

Install `pyserial` with `python -m pip install -r tools/requirements.txt`, or use IDF Python. `tools/serial_log.py` records raw bytes at 115200 and prints UTF-8 with replacement for invalid bytes. It disables DTR/RTS before opening; only `--reset` deliberately pulses RTS. Closing the logger does not stop live view.

```sh
python tools/serial_log.py --port COM8 --reset --seconds 30 --output build/boot.log
python tools/serial_log.py --port COM8 --command j --seconds 30 --output build/live.log
python tools/serial_log.py --port COM6 --seconds 60 --output build/atom.log
```

`--port` defaults to COM8, `--seconds` to 25 and `--output` to `build/serial-boot.log`. Output overwrites an existing file. `--command` sends one ASCII line without reset; `--until` ends on a matching ASCII string. Replace ports and close all other monitors first. Record both boards in separate terminals with one owner per port.

`tools/test_camera_connection.py --port COM8 --connect-wait 120 --steady 30 --output captures/connection-test` checks stop/start, first frame, steady operation and saved-identity reconnection. It requires the camera task initially running, does not reset or clear pairing, and leaves live view running. A missing camera/DHCP lease can fail prerequisites. Hardware reboot persistence needs a separate reset test.

Look for `SESSION VERIFIED`, `LIVEVIEW RUNNING`, `LIVEVIEW frames=… fps=…`, `ATOM v2 online`, input-ready and overflow messages. Older AP/init/vendor logs may no longer exist; use current UART `status` semantic snapshots. `s` acknowledgment is admission, not the physical task exit. Save heap low-water marks and largest free blocks as well as current free memory.

Analyze isolated full-screen or SETTINGS captures:

```sh
python tools/analyze_liveview.py build/live.log --expect-settings 0 --output build/live-summary.json
```

Use `--expect-settings 1` for SETTINGS. Mixed/missing page metadata returns failure while retaining useful statistics. FPS is weighted across valid windows; reset/stream gaps and initial short windows are excluded. Read/display/JPEG percentiles describe approximately five-second log samples of the last frame, not every frame. Keep at least 615 seconds for a 600-second window. The tool does **not** declare performance or stability acceptance automatically. Record version, environment, mode and visual confirmation separately. Keep raw evidence in ignored `build/` or `captures/`; see [commands](../user-guide/serial.md).
