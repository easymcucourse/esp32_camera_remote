# Maintenance requirements

**English** · [简体中文](../../request/maintenance-request.md) · [日本語](../../ja/request/maintenance-request.md)

Current requirements replace the earlier reversible mode and PIN/login design. The LCD provides **startup-only, unauthenticated maintenance** on its AP, normally port 80 at `192.168.4.1`. Any joined AP client can use all maintenance operations. ATOM OTA and Internet update checking are excluded.

1. STARTUP exposes an HTTP trigger. The first request claims exclusive mode; NORMAL closes the trigger and cannot reenter maintenance until reboot.
2. Core must release input, stop/drain Camera/JPEG and normal services, retire ordinary tokens and freeze the UI to **MAINTENANCE** before full routes run. A failed switch restarts; it must not resume partially stopped services.
3. Web shows version/build/partition/IDF, uptime and diagnostics; edits AP and controller/information preferences, and offers confirmed AP-only/full reset, LCD OTA, exit/reboot. No PIN, token, cookie or login flow is required.
4. AP validation follows [network requirements](wifi-ap-request.md). Save commits only after the staged acknowledgment succeeds; Core follows the committed result and schedules reboot. UI preferences are saved independently and reloaded after reboot.
5. Factory reset requires explicit scope and confirmation. AP-only retains camera identity; full reset also clears LCD camera/UI preferences, preserves ATOM binding and unrelated storage. Cross-namespace failures may be partial.
6. OTA accepts a validated LCD application image within the 5MiB gate, writes the inactive 6MiB slot, verifies before selecting boot, and keeps rollback. Invalid, oversized, interrupted or wrong-chip images must not become the boot image. Healthy pending images are confirmed after 60 seconds. Exit and successful writes/OTA reboot.

Acceptance includes first-request races, NORMAL refusal, AP-interface isolation, real browser behavior, complete draining/fixed screen, save/ACK-loss/reset failure, upload disconnect/power loss, rollback and preserved identities. Host/build evidence exists; the new split image's full hardware acceptance remains pending. Historical device-loop OTA success belongs to its original image. See [design](../design/maintenance-design.md) and [current status](../development/current-status.md).
