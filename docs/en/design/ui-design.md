# UI design

**English** · [简体中文](../../design/ui-design.md) · [日本語](../../ja/design/ui-design.md)

UI owns model, navigation, fonts, renderer and JPEG decode. It obtains one writable back-canvas lease from display_surface; board owns RGB/front/back buffers. Camera/input/network publish semantic values/readonly leases rather than drawing or exposing Sony objects. Short model locks never enclose rendering or network I/O.

Connection screen shows `easymcucourse camera console`, actual AP SSID/password/IP/default label, pairing stages and device links. LIVE1024×576 at(0,12); SETTINGS768×432 plus right menu and lower extras. Main values WIFI/FPS/CAM/FW/BATTERY/DS4-or-XBOX/MODE/FOCUS use actual snapshots. Missing values `--`, unknown enums hexadecimal. Battery thresholds >50 green/21–50 yellow/≤20 red.

Information level/full/compact/hidden and controller type load from versioned ui_prefs at boot. Current ordinary APIs are GET-only; touchpad info-next is ignored in Input service. Web save/reboot is the writer. REC red border reflects camera recording readback; SIM and gimbal fault survive hidden information. Focus-position boxes/magnify remain planned.

UI endpoint serializes JPEG decode and menus. Camera frame message carries readonly slot lease/token/read duration; UI result uses original frame generation, not UI lifetime. Success, corrupt frame, display failure, stop and late/full result paths release leases and report metadata. Decoder/work area are boot allocated, pixels borrowed from canvas; no third full-screen copy. Recovered display reuses buffers.

Debug benchmark shares this renderer, reserves/drains Camera via typed messages and displays20 synthetic frames; its FPS excludes camera/TCP. Fixed maintenance closes normal admission, cancels ordinary tokens, drains users, clears/freezes model and shows MAINTENANCE. Errors must not reopen normal drawing. Real RGB/SMP/fonts and newest gimbal-fault visual still require physical proof. See [menus](camera-menu-design.md), [resources](module-resource-ownership.md) and [requirements](../request/ui-request.md).
