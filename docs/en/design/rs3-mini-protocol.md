# RS 3 Mini protocol and limits

**English** · [简体中文](../../design/rs3-mini-protocol.md) · [日本語](../../ja/design/rs3-mini-protocol.md)

This is an independently written, **unofficial** adapter. References and MIT notices are in [third_party/rs3-protocol](../../../third_party/rs3-protocol/README.md): the pinned Mini bridge informs the Mini wire format; the RS3 project is cross-evidence, not Mini acceptance. Official activation uses Ronin App; do not infer activation merely from a prior motion response.

Discover FFF0, FFF5 Write Without Response, FFF4 Notify and 2902 CCCD dynamically. Negotiate MTU185; reject<25 for22-byte stick frames. The tested FFF4 is notify-only: write0100 to CCCD, **do not require the characteristic to be writable**. An optional characteristic initialization is used only when declared writable and remains untested. Wait300ms, submit neutral/poll and require valid DUML RX before ready.

DUML:55 /10-bit length+version /header CRC8 /sender /receiver /seqLE16 /flags /set /id /payload /CRC16LE. CRC8 init 77 reflected 8C; CRC16 init 3692 reflected 8408. Bounded256-byte receiver handles fragments, concatenation and resynchronization after corrupt length/version/CRCs.

| Action | Receiver / flags / set / id | Payload |
| --- | --- | --- |
| Stick/neutral | 04/40/04/01 | Tilt,Roll,Pan LE16 centered 1024; tail000002. |
| Native center | 04/40/04/4c | fe01. |
| Current heartbeat1Hz | 04/00/04/12 | 1051010000000c00005000f1036624c01d00001c. |
| Battery notification | senderE5→02,0d/02 | exactly 21 payload bytes; final0–100, unknown after 15s. |

Heartbeat endpoint4 follows the Mini reference **builder** and worked in this device test; that reference's original capture usesE5. Earlier E5 could return telemetry without physical motion. Do not conflate these sources or claim all firmware compatibility.

No target: match a unique `DJI RS3 MINI-`/`DJI RS 3 Mini` name. Saved target: exact address even without name; never auto-replace. Multiple candidates wait. Ready saves the application target; `gimbal pair` clears only that binding, not DS4 or an asserted SMP bond. Shared BLE dispatcher serializes scans and filters callbacks by app/interface/peer.

Worker20ms, manual200ms, input freshness<200ms, neutral-first rearm, native center and TX1+neutral reservation follow [control design](gimbal-design.md). Five repeated failures, pending completion500ms, initialized RX silence5s or event overflow close/fault. Actual physical stop≤100ms remains unverified.

Read-only pose accepts only verified04/66 header1, correct endpoints and complete unique two-byte TLV tags22Tilt/23Roll/24Pan. Unit/zero are uncalibrated. Before activation31-byte payloads once passed; after activation19-byte packets lacked these tags, so **pose valid remains false**. There is no angle-based limit/zero protection.

The user confirmed stick, L3, Pan120/Tilt240 and gimbal power-cycle recovery. Approximately14-minute concurrency is partial, not30 minutes. Native-center ACK is not measured cancel behavior. New LCD fault UI is host/build verified but unflashed. See [status](../development/current-status.md) and [original detailed ledger](../../design/rs3-mini-protocol.md).
