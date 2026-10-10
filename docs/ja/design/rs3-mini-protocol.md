# RS 3 Miniプロトコルと制約

[English](../../en/design/rs3-mini-protocol.md) · [简体中文](../../design/rs3-mini-protocol.md) · **日本語**

独立実装の **非公式** adapterです。固定版参考とMITは[third_party](../../../third_party/rs3-protocol/README.md)。Mini bridgeがwire根拠、RS3参考は交差根拠でMini合格ではありません。公式Ronin Appで有効化し、以前動いたことから有効化済みと推定しません。

FFF0、FFF5 Write Without Response、FFF4 Notify、2902 CCCDを動的発見。MTU185、22byte frameに必要な25未満を拒否。実FFF4はnotify-only、CCCDへ0100、characteristicのwrite能力を要求しません。宣言write対応時だけ追加初期化する分岐は未実測。300ms待機、neutral/poll、有効DUML RX後readyです。

DUMLは55/10bit length+version/header CRC8/sender/receiver/seqLE16/flags/set/id/payload/CRC16LE。CRC8初77反射8C、CRC16初3692反射8408。256byte有界受信で分割・結合・壊れたlength/version/CRC後再同期します。

| 動作 | receiver/flags/set/id | payload |
| --- | --- | --- |
| stick/neutral | 04/40/04/01 | Tilt,Roll,Pan LE16中央1024、末尾000002。 |
| 固有center | 04/40/04/4c | fe01。 |
| 現heartbeat1Hz | 04/00/04/12 | 1051010000000c00005000f1036624c01d00001c。 |
| 電量notify | senderE5→02、0d/02 | payload厳密21byte、末尾0–100、15sで不明。 |

endpoint4はMini参考builderに従い実機成功、参考raw captureはE5です。旧E5はtelemetryが来ても物理動作なしでした。根拠を混同し全firmware互換を宣言しません。

未登録は `DJI RS3 MINI-` / `DJI RS 3 Mini` 名の一台だけ。登録後は名前なしでもaddress一致、他機自動置換なし、複数候補は待機。readyでapp target保存、`gimbal pair` はその対象だけ解除しDS4/SMP bond解除とは記しません。dispatcherはscan直列、app/interface/peer filterです。

20ms worker、200ms manual、入力<200ms、neutral後arm、固有center、TX一個＋neutral予約は[制御設計](gimbal-design.md)。失敗5回、完了500ms、RX無通信5s、event overflowでclose/fault。実≤100ms停止は未実測です。

readonly poseは正端点04/66 header1、重複なし二byte tag22Tilt/23Roll/24Panを全部要求。単位・零点未校正。有効化前31byteで一度valid、有効化後19byteは三軸なしで **valid false**。角度limit/任意zero保護はありません。

stick/L3、120/240、gimbal電源再投入正常を確認済み。約14分は30分合格ではなく、center ACKは取消実測でもありません。新LCD障害表示はHost/buildのみ未書込。[現状](../development/current-status.md)と[詳細台帳](../../design/rs3-mini-protocol.md)を参照してください。
