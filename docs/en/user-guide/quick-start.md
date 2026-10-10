# Quick start

**English** · [简体中文](../../user-guide/quick-start.md) · [日本語](../../ja/user-guide/quick-start.md)

Use the Waveshare LCD-7B, ATOM Matrix, Sony ZV-E10 and DS4, with separate USB power. LCD targets ESP32-S3; ATOM targets classic ESP32. Full build/profile/update details are in [build and flash](../development/build-and-flash.md).

## Initial LCD installation

From the root in an exported ESP-IDF 5.5.1 environment, replacing COM8 with the actual port:

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

Exit monitor with Ctrl+]. Pairing identity remains if NVS is not erased. Full USB flash writes partitions/initial OTA metadata and can change a deployed boot slot; use Web OTA or verify the active slot before an application update. Whole-chip erase removes identity and requires pairing again.

## ATOM installation

```sh
cd m5_atom_matrix
idf.py set-target esp32
idf.py build
idf.py -p COM6 -b 115200 flash
```

COM8/COM6 are historical examples, not device detection. Verify ports. ATOM at 115200 is verified locally. I²C v1→v2 requires both boards. Existing ATOM sdkconfig must actually enable BTDM/BLE/GATTC; defaults alone do not change it.

## Wiring and camera

LCD SDA8/SCL9/GND → ATOM SDA26/SCL32/GND. With separate USB supplies, leave Grove 5V disconnected. Use 100kHz and 3.3V pull-ups.

1. Read the AP configuration on the startup page or with `wifi show`; saved settings override factory values in common/network_config.h.
2. Join that AP from the camera, enable PC Remote and select Wi-Fi access-point connection.
3. Approve `ESP32-Camera-Remote` on the camera when prompted.
4. Continuous live view starts after the first successfully decoded frame; recoverable failures return to status/retry.

[Camera discovery/replacement](camera.md), [controller/gimbal operation](controller.md), and [troubleshooting](troubleshooting.md) provide the next steps.

## Maintenance Web

For Wi-Fi/preferences/reset/OTA, restart LCD, join its AP and visit the displayed IP (normally http://192.168.4.1/) while the startup connection page remains active. Normal mode closes port80; restart to try again if the window has passed.

The first request claims exclusive maintenance and redirects to its home page. Ordinary services drain and the LCD displays MAINTENANCE; camera/controller/UART cannot continue normal control, and maintenance does not return directly to live view. There is no PIN/login: any AP client has maintenance access. Successful save/exit/restart/OTA restarts the LCD; reconnect to a changed AP if necessary.

AP reset preserves camera pairing. Full reset also clears LCD camera binding/display preferences, retaining ATOM bindings. Failures can leave partial record changes; inspect page errors and boot logs. Upload only the LCD application bin, never ATOM, bootloader or a merged image. Initial migration to dual OTA partitions still requires USB partition installation. Current mobile/browser/visual/OTA fault acceptance is incomplete; see [status](../development/current-status.md).
