# 型付きメッセージ契約

[English](../../en/design/module-message-contracts.md) · [简体中文](../../design/module-message-contracts.md) · **日本語**

全番号はAPP_MESSAGE_ prefixです。表は現/廃止/予約46番号を含み、同状態の隣接だけまとめます。[全field台帳](../../design/module-message-contracts.md)と `app_message.h` /handlerが厳密payloadです。maintenanceは通常busを使いません。

task requestはcorrelation、非0世代、絶対esp_timer期限。routerは別にsource/target寿命とdestination epochを管理。request/reply lockは原期限、普通sendはzero-wait。transport ESP_OK後もreply.result確認。timeoutは実行済みactionや配達leaseを撤回しません。

値コピー、BULKはlease必須。readonly変更不可、writableは指定receiverだけ。sendは成功失敗ともref消費、receive/reply callerがrelease。partial fanout/cancel/timeout後も最後callbackまでbuffer/context生存。control優先。network世代、安全世代、report epoch/ID、channel token、endpoint寿命を混同しません。

| ID | 完了・所有権 |
| --- | --- |
| `CAMERA_DISCOVER` | DHCP候補最大4をコピー、session作成なし。 |
| `WIFI_RSSI` | 選択peer照会/約2s event、不明−127。 |
| `WIFI_CHANNEL_OPEN` | token/network世代、遅openはclose。 |
| `WIFI_CHANNEL_SEND` | 借用bulk、partial長/status、返却まで保持。 |
| `WIFI_CHANNEL_RECEIVE` | 有界write lease、pollはlease/消費なし。 |
| `WIFI_CHANNEL_CLOSE` | I/O取消・排出・close後ACK、open照合取消。 |
| `WIFI_CONFIG_GET` | configコピー照会。 |
| `WIFI_CONFIG_PREPARE / WIFI_CONFIG_COMMIT / WIFI_CONFIG_CANCEL / WIFI_CONFIG_RESULT` | 通常番号廃止、NOT_SUPPORTED、Webは隔離facade。 |
| `WIFI_SELECT_CAMERA` | RSSI target指定、登録保存なし。 |
| `WIFI_NETWORK_CHANGED` | 現世代control event再試行、排出ACKではない。 |
| `WIFI_STATUS` | network state、約200ms event。 |
| `CAMERA_START` | preview/診断flag、producer開始で接続完了ではない。 |
| `CAMERA_STOP` | UART flagは受付、通常lifecycle ACKは物理終了待ち。 |
| `CAMERA_FORGET` | 廃止、識別解除はmaintenance storage。 |
| `CAMERA_DISPLAY_SESSION` | Debug予約、timeout後も同token解除必須。 |
| `CAMERA_ACTION` | 安全世代/action受付、結果は読み戻し。 |
| `CAMERA_SETTING_ADJUST / CAMERA_MENU_ACTION` | 意味property/方向、desired受付。 |
| `CAMERA_FRAME` | readonly JPEG bulk/token/frame世代/read時間。 |
| `UI_FRAME_RESULT` | 原frame世代/token、leaseなし、metadata最大2。 |
| `CAMERA_STATE` | producer世代state、信頼event履歴ではない。 |
| `CAMERA_CAPABILITIES` | 安全世代は別、現publishはproperties内。 |
| `CAMERA_PROPERTIES` | 厳密size readonly snapshot、backend pointer不可。 |
| `CAMERA_COMMAND_STATUS` | control状態0..5、publish失敗でaction取消なし。 |
| `CAMERA_STATUS` | 瞬間snapshot、reply待機中変更あり。 |
| `UI_MENU_ACTION` | Input menu500ms、意味reply、安全RELEASE_ALL。 |
| `UI_STATE` | 未使用予約、NOT_SUPPORTED。 |
| `UI_STATUS` | UI snapshot、将来page保証なし。 |
| `UI_PREFERENCES` | GET-only flag=false、leaseなし、旧setter不可。 |
| `UI_PROPERTY_STATUS` | 表示actual/target/status、意味property。 |
| `DISPLAY_BENCH` | Debug token worker、30s budget、完了event別。 |
| `DISPLAY_FAULT` | Debug注入、canvas所有権なし。 |
| `INPUT_STATE / INPUT_STATUS` | Input寿命とsource_epoch/report_idは別、stateコピー。 |
| `INPUT_SELECT` | ATOM0/SIM1、release完了後rearm。 |
| `INPUT_ATOM_COMMAND` | pad kindはInputだけ、UART監視/統計値コピー。 |
| `INPUT_SIM_COMMAND` | Debug readonly seqを4job/8完了へコピー、寿命取消。 |
| `SYSTEM_STATUS` | mode/uptime/heap/restart snapshot。 |
| `SYSTEM_RESTART` | 予約commit500–10000ms、ACK消失でもrestart。 |
| `SYSTEM_FACTORY_RESET / SYSTEM_FACTORY_RESULT` | 通常廃止、factoryはWebだけ。 |
| `SYSTEM_CAMERA_SESSION` | Camera attempt世代policy、maintenance/canvas leaseではない。 |
| `SYSTEM_ENTER_NORMAL` | UI寿命/page要求、この起動のtriggerを閉鎖。 |

handler範囲/producer検査はdomain契約でhotspot認証ではありません。errorはrollbackを意味しません。fake RTOS/直接symbol/source/buildはsoftware根拠、実SMP/HTTP/Camera/cache-off/停止は別検証。[所有権](module-resource-ownership.md)と[検証](../development/module-split-checklist.md)を参照してください。
