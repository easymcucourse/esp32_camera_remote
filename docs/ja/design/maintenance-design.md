# メンテナンスとOTA設計

[English](../../en/design/maintenance-design.md) · [简体中文](../../design/maintenance-design.md) · **日本語**

Core mode STARTUP/NORMAL/ACTIVATING/MAINTENANCE/RESTARTは起動中単方向です。起動port80で排他claim、NORMALで閉鎖、trigger→activating→active→closedから再開なし。Webは **認証なし**、隔離Wi-Fi objectと注入Core storage/restartを使い通常Console/Camera/UI APIへ依存しません。

Coreは通常受付閉鎖、Input release、Camera/JPEG/owner排出、model凍結、固定MAINTENANCE後route有効化。失敗はrestartし未終了owner保持、normalへ戻しません。APは維持します。

HTTP priority3、内部6144byte、3socket、15handler、recv/send10s。停止はadmission閉鎖→SDK session-close予約→同期joinで、project3000msは厳密SDK終了上限ではありません。

| route | 用途 |
| --- | --- |
| GET /、/api/info | page/診断。 |
| GET/POST /api/settings、/api/controller | controller/info、互換routeは共通実装。 |
| GET/POST /api/wifi、GET /api/wifi/random_password | config/token、stage保存、生成のみ。 |
| POST /api/factory | 厳密scope wifi/all、confirm:true。 |
| POST /api/maint/exit、/api/reboot | restart予約、復帰なし。 |
| POST /api/ota/check、/api/ota、GET /api/ota/status | prefix/full upload/進捗。 |

404/405含む初requestも先にtrigger gate。settings保存後1500ms再起動、返信消失でも実行。Wi-Fiは予約→prepare→ACK→commit、ACK失敗はcancel。Coreはclient照会なしでtoken追跡。factoryはconfig凍結、allはLCD Camera/UI/APだけ、ATOM保持。部分失敗はWi-Fi rollbackだけでnamespace全体原子ではありません。

NVS0x9000/0x6000、otadata0xF000/0x2000、PHY0x11000、app0x20000/0x620000各6MiB、data0xC20000。初移行USB、通常app更新はmetadata/識別保持。OTAは288byte prefix、十進X-Image-Size、application/octet-stream、full再検査、4096byte PSRAM chunkを内部HTTP栈で非実行slotへ書き、esp_ota_end後boot選択。失敗abort/cancel、成功は返信消失でも再起動。正常ready60sでpending確定、不健康・未確定resetはrollback。

source/Host/buildと実AP隔離、blocked stop、Flash/cache-off、電源断、画面/browserは別です。[所有権](module-resource-ownership.md)、[要件](../request/maintenance-request.md)、[現状](../development/current-status.md)を参照してください。
