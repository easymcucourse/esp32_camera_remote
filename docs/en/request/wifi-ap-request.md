# Wi-Fi AP requirements

**English** · [简体中文](../../request/wifi-ap-request.md) · [日本語](../../ja/request/wifi-ap-request.md)

The LCD creates a WPA2-PSK AP with DHCP, four-client limit and no required PMF. Defaults are defined in `common/network_config.h`; saved `wifi_ap/cfg` overrides them. The default gateway is `192.168.4.1/24`, channel 6. Display the actual netif address, not an assumed camera IP.

| Requirement | Behavior |
| --- | --- |
| R1 defaults | Restore the configured factory SSID/password/channel; current defaults are `easycamctrl`, `00000000`, channel 6. |
| R2 editing | Only startup maintenance Web may change SSID/password/channel or generate a password. SSID 1–32 bytes without control characters; password 8–63 printable ASCII; channel 1–13 and within country capability. Reject atomically before save. |
| R3 persistence | Preserve across power cycle and ordinary app update; missing/bad records use defaults with diagnostics. Do not erase the entire NVS automatically. |
| R4 display | SSID, password visibility and dynamic LCD IP on connection page; hidden password is `********`; RSSI tracks selected camera MAC, not the first phone. |
| R5 reset | Confirm AP-only or full reset on Web; preserve Camera identity for AP-only and ATOM bindings for both. Successful operations reboot. |

Normal UART `wifi show` and the controller Wi-Fi menu are readonly. `wifi show password` explicitly reveals the password. Factory-default labeling depends on the password value, not SSID or visibility. A generated 12-character password is not saved until the user applies it.

Acceptance uses the current Web entrance, not retired UART/controller editors: valid save/reboot/rejoin, invalid 7-byte password/33-byte SSID/channel14 rejection, cold persistence, corrupt record recovery, hidden password/dynamic IP, phone+camera RSSI selection and both reset scopes. Country configuration alone does not establish camera support on channels12/13. See [design](../design/wifi-ap-design.md) and [current validation](../development/current-status.md).
