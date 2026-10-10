# I²C v2通信プロトコル

[English](../../en/design/i2c-protocol-design.md) · [简体中文](../../design/i2c-protocol-design.md) · **日本語**

LCD masterGPIO8/9、ATOM slave26/32、0x42、100kHz、3.3V/共通GND。v1と非互換で両端更新します。正本codecは `common/atom_protocol.*`。gimbal motionはATOM内、左stickは送らずL3をsnapshot/eventから除外します。右stick転送はfocus点実装を意味しません。

多byteはLE。CRC-8/SMBUS polynomial0x07、init0、反射/xorなし、`123456789`→F4。request9byte：A5/version2/seq/cmd/param[4]/CRC。reply：5A/version2/seq/cmd/status/len/payload[N]/CRC、7+N。errorはN=0、lenでCRC位置を求め余分paddingを無視。status0..5はOK/BAD_CRC/BAD_VERSION/UNKNOWN_CMD/BAD_PARAM/NOT_READY。

HELLO0x01はpayload12（合計19）。paramはmin2/max2/input0DS4または1BLE/reserved0。payload0–3 boot_id非0、4–5 firmware版、6feature、7capacity128、8–10 local_mask(L3=0x000002)、11予約。feature bit0Classic/1BLE/2gimbal/3gap/4入力選択。

POLL0x10 paramはu32 ack_id。payload28（合計35）：

| offset | 型・内容 |
| --- | --- |
| 0–3 | u32 boot_id。 |
| 4/5/6/7/8 | u8コントローラーlink/gimbal link/local button/fault/電量0..10または255。 |
| 9–10 | u16本体押下数、65535でwrap。 |
| 11–13 | u24現在button、L3除外。 |
| 14/15/16/17 | i8 RX/RY、u8 LT/RT。 |
| 18 | bit0valid/bit1gap。 |
| 19–22 | u32 event ID、無効時0。 |
| 23–25 | u24 event button、L3除外。 |
| 26 | 残未確認event数、255飽和。 |
| 27 | bit0SIM、bit1–7source世代。 |

link0off/1探索/2接続/3ready。fault bit0overflow/1I²C/2BT/3gimbal。button bit0Share/1除外L3/2R3/3Options/4Up/5Right/6Down/7Left/8L2/9R2/10L1/11R1/12Triangle/13Circle/14Cross/15Square/16PS/17Touch、残り0。triggerはanalogで判定します。

write後15msで別read、repeated-startなし。poll50ms、timeout100ms、失敗3回offline、probe1s、版不一致5s。retryはseq/ack保持。boot/source変更はrelease/baseline、press生成なし。ACKは一致headだけ削除、重複は冪等。overflow gapは対応event ACKまで保持し、held同期だけでactionなし。

slaveはA5で再同期、partial20ms破棄、合法requestだけheartbeat更新。core0 reply adapterはSDK5.5.1 private ringbuffer/FIFOを置換し、SDK更新時再監査が必要です。HostはISR/FIFO時限や実30分を証明しません。[詳細byte台帳](../../design/i2c-protocol-design.md)、[入力](gamepad-design.md)、[Mini](rs3-mini-protocol.md)を参照してください。
