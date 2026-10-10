# Camera menu

**English** · [简体中文](../../design/camera-menu-design.md) · [日本語](../../ja/design/camera-menu-design.md)

UI owns cursor/navigation; Input requests a semantic menu route, then submits property direction to Camera. Camera's pure `camera_menu`/`setting_control` kernels own seven basic parameters and nine extra targets. Moving the cursor cannot redirect a previously accumulated adjustment.

SETTINGS cursor order: Focus, Shutter, Aperture, ISO, EV, WB, Meter, MORE, Wi-Fi. Up/down cycle; left/right cycle complete enumerations. Hold400ms then repeat150ms; multiple directions, gap, mode/session changes require release. MORE: ASPECT/DRIVE/EFFECT/DRO/AF AREA/WL FLASH/WB TEMP/WB AB RAW/WB GM RAW, plus EXIT. Wi-Fi is information only; maintenance is not a menu item.

Before each write, validate the whole descriptor dataset, scalar type, writable capability, actual value and complete first enumeration. Missing/duplicate/truncated/oversized or unknown-current descriptors do not authorize writes; the second enumeration cannot bypass readonly state. EV is sorted by signedINT16 value, right increases/left decreases, while original16-bit wire values remain intact. Other enumerations retain their ordering.

Keep actual, final desired and sent target distinct. Rapid steps merge around desired; wait for readback of sent before sending another target. Timeout10s, rejection/capability/safety change clears work. Mode confirmation blocks other parameter writes; pending refresh isabout500ms. Focus menu and X share one target state.

Shutter/aperture use absolute enumerations when available. Otherwise a valid writable current value permits int8 ControlDeviceB single steps; wait for directional readback, merge at most64 net steps, never show a guessed absolute target. Invalid shutterFFFFFFFF/apertureFFFE forbids writes. Wire tests do not prove ZV-E10 step acceptance.

Safety/release/record/shutter commands precede property writes and frame reads. Each new snapshot permits at most one menu write, checking safety generation immediately beforehand. Display actual plus TO/PENDING; selected terminal feedback persists3s. See [input](gamepad-design.md), [Sony](sony-ptpip-design.md) and [tests](../development/testing.md).
