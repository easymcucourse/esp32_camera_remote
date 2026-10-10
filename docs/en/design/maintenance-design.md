# Maintenance and OTA design

**English** · [简体中文](../../design/maintenance-design.md) · [日本語](../../ja/design/maintenance-design.md)

Core modes STARTUP/NORMAL/ACTIVATING/MAINTENANCE/RESTART are monotonic for a boot. Startup HTTP port80 claims exclusive maintenance; normal transition closes it. Trigger→activating→active→closed cannot reopen. Web has **no authentication** and uses the isolated Wi-Fi object plus injected Core storage/restart callbacks, not normal Console/Camera/UI APIs.

Core closes ordinary admission, safely releases Input, drains Camera/JPEG and normal owners, freezes the model and publishes fixed MAINTENANCE, then enables business routes. Failure requests restart and retains unfinished owners; no normal-mode restoration. AP remains available.

HTTP task priority 3, internal6144-byte stack, three sockets,15 handlers,10s receive/send timeouts. Stop closes admission then queues SDK session-close and synchronously joins HTTP; the project3000ms argument is not a strict SDK join bound.

| Route | Purpose |
| --- | --- |
| GET /, /api/info | Page and diagnostics. |
| GET/POST /api/settings, /api/controller | Current/controller/info preferences; compatibility route shares implementation. |
| GET/POST /api/wifi; GET /api/wifi/random_password | Config/token result, staged update, generate-only password. |
| POST /api/factory | Strict scope wifi/all and confirm:true. |
| POST /api/maint/exit, /api/reboot | Reserve reboot; no resume. |
| POST /api/ota/check, /api/ota; GET /api/ota/status | Prefix check, full upload, progress. |

First requests, including404/405, pass the trigger gate before business execution. Settings save uses shared storage then1500ms reboot even if the final reply is lost. Wi-Fi reserves restart→prepare→send ACK→commit; failed ACK cancels stage. Core tracks committed token independently of client polling. Factory freezes config; all clears LCD Camera/UI plus AP, not ATOM. Only Wi-Fi rollback is attempted on partial failure; namespaces are not one atomic transaction.

Partition layout: NVS0x9000/0x6000, otadata0xF000/0x2000, PHY0x11000, apps0x20000 and 0x620000 each6MiB, data0xC20000. Initial migration uses USB; routine app updates preserve metadata/identity. OTA checks288-byte prefix with decimal X-Image-Size and application/octet-stream, rechecks full image, writes inactive slot using4096-byte PSRAM chunks on internal HTTP stack, esp_ota_end verifies before selecting boot. Failure aborts/cancels; success reboots even after lost reply. Healthy pending image is confirmed60s after readiness; unhealthy/pending reset rolls back.

Source/host/build verification is separate from real AP isolation, blocked stop, flash/cache-off, power loss, fixed pixels and browser acceptance. See [ownership](module-resource-ownership.md), [requirements](../request/maintenance-request.md) and [status](../development/current-status.md).
