# Gimbal control design

**English** · [简体中文](../../design/gimbal-design.md) · [日本語](../../ja/design/gimbal-design.md)

Production modules are `gimbal_link`, pure `gimbal_control`, `gimbal_proto_rs3`, bounded `gimbal_tx`, and shared `ble_clients`. The old generic ops/config schema and 50ms task proposal in the Chinese design are **future/historical interfaces**, not the current structure. Link's20ms worker owns events/GATT/NVS; shared dispatcher owns GAP/GATTC callback registration and serialized BLE scans.

Control reads only physical Classic DS4 with the same esp_timer clock, connected epoch and report age<200ms. SIM blocks hardware control. Reconnect/epoch change sends neutral and disarms; fresh centered stick with released L3 rearms. I²C/LCD state is irrelevant to motion.

Subtract calibrated offsets, clamp to±127, deadzone13, then square the remaining normalized magnitude and multiply each axis span. Factory spans 120/120, range 20–400; default Tilt inversion1. These are protocol offsets, not angles/s. Local tested tuning120/240 is persisted. Manual cadence200ms retains the latest input; neutral bypasses the interval.

L3 press sends native center, held L3 suppresses accidental stick motion. After release, push or5s timeout requests neutral then manual. When moving, neutral is submitted before center and the press edge is retained until TX can proceed. Actual cancel/stop timing still needs validation.

TX permits one ordinary pending write and one reserved neutral; it does not replay queued historical targets. No completion within 500ms closes/faults the link. Consecutive failure policy, initialized notifications, valid RX and silence timeout are documented in [Mini protocol](rs3-mini-protocol.md). Ready publishes state3 and feature/fault bits over I²C; LCD does not configure/control motion.

NVS v1 cfg retains target/enable/Pan/inversion/offsets; `tilt_span` is a separateu16 key, absent inherits old shared span. Current calibration uses a fresh centered sample within±32, not the proposed one-second averaging menu. Arbitrary zero, trustworthy angle closed loop, soft limits and board-button menu are not implemented. Native center is the gimbal's own reference. See [requirements](../request/gimbal-request.md) and [validation](../development/current-status.md).
