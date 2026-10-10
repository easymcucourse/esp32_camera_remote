# Sony PTP/IP要件

[English](../../en/request/sony-ptpip-request.md) · [简体中文](../../request/sony-ptpip-request.md) · **日本語**

Sony ZV-E10のPC Remote、TCP15740が対象です。探索、永続登録、再接続、取景、property、目標設定、focus/撮影/録画、zoomを要求します。AP/UI/buttonは別要件です。

| 分類 | 必要動作 |
| --- | --- |
| R1探索 | 実DHCP/MAC、未登録は一意候補、登録後は保存対象。phoneでsessionを変更しない。 |
| R2登録 | ESP32-Camera-Remoteを一度許可、通常更新でGUID/peer保持、拒否表示、Web reset。保存再接続初frame≤10sは実測目標。 |
| R3取景 | 初decodeで中央1024×576。旧下限は全≥3.5FPS/SETTINGS≥2.4FPS、後のFPS計画はさらに強い全画面/read基準。30分安定heapと壊れframe復旧も必要。 |
| R4停止 | 全段階のhandshake/read取消、排出後close、最終画面保持、同識別で再開。全体≤1sは未実測。 |
| R5復旧 | 1–30s退避、許可拒否はユーザー待ち。旧actionを取消し再接続で再送しない。 |
| R6property | 基本9/追加9、eventと5s定期更新。event≤1sは目標。不明raw/`--`、parse失敗は旧状態保持。 |
| R7制御 | 最終目標合成、PENDING/APPLIED/REJECTED/TIMEOUT、読み戻し、S2先release、安全優先。不明録画を推測しない。 |
| R8頑健性 | 異常・切断・過大・誤transactionを安全に拒否。段階/opcode/transaction/errorを記録。 |

電動zoomはユーザー申告であり自動識別ではありません。非電動MF代替は現在無効です。UART `u` は廃止し、識別解除は起動Web全resetです。受付と物理効果は別です。

初回・再登録、拒否、各段階取消、断線後再送なし、複数client、本体変更、快速10目標、全action読み戻し、30分を検証します。過去実測は当時のbinに限定します。[設計](../design/sony-ptpip-design.md)、[FPS計画](../design/liveview-memory-fps-plan.md)、[現状](../development/current-status.md)を参照してください。
