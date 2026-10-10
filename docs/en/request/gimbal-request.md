# Gimbal requirements

**English** · [简体中文](../../request/gimbal-request.md) · [日本語](../../ja/request/gimbal-request.md)

ATOM directly controls **RS 3 Mini** from physical DS4 left X/Y and L3. LCD receives connection/fault state only. Camera tracking, paths and time-lapse are outside scope. Official activation and concurrent Classic+BLE operation are prerequisites.

| Requirement | Acceptance obligation / current boundary |
| --- | --- |
| R1 connection | Reconnect the saved Mini automatically. With no target, select only one matching Mini; multiple candidates must not be chosen randomly. Ready requires control/notification initialization, not BLE opening alone. |
| R2 manual motion | X→Pan, Y→Tilt, approximately 10% deadzone, quadratic response, latest target at about 5Hz. Released stick must stop within 100ms: timing still unverified. |
| R3 center | L3 must center smoothly; ignore accidental stick movement while L3 is held, cancel after release/push. Current native center works; arbitrary recorded zero and physical cancel acceptance are incomplete. |
| R4 safety | Stop on DS4 loss, report age ≥200ms, disable and reconnect-before-arm. LCD/I²C loss must not stop independent DS4 control. Repeated writes fail closed and report a fault. |
| R5 local settings | Persist target, enable, inversion, centered offsets and axis speeds. Board-button menu, arbitrary zero and soft limits remain targets. Without trustworthy angles, limits cannot claim protection. |

Factory axis spans are 120/120, range 20–400 in protocol units, not degrees/s. The tested device retains Pan120/Tilt240 after the user's slow-Tilt feedback. Pairing is local application binding; `gimbal pair` replaces only the gimbal target, not DS4 or an asserted SMP bond.

Acceptance: unique first selection, saved cold-start reconnect, four directions/deadzone/speed, ≤100ms stop, L3/cancel/timeout, input stale/offline, I²C unplug, gimbal power cycle, limits and NVS persistence, Matrix/LCD faults, and **30 minutes** without uncontrolled movement/disconnect. User confirmed basic stick/L3 and successful gimbal power-cycle recovery; approximately 14-minute concurrency is partial. Real ATOM cold startup and full fault/timing tests remain.

See [current protocol](../design/rs3-mini-protocol.md), [design](../design/gimbal-design.md) and [validation](../development/current-status.md). Requirements are not an implementation-complete claim.
