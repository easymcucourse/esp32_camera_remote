# Sony PTP/IP design

**English** · [简体中文](../../design/sony-ptpip-design.md) · [日本語](../../ja/design/sony-ptpip-design.md)

Camera owns generic discovery/session/stream/control. Sony backend owns vendor parsing/encoding and embeds **one** PTP client; that client owns standard wire/session/transaction logic and typed Wi-Fi channels. UI consumes readonly JPEG/property leases. Historical fd APIs are test-only, not production paths.

Discovery probes actual DHCP candidates atTCP15740,800ms per candidate. Before binding select only one; afterward MAC and returned GUID must match. GUID-only migration keeps identity and saves peer after complete initialization. `sony_remote/guid/peer` primitives are owned by common_runtime, normal persistence runs on an internal-stack Camera worker.

Initialization: command InitRequest/Ack → event InitRequest using returned connection number/Ack → OpenSession0x1002(id1) → Sony0x9201(1,0,0),(2,0,0) → GetDeviceInfo0x1001(0) →0x9202(300) →0x9201(3,0,0) →0x9202(300) → properties0x9209(0) → GetObjectInfo0x1008(FFFFC002) → repeated GetObject0x1009(FFFFC002). Command/event share one transaction/session owner.

First init waits up to120s, saved pairing10s, ordinary/event transactions5s. Absolute deadlines survive nested reads; Wi-Fi nonblocking select checks cancellation within 100ms steps. PTP sees typed tokens/generation/leases, never socket fd. InitFail/GUID mismatch pauses retries; network failures back off1,2,4,8,16,30s. A displayed first frame resets failures.

Complete0x200F returns the slot and waits100ms before same-session retry; over 50 continuous refusals reconnect. Missing EndData, wrong transaction, oversize and network failure close rather than reuse incomplete protocol state. `s` clears controls and drains frames/backend before task exit; UART ACK only admits the stop. Whole-system timing still needs hardware tests.

Validate the entire0x9209 descriptor dataset before publishing. Property events0xC203 refresh all, with 5s fallback/500ms while pending. Generic targets/readback and actions are separate from Sony wire encoding. Unknown enums/WB microadjustments remain raw; recorded WB temperature need not be active. Current POWER_ZOOM is a user declaration. S1/S2 and recording acceptance do not prove physical effects. See [menu](camera-menu-design.md), [messages](module-message-contracts.md) and [tests](../development/testing.md). The [Chinese design](../../design/sony-ptpip-design.md) retains the historical14-section proposal separately from its current implementation section.
