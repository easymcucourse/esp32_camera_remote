# M5Stack ATOM Matrix

**English** · [简体中文](README.zh-CN.md) · [日本語](README.ja.md)

Independent ESP-IDF5.5.1 firmware for the classic ESP32 in ATOM Matrix. It is an I²C slave at 0x42, receives controllers and controls RS 3 Mini locally. A dedicated status task owns the LEDs; LCD sends no RGB commands. The front button reports state and press count, not color changes.

## Build

From an exported IDF environment:

```sh
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash monitor
```

Replace COM6 with the actual port;115200 was verified locally. Exit monitor withCtrl+]. Defaults enable Classic Bluetooth/Bluedroid/HID Host and BTDM/BLE/GATTC, disable SPP, and use the v2 I²C slave with IRAM-safe ISR. Existing BR/EDR-only sdkconfig needs explicit regeneration/configuration; defaults do not overwrite it. Keep capacity for the BLE HID/gimbal connections. Portable isolated builds are described in [build instructions](../docs/en/development/build-and-flash.md). This project is not for ESP32-S3/C3 and needs no Arduino dependency.

## Board resources

| Resource | GPIO | Notes |
| --- | ---: | --- |
|5×5 WS2812|27|25 LEDs,GRB,IDF RMT|
|Front button|39|Active low,external pull-up|
|Grove SDA/SCL|26/32|Yellow/white|

Normal rows1/2/3 show DS/BLE/gimbal battery, at most five LEDs from left to right. At≤20% they flash red; disconnected/unknown values are dark. Row5 shows connections; startup/fault patterns override normal state. Battery parsing and physical LED direction are separate checks.

## DualShock 4

1. Disconnect controller USB, hold SHARE+PS until its bar flashes rapidly.
2. ATOM discovers `Wireless Controller`, authenticates and waits for valid input. `DualShock 4 connected; input ready` establishes readiness; HID-open acceptance alone does not.
3. Successful target identity persists in NVS and the stack stores bonding. Wake a saved DS4 with PS; ATOM reconnects and retries. Pair only one intended controller at a time.

Supported Sony VID/PID pairs are 054C:05C4 and 054C:09CC, checked through SDP. A third-party device with the same name is not automatically supported. Rumble, light-bar output and touch coordinates are absent. Scanning is about 10seconds and opening timeout about 20seconds.

Stick reports are-128..127, triggers0..255, battery0..10 or255unknown. Disconnect clears input. DEBUG fords4_host prints phase/report diagnostics; normal tags stayINFO. Initial report excerpts and ten-second statistics are diagnostics, not acceptance evidence. Raw device identities remain local.

Bits0..17: Share,L3,R3,Options,Up,Right,Down,Left,L2,R2,L1,R1,Triangle,Circle,Cross,Square,PS,Touchpad click. LCD receives an edge-based event cache; localL3 is removed. Options toggles LIVE/SETTINGS once per press. Left-stick data is not carried to LCD.

Camera mapping isL1 Wide/R1 Tele,Triangle Mode,Square Focus,RT half/full S1/S2,LT full recording. Lens declaration is POWER_ZOOM, not auto-detection. [Controller details](../docs/en/user-guide/controller.md) distinguish implemented and physically verified actions.

## RS 3 Mini

Activate through official Ronin, balance/unlock axes and disconnect the app. Without a saved target ATOM selects one name-matching Mini, verifies its control/notify channels and saves it when ready. Multiple candidates are not chosen. With a saved target only that address reconnects; `gimbal pair` replaces the gimbal target without changing DS4 pairing.

Real Classic DS4 left stick controls Pan/Tilt;L3 requests native recenter. Center/releaseL3 after connect. LCD disconnection/source selection does not affect the local source; UART SIM prohibits physical motion.

```text
gimbal status
gimbal speed pan 120
gimbal speed tilt 240
gimbal speed 120
gimbal invert 1
gimbal calibrate
gimbal stop
gimbal off
gimbal on
gimbal pair
```

Speed20..400 is a protocol offset, not angular velocity. The shared speed command sets both axes; named axes are independent. Factory defaults120/120; user-confirmed local tuning120/240 persists. Calibration requires fresh centered real input with offsets within 32. on/off,pair,tuning/invert/calibration persist;stop does not. Queue acceptance is not execution.

User confirmed basic stick/L3 and real gimbal power-cycle recovery. Custom recorded zero, soft limits and on-board settings are absent. Exact stopping/cancel/disconnect timing and full30-minute concurrency remain unverified; see [RS 3 Mini wire/limits](../docs/en/design/rs3-mini-protocol.md) and [current status](../docs/en/development/current-status.md).

<a id="lcd-主从通信"></a>
## LCD I²C link

Connect LCD SDA8/SCL9/GND to ATOM26/32/GND. With separate USB power do not connect Grove5V; pull up to3.3V, never5V. Address0x42 avoids the LCD expander at 0x24.

v2 is incompatible withv1, requiring both boards on upgrade. HELLO checks version/features/boot_id; offline probing is every 1second, online POLL every 50ms with 15ms write/read gap. Failed attempts preserve sequence/ACK for retry; three consecutive failures go offline. Version mismatch retries every 5seconds.

| Command | Request | Successful response | Payload |
| --- | --- | --- | --- |
|HELLO0x01|9bytes,param0x0202 plus selected input-mode byte|19bytes|boot_id,version,features,capacity,local_mask|
|POLL0x10|9bytes,param=ack_id|35bytes|device status,faults,batteries,buttons,input,event|

CRC-8/SMBUS vector `123456789`→0xF4. Error replies have zero payload and 7bytes. Shared atom_protocol/atom_client implement framing. The slave parser resynchronizes garbage and drops partial frames after 20ms. The core0 reply adapter replaces software/FIFO buffers using IDF5.5.1-specific internals; review it on SDK upgrade.

The 128-entry RAM event cache clears localL3 and deduplicates edges. Overflow setsgap, ACK clears the corresponding fault, later overflow must remain visible. Reconnect/reboot discards stale events and never replays old commands. Button count wraps after 65535. Gimbal states0/1/2/3 mean disabled/disconnected,searching,connecting,control-ready; fault bit3 is separate. [Complete offsets](../docs/en/design/i2c-protocol-design.md).

## BLE HID and validation

ble_clients owns shared GAP/GATTC callbacks and scanning; Classic callbacks remain independent. BLE candidates match appearance/supported names, then must expose verified HID services. Unique candidates only. No-IO bonding, Battery Service0x180F/Level0x2A19 updates about every 10seconds, invalid/disconnected value255.

Ultimate 2 input requires the observed113-byte descriptor and a unique notify characteristic. Its33-byte reports normalize through shared publication/I²C. DS4 has priority; missing BLE input releases after 1second. `ble map` prints the cached descriptor. Historical battery88% was observed; physical buttons/trigger directions,camera actions,reboot recovery and stability are still pending.

Run the repository host suite usingcmake/ctest. [Dated records](../docs/en/records/README.md) preserve earlier I²C retry and simulator results; they do not prove current30-minute acceptance. One process owns each UART. Captures/builds/backups/sdkconfig/memory are ignored.
