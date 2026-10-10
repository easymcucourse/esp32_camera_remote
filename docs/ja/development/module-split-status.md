# モジュール移行状況

[English](../../en/development/module-split-status.md) · [简体中文](../../development/module-split-status.md) · **日本語**

本番sourceにモジュール分割を実装しています。`main` はCoreを開始し、Coreが構成、mode、起動barrier、health、restart、lifecycle、factoryを所有します。通常サービスはConsole/routerの型付きmessageで通信します。[構造](../design/architecture-design.md)を参照してください。

UI → display surface → boardで表示・canvas所有権・hardwareを分離します。Cameraはruntime/backend/session/control/JPEG slotを所有し、Sonyはvendor解釈と唯一のPTP clientを持ちます。PTPは裸socketではなくWi-Fi message channelを使います。Inputはreport/actionを所有し、実ATOMとDebug SIMは別providerです。UARTはmessageを符号化し業務状態を所有しません。

起動maintenanceは排他的で、再起動まで戻れません。最初のHTTP claimで通常ownerを停止・排出し、固定MAINTENANCE画面にしてからWeb全機能を有効にします。認証、通常modeからの再入場、UART設定編集、再起動なしの取景復帰はありません。Coreが限定storage callbackを調整し、maintenanceはCamera/UIを直接操作しません。

移行fixtureと現在の四構成は合格しています。最新件数は[現在の状態](current-status.md)。過去の五構成にはStableも含みますが、今回の五構成合格とは記しません。[中国語の各回台帳](../../development/module-split-status.md)には各移行と原証拠があります。

実機移行は **全面合格ではありません**。内部RAM、router SMP lock、入力・期限の問題と修正・短測は記録済みです。その後の取景も最終性能基準を満たしていません。実起動順、全owner排出時限、HTTP隔離、storage/電源断、全画面、長期安定性は別検証が必要です。[検証一覧](module-split-checklist.md)を参照してください。
