# Matrix LED requirements

**English** · [简体中文](../../request/matrix-led-request.md) · [日本語](../../ja/request/matrix-led-request.md)

ATOM owns its 5×5 LED display; LCD never sends colors. Logical origin is top-left `(0,0)`. Low brightness, fixed locations and distinct fault shapes must remain readable without serial logs.

Startup has five white columns: LED, peripherals, storage, Bluetooth stack and HID Host. Completed columns are steady; current column flashes 250ms on/off. Hold all columns 300ms, then clear into normal mode. Synchronous initialization failure logs/resets; the last frame indicates the stage. LED failure itself cannot display a reliable pattern.

| Normal location | Meaning |
| --- | --- |
| Rows0/1/2 | Classic battery blue, BLE battery cyan, gimbal battery purple. |
| Row3 | LCD startup-wait animation, one yellow point every 125ms, up to10s from boot. |
| `(0,4)` | LCD yellow waiting / green online / red lost. |
| `(1,4)` | Reserved/off. |
| `(2,4)/(3,4)/(4,4)` | Classic blue / BLE cyan / gimbal purple. |

Battery bars round up one pixel per20%, max five; known≤20% (including zero) flashes one red minimum, 250ms on/off. Unknown/offline is dark. Wireless disabled/off is dark, searching pulses125ms each2s, connecting flashes500ms period at 50%, ready stays on. Ready means valid input or actual control/notification initialization, not socket opening.

Fault priority: Bluetooth purple B → I²C orange exclamation → overflow yellow rows0/2/4 → normal. Faults cover all25 pixels. I²C threshold is three invalid requests within 2s, cleared by three legal requests; heartbeat loss is a link indication, not a protocol fault. Overflow lasts5s and restarts its timer on another overflow. LED refresh failure logs/retries without stopping Bluetooth/I²C.

Current BLE/controller/gimbal sources are implemented; battery validity still needs field confirmation. Acceptance includes physical corner mapping, boot failure stages, simultaneous links, stale/unknown batteries, all fault priorities/recovery, LCD heartbeat loss around1500ms and at least30-minute RMT/Bluetooth/I²C stability. See [rendering design](../design/matrix-led-design.md) and [current status](../development/current-status.md).
