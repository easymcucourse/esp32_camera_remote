# 取景メモリとFPS計画

[English](../../en/design/liveview-memory-fps-plan.md) · [简体中文](../../design/liveview-memory-fps-plan.md) · **日本語**

2026-10-07段階計画と実行訂正です。見積りと旧「buildなし」は現事実ではありません。[詳細段階](../../design/liveview-memory-fps-plan.md)と[現状](../development/current-status.md)を参照します。

同条件純10分全画面：平均FPS≥5、最低5秒≥4、read P50≤150ms/P95≤300ms、SETTINGS≥2.4かつ悪化なし。内部最低≥40KBかつbaseline以上、最大block≥16KB、PSRAM最低≥1MB（byte単位を明記）。30分NO_MEM/stream退出なしは別gateです。

段階0は実sdkconfig/size/heap、純全/SETTINGS FPS/phase、必要時TCP capture。混在baseline拒否。段階1はWi-Fi/lwIP PSRAM優先、内部reserve32768と実DMA/static影響。現defaultsはRX10/RX BA6、static TX6/cache32、ooseq4、main stack24576。PSRAM優先はstatic TX必須で、初16buffer/ooseq無制限は内部heap悪化後修正しました。

段階2は前段合格後、受信window/mailbox/RXを個別測定。SDK ooseqは0..12で旧16/32案は無効。HTTP含むsocket全体を確認。新tagでdefaults再生成、reconfigureだけで旧sdkconfigは更新されません。

段階3第三512KiB slotは計画で現sourceは **二slot**、readonly lease/result/drainです。追加slotは揺れ吸収で単独throughput増ではありません。段階4decode/copy/overlay/publishを個別計測。benchはCamera/networkなし。独立再接続作業はLCD再起動後Camera電源再投入なしで復帰、識別・protocol cleanup保持です。

最新停止30分sampleは平均4.76081、最低3.34、readP50=171ms。NO_MEM/退出なしでも最終性能 **未達**。短いStable目視は全profile/OTA/NVS/再接続合格ではありません。約5sごとの最終frame標本です。[解析](../development/serial-log.md)で各段階を保存・個別rollbackし、全計画完了としません。
