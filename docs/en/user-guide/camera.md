# Camera connection

**English** · [简体中文](../../user-guide/camera.md) · [日本語](../../ja/user-guide/camera.md)

The current target is Sony ZV-E10. Enable PC Remote over Wi-Fi and connect the camera to the LCD access point. Read the SSID and password from the connection page; saved settings override the defaults in `common/network_config.h`.

1. Start the LCD and confirm that the access point is ready.
2. Join that network from the camera and open its pairing screen.
3. Accept **ESP32-Camera-Remote** when prompted. The LCD saves its GUID and the selected camera identity only after successful initialization.
4. The first decoded frame opens LIVE: 1024×576, with 12-pixel borders above and below. Options/Start or UART `S` switches to the 768×432 SETTINGS preview.
5. Use `s` to stop and `j` to resume. `p` runs pairing/reconnection diagnostics while the camera task is stopped.

The camera address comes from DHCP, rather than a fixed `.2` address. Before binding, only one reachable TCP 15740 candidate is selected. Several candidates require disconnecting the extra cameras. After binding, only the saved camera is selected. Ordinary application updates preserve NVS; erasing it removes the identity.

To change cameras, restart the LCD, enter the startup maintenance website and perform the confirmed full reset. This also resets LCD access-point and UI settings; it preserves ATOM bindings. There is no UART `u` command. See [quick start](quick-start.md).

Properties are refreshed by events and approximately every five seconds. `--` means unavailable; unknown enumerations remain hexadecimal. WB temperature may be a saved value rather than the active white-balance mode. Wireless flash and WB AB/GM values remain raw where their meaning is unconfirmed.

The current lens is declared **POWER_ZOOM** by the user. L1 is Wide, R1 Tele; the conditional non-power-zoom MF fallback is inactive. RT provides S1/S2 focus/shutter stages; LT full press requests a recording target. A successful command response does not prove recording or a parameter change: check actual readback and the camera itself.

If pairing is rejected, confirm authorization on the camera before `j/p`. Network failures use 1–30-second backoff. A complete `0x200F` response can reuse the session; malformed data or sustained failures follow recovery policy. Preserve the preceding transaction and stage logs. See [troubleshooting](troubleshooting.md), [controls](controller.md) and [current validation](../development/current-status.md). Historical camera tests do not establish all effects of the newest unflashed build.
