# Sony PTP/IP設計

[English](../../en/design/sony-ptpip-design.md) · [简体中文](../../design/sony-ptpip-design.md) · **日本語**

Cameraはgeneric探索/session/stream/control、Sonyはvendor parser/encoderと **唯一PTP client**、PTPは標準wire/session/transactionとWi-Fi message channelを所有します。UIはreadonly JPEG/property leaseを消費。旧fd APIはtest-onlyです。

DHCP候補TCP15740を各800msでprobe、未登録一意、登録後MAC/GUID一致。GUID-only移行は識別保持し全初期化後peer保存。`sony_remote/guid/peer` primitiveはcommon_runtime、通常保存は内部stack Camera workerです。

初期化：command InitRequest/Ack →返されたconnection番号でevent InitRequest/Ack →OpenSession0x1002(id1) →0x9201(1,0,0),(2,0,0) →GetDeviceInfo0x1001(0) →0x9202(300) →0x9201(3,0,0) →0x9202(300) →property0x9209(0) →GetObjectInfo0x1008(FFFFC002) →連続GetObject0x1009。唯一transaction/sessionです。

初回120s、保存10s、通常/event5s。nested readも絶対期限、Wi-Fi selectは最大100ms単位でcancel。PTPはtoken/世代/leaseだけ。InitFail/GUID不一致は待機、networkは1,2,4,8,16,30s退避、初表示でfailure解除。

完全0x200Fはslot返却100ms後同session再試行、連続50超で再接続。EndData欠落/誤transaction/過大/network失敗はclose。`s` はcontrol取消とframe/backend排出後終了、UART ACKは受付だけ。全体時限は実機待ちです。

全0x9209 descriptor検証後publish。0xC203で全更新、通常5s/pending500ms。generic target/actionとSony wireを分離。不明enum/WB微調整はraw、保存WB温度はactiveとは限りません。POWER_ZOOMは申告です。S1/S2/録画受付は物理効果ではありません。[menu](camera-menu-design.md)、[message](module-message-contracts.md)、[テスト](../development/testing.md)を参照。[中国語設計](../../design/sony-ptpip-design.md)では現実装と旧14節案を分けています。
