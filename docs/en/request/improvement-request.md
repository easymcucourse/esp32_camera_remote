# Prioritized improvements

**English** · [简体中文](../../request/improvement-request.md) · [日本語](../../ja/request/improvement-request.md)

This is the current priority map. The [Chinese priority map](../../request/improvement-request.md) links the archived earlier checklist; historical checkboxes refer to code/host work, not complete hardware acceptance. Read [current status](../development/current-status.md) before scheduling changes.

| Priority | Remaining work |
| --- | --- |
| P0 safety/stability | Real cold startup, all-owner drain, display failure recovery, input release and NVS failure paths; no forced deletion of active resource owners. |
| P1 camera/network | Complete discovery/reconnect/authorization matrix, real parameter/record/shutter effects, raw enum interpretation and sustained stream recovery. |
| P1 gimbal | Measure stop≤100ms and cancel behavior, ATOM cold start/abnormal disconnect, field-valid battery/angles and 30-minute concurrency. Arbitrary zero/limits require reliable pose first. |
| P2 performance | Complete staged PSRAM/TCP/FPS plan, pure full/SETTINGS comparisons, heap/stack gates, stable-profile and restart-reconnect checks. Do not add a third slot before prior gates pass. |
| P2 maintenance/UI | Real Web exclusivity/AP isolation, save/reset/OTA/power-loss, every info/layout/fault visual and physical Ultimate mapping. |
| P3 portability/documentation | Explicit ports/SDK paths, general capture extraction, redacted fixture provenance and maintained English/Chinese/Japanese navigation. |

Already present: structured Sony descriptors, DHCP selection, cancelable typed TCP, target coalescing, I²C v2/gap, module ownership/lease gates, Debug/Release CI matrix, dual-slot OTA, Stable profile, heap analyzer and basic Mini control. Do not reopen these as absent implementations. Their complete hardware/performance acceptance remains separate.

Touchscreen fallback, configurable mappings, focus-point/magnification and broad BLE compatibility remain future scope. Remote CI/branch-protection enforcement has not been independently confirmed. Existing frame/control fixtures and historical captures must be retained when consolidating source. See [testing](../development/testing.md) and [acceptance](../development/module-split-checklist.md).
