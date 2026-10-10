# Matrix rendering design

**English** · [简体中文](../../design/matrix-led-design.md) · [日本語](../../ja/design/matrix-led-design.md)

`matrix_model` is pure C: time and copied link/battery/boot/fault state produce25 logical pixels. `matrix_status` owns GPIO27/RMT rendering and bounded retry; Bluetooth/I²C owners publish state without drawing. GPIO39 is active-low local button. Logical index isy*5+x, origin top-left; physical corner mapping needs visual acceptance.

Boot five columns and normal battery/link/fault layouts follow [requirements](../request/matrix-led-request.md). Top three rows show Classic/BLE/gimbal battery, fourth LCD wait, bottom fixed LCD/reserved/Classic/BLE/gimbal. Known-zero is low battery; unknown255/offline is dark. Gimbal battery from strict DUML expires15s; stale data must not be fabricated.

HID asynchronous initialization has a3s completion deadline; boot final steady phase300ms. Normal LCD wait is 10s from boot, heartbeat timeout1500ms. Wireless search125ms/2s, connecting500ms50%, connected steady. Bad-request window records real timestamps: at least3 within 2s sets I²C error;3 valid requests clear. Overflow display5s retriggers. Bluetooth B outranks I²C exclamation, then overflow bars, then normal.

Shared `ble_clients` alone registers BLE callbacks/scan ownership and dispatches by app/interface/peer. Classic DS4 keeps its HID callbacks. BTDM/BLE/GATTC and appropriate connection/cache/notify capacity are required; existing BR/EDR-only sdkconfig must be regenerated. BLE controller state/standard Battery Service and Mini protocol are implemented, not placeholders. Unrecognized input reports may still provide battery but cannot control the camera.

Debug led test cycles corners; forced faults change display only, not LCD link data. Forced overlays and calibration are RAM-only, Release excludes them. RMT refresh failure logs and attempts recovery without resetting unrelated owners. Pure model/FIFO adapter tests and builds cannot prove physical orientation, RMT/SMP or30-minute radio/display stability. See [hardware](hardware-design.md), [I²C](i2c-protocol-design.md) and [status](../development/current-status.md).
