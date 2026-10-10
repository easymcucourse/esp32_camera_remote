# Controller requirements

**English** · [简体中文](../../request/gamepad-request.md) · [日本語](../../ja/request/gamepad-request.md)

The default development controller is Classic Bluetooth DualShock 4. Ultimate 2 BLE has a restricted descriptor/report parser; physical button mapping remains to be accepted. Settings select DS or Xbox-compatible input through startup Web. Generic BLE controllers are not automatically compatible.

Required camera/UI mapping in LIVE and SETTINGS:

| Input | Required/current action |
| --- | --- |
| Options/Start | LIVE/SETTINGS switch. |
| Square/X, Triangle/Y | Next focus mode, next exposure mode; one action per press, readback confirmation. X cancels shoulder control. |
| L1/R1 | Wide/Tele for the declared power-zoom lens; conditional Near(+1)/Far(−1) only for a confirmed non-power-zoom lens in MF. |
| RT | S1 half press and S2 full press; S2 release precedes S1 release. |
| LT | Half press does nothing; full-press edge toggles a known recording target, never guesses unknown/pending status. |
| D-pad, A/B | Menu navigation/step, confirm/back; 400ms initial hold then 150ms repeat, no catch-up backlog. |
| Left stick/L3 | ATOM-local Pan/Tilt and native Mini center. |
| Touchpad click | Recognized by the kernel but ignored by the current service; display preferences are Web-only. |

RT enters half at 77 and full at 230; releases half below 51 and full below 204. LT has only a full stage, 230/204. Both triggers must be released after connection/source/session changes before arming. Shoulder conflict stops and locks until both are released. Lens status must not be inferred from unavailable zoom.

Gap, stale epoch/report, offline, source change and stop must release camera actions and reject old commands; held reconnect input establishes a baseline without new press edges. Recording/settings are confirmed by actual camera state, not queue acceptance. Select/right-stick/R3 focus-point and magnification targets remain unimplemented. Current touchpad behavior replaces the former cycling requirement.

Acceptance covers every button and threshold, simultaneous/held/gap input, reconnect without replay, physical camera effects, Ultimate mapping, end-to-end P95 latency and at least 30 minutes. Mini stop/center/limits have separate [requirements](gimbal-request.md). See [controls](../user-guide/controller.md), [design](../design/gamepad-design.md) and [current status](../development/current-status.md).
