# Input ownership and controls

**English** · [简体中文](../../design/gamepad-design.md) · [日本語](../../ja/design/gamepad-design.md)

Physical ATOM and Debug SIM each own a protocol/provider and submit copied normalized reports to a fixed16-entry registry ring. Input owner alone selects source, validates epochs/IDs and runs `gamepad_input`; it sends typed Camera/UI requests rather than calling their private APIs.

Provider API version 1 uses opaque nonzero handles, nonzero source_epoch/report_id, copied buttons/axes/triggers/battery/link/SIM/gap/event fields. Reject duplicate ID/old epoch; backward IDs quarantine that epoch until advancement. Full report ring drops that source's backlog and schedules priority disconnect. Unregister/re-register invalidates the old handle. Source switch/gap/reconnect establishes held baseline and retries complete release before allowing new presses.

Input task4096 internal bytes/priority 4 runs50ms. Most queries/actions have100ms deadlines; menu requests allow500ms for the UI JPEG consumer. Menu reply is a semantic route/property; Camera adjustment is a separate request. Source acceptance does not mean physical release is finished. Core stops Input before providers and Camera; an active timed-out owner is retained for retry/reset.

RT hysteresis77/51 half,230/204 full; LT only230/204 full. Arm only after both are released. S1 press precedes S2, S2 release precedes S1. LT never changes RT focus. One-shot buttons use events; snapshots sustain analog/repeat/release only. D-pad/conditional MF repeat400/150ms without catch-up. Shoulder conflict stops and locks to release; X cancels. Targets/readback and safety generations prevent old commands executing.

The current lens is declared POWER_ZOOM: L1Wide/R1Tele. Non-power-zoom+MF fallback requires explicit knowledge, not ZoomEnableStatus. Focus point/magnify remain planned. Touchpad action is recognized by the pure kernel but deliberately discarded by `input_service`; Web owns display preferences.

Ultimate2 accepts only the captured113-byte HID descriptor and unique notify input33-byte report. Unsupported layouts cannot control the camera. DS4 wins aggregation; BLE becomes stale after 1s. Gimbal separately consumes fresh physical Classic DS4, never this aggregated/SIM report. See [mapping](../user-guide/controller.md), [menu](camera-menu-design.md), [I²C](i2c-protocol-design.md) and [acceptance](../development/module-split-checklist.md).
