# I²C v2 wire protocol

**English** · [简体中文](../../design/i2c-protocol-design.md) · [日本語](../../ja/design/i2c-protocol-design.md)

LCD masterGPIO8/9, ATOM slaveGPIO26/32, address 0x42,100kHz,3.3V/common ground. V2 is incompatible with V1: update both ends. The canonical codec is `common/atom_protocol.*`. Gimbal motion stays local to ATOM; left stick is absent and L3 is masked from snapshots/events. Right-stick transport does not mean focus-point control is implemented.

All multi-byte values are little-endian. CRC-8/SMBUS polynomial 0x07, init 0, no reflection/xor; `123456789`→F4. Request is 9 bytes: A5/version 2/seq/cmd/param[4]/CRC. Response is 5A/version 2/seq/cmd/status/len/payload[N]/CRC, total 7+N. Error status hasN=0; locate CRC using len and ignore extra read padding. Status0..5: OK/BAD_CRC/BAD_VERSION/UNKNOWN_CMD/BAD_PARAM/NOT_READY.

HELLO0x01 returns12-byte payload (19 total). Params are min2,max2,input0=DS4 or1=BLE,reserved0. Payload offsets:0–3 boot_id(nonzero),4–5 firmware version,6 features,7 capacity128,8–10 local_mask(L3=0x000002),11 reserved. Features bits0Classic,1BLE,2gimbal,3gap,4input selection.

POLL0x10 request param isu32 ack_id. Success payload is 28 bytes (35 total):

| Offset | Type / meaning |
| --- | --- |
| 0–3 | u32 boot_id. |
| 4/5/6/7/8 | u8 controller link / gimbal link / local-button bit / faults / battery0..10 or255. |
| 9–10 | u16 board-button press count, wraps65535. |
| 11–13 | u24 current buttons with L3 removed. |
| 14/15/16/17 | i8 RX/RY, u8 LT/RT. |
| 18 | valid bit0, gap bit1. |
| 19–22 | u32 event ID or0 when invalid. |
| 23–25 | u24 event buttons, L3 removed. |
| 26 | Remaining unconfirmed events, saturated255. |
| 27 | SIM bit0 and source-generation bits1–7. |

Links0off/1search/2connecting/3ready. Fault bits0overflow,1I²C,2Bluetooth,3gimbal. Button bits0Share,1maskedL3,2R3,3Options,4Up,5Right,6Down,7Left,8L2,9R2,10L1,11R1,12Triangle,13Circle,14Cross,15Square,16PS,17Touch; remaining bits zero. Triggers use analog values, not digital bits.

Master writes, waits15ms, then reads separately (no repeated start); poll50ms, transfer timeout100ms, three failures offline, probe1s, mismatch retry5s. Retry preserves seq/ack. Boot/source changes rebaseline/release without press edges. ACK removes only a matching head; duplicates are idempotent. Overflow marks gap until its event is acknowledged; gap synchronizes held state without actions.

Slave resynchronizes atA5, discards partial frames after 20ms, and refreshes heartbeat only for legal requests. The core0 reply adapter replaces SDK5.5.1 private ringbuffer/FIFO; upgrade requires re-audit. Host simulation does not prove FIFO/ISR timing or30-minute physical stability. See [full Chinese byte ledger](../../design/i2c-protocol-design.md), [input](gamepad-design.md) and [Mini](rs3-mini-protocol.md).
