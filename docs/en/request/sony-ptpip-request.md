# Sony PTP/IP requirements

**English** · [简体中文](../../request/sony-ptpip-request.md) · [日本語](../../ja/request/sony-ptpip-request.md)

Target Sony ZV-E10 PC Remote over TCP15740. Required scope is discovery, persistent pairing, reconnect, live view, properties, setting targets, focus/shutter/recording and zoom; AP/UI/button assignment have separate requirements.

| Group | Required behavior |
| --- | --- |
| R1 discovery | DHCP address and actual MAC, unique reachable candidate before pairing, saved target afterward; phones do not redirect the camera session. |
| R2 pairing | Confirm ESP32-Camera-Remote once, retain GUID/peer across ordinary updates, handle rejection explicitly and allow Web reset. Saved first-frame target ≤10s remains a hardware metric. |
| R3 live view | First decode opens centered 1024×576; old minimum targets full≥3.5FPS/SETTINGS≥2.4FPS. The later memory/FPS plan adds stronger full/read gates. Thirty-minute stable memory and damaged-frame recovery remain required. |
| R4 stop | Cancel handshake/read at any phase, drain before closing, retain last image, resume same identity; whole-system ≤1s remains to be measured. |
| R5 recovery | 1–30s bounded backoff; authorization refusal waits for the user; cancel old actions so reconnection never replays them. |
| R6 properties | Nine basic/nine extra values, event-triggered refresh with 5s fallback; ≤1s event response is a target. Unknown stays raw/`--`; parse failure preserves prior state. |
| R7 control | Merge final targets, show PENDING/APPLIED/REJECTED/TIMEOUT, confirm readback, release S2 before S1, prioritize safety over frame reads. Unknown recording state is not guessed. |
| R8 robustness | Reject malformed/truncated/oversized/wrong-transaction data without unsafe access; log stage/opcode/transaction/error. |

The current power-zoom lens is a user declaration, not automatic identification. Conditional non-power-zoom MF fallback stays inactive. UART `u` is retired; camera identity reset is startup Web full reset. Parameter acceptance is distinct from the physical effect.

Acceptance requires first/repeated pairing, rejected authorization, cancellation in each phase, loss/reconnect with no replay, multiple clients, camera-body changes, ten quick target changes, each action/readback and a 30-minute measured run. Historical camera tests validate their own image. See [design](../design/sony-ptpip-design.md), [live-view plan](../design/liveview-memory-fps-plan.md) and [status](../development/current-status.md).
