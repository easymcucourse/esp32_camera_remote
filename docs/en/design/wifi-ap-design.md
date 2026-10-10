# Wi-Fi ownership

**English** · [简体中文](../../design/wifi-ap-design.md) · [日本語](../../ja/design/wifi-ap-design.md)

Core creates one `app_wifi` object. `wifi_esp32` owns SDK/netif/socket/NVS; `app_wifi_messages` supplies normal typed discovery/RSSI/status and two TCP lane owners. PTP never borrows raw fd. Maintenance directly uses the isolated facade after normal owners stop. Drivers use RAM storage; application config alone persists.

`network_config_t`: SSID1–32 bytes(no control/DEL, UTF-8 allowed), printable ASCII password8–63, country-limited channel1–13, show_password bool. WPA2-PSK/no required PMF/four clients are fixed. Random password12 characters uses rejection sampling; generation does not save.

NVS `wifi_ap/cfg`, version 1, fixed100-byte blob: offset 0 version,1 channel,2 flags(bit0 visibility),3 SSID length,4–35 SSID,36 password length,37–99 password. Require canonical zero padding/legal flags/length/content; unknown version is invalid. Default configuration deletes only cfg and commits, not whole NVS.

Core NVS init never automatically erases. Backend read can repair missing/invalid/oversized records with defaults and a write; therefore Core's no-writer test does not prove the whole boot is Flash-write-free. Storage mutex serializes reads/writes. Country-invalid but codec-valid fallback can stay in RAM. Namespace open failure is diagnostic, not automatic erase.

Web reserves reboot then stages prepare, ACK, commit. Config queue2/history8 preserves unfinished results. Worker validates, saves first, restarts AP only for network-field changes, and attempts old-record/AP rollback on failure. Core tracks completion and schedules1500ms reboot; save+radio and multi-namespace factory are not atomic. Normal config write message IDs return NOT_SUPPORTED.

Actual netif address and password/default label reach UI via copied state. RSSI follows selected camera MAC, approximately 2s; phones cannot become the camera by list order. Network generation invalidates old PTP channels and late replies; buffers are held until lease completion. Factory scopes preserve identities as described in [maintenance](maintenance-design.md). Real channels12/13, flash, AP restart/ACK loss and reset/re-pairing remain separate hardware tests. See [requirements](../request/wifi-ap-request.md) and [resources](module-resource-ownership.md).
