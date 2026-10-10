# UI設計

[English](../../en/design/ui-design.md) · [简体中文](../../design/ui-design.md) · **日本語**

UIはmodel/navigation/font/renderer/JPEGを所有し、display_surfaceから一つのback canvas leaseを取得します。boardはRGB/front/backを所有。Camera/Input/networkは意味値・readonly leaseだけを渡し描画しません。短model lock内で描画/networkを行いません。

接続は `easymcucourse camera console`、実SSID/password/IP/default、登録段階、機器link。LIVE1024×576(0,12)、SETTINGS768×432と右menu/下extra。WIFI/FPS/CAM/FW/BATTERY/DS4-or-XBOX/MODE/FOCUSは実snapshot。不明 `--` /hex、電量>50緑/21–50黄/≤20赤。

info full/compact/hiddenとコントローラー型を起動ui_prefsから読みます。通常GET-only、Inputのtouchpad info-nextは無視。writerはWeb保存・再起動。REC赤枠は録画読戻し、SIM/gimbal faultは非表示档位でも残します。focus枠/拡大は計画です。

UI endpointがJPEG/menuを直列実行。frameはreadonly lease/token/read時限、resultは元frame世代でUI寿命ではありません。成功/壊れ/表示失敗/停止/遅reply/満杯でもlease回収とmetadataを処理。decoder/workは起動確保、pixelはcanvas借用、第三全画面copyなし。復旧はbuffer再利用です。

Debug benchは共通rendererとtyped Camera予約・排出、合成20frameでCamera/TCPを含みません。maintenanceは通常受付閉鎖、token取消、user排出、model凍結、固定画面。errorで通常描画を再開しません。実RGB/SMP/fontと新gimbal障害表示は実測待ちです。[menu](camera-menu-design.md)、[所有権](module-resource-ownership.md)、[要件](../request/ui-request.md)を参照してください。
