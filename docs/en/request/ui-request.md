# UI requirements

**English** · [简体中文](../../request/ui-request.md) · [日本語](../../ja/request/ui-request.md)

The LCD is 1024×600 RGB565 with no initialized touch control. Use Inter for English, Source Han Sans for Chinese and JetBrains Mono for parameter numbers. Device text remains English; document translations do not change firmware UI language.

Connection page: title **easymcucourse camera console**, separate SSID/password/IP lines, pairing/connection stages, Expansion/controller/camera/gimbal state and default-password indication. LIVE is 1024×576 centered; SETTINGS shows a 768×432 preview plus menus. Mode transitions follow first displayed frame, Options/Start, UART `S` and disconnection.

Right-column values include WIFI, FPS, CAM, FW, BATTERY, selected controller battery (DS4/XBOX), MODE and FOCUS. Known batteries use green >50%, yellow21–50%, red≤20%, unknown gray `--`. Menu values are actual readback; writable cursor/target/PENDING and transient rejection/timeout feedback do not fabricate success. Signed EV right increases/left decreases, with cyclic complete enumerations.

Full/compact/hidden LIVE information levels are **saved through startup Web and loaded after restart**. Touchpad clicks do not change them in the current service. SETTINGS remains readable. REC state/border follows actual recording readback; SIM and GIMBAL FAULT remain visible where applicable, even when normal information is hidden. The new LCD fault display is built/host-tested but unflashed.

MORE contains ASPECT, DRIVE, EFFECT, DRO, AF AREA, WL FLASH, WB TEMP, WB AB RAW, WB GM RAW; A enters/confirms, B/EXIT returns. Wi-Fi is readonly. Maintenance has a fixed exclusive screen and only the startup HTTP entrance; there is no SETTINGS maintenance entry.

Focus red/green position boxes, Select/right-stick/R3 control, magnification and unified NO ZOOM feedback remain planned. Acceptance requires visible state transitions, each information level after Web save/reboot, camera-confirmed REC/menu feedback, raw/unknown values, all fault overlays and real pixel timing. Historical FPS/layout observations are dated evidence. See [design](../design/ui-design.md), [controller guide](../user-guide/controller.md) and [status](../development/current-status.md).
