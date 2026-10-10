# UI要件

[English](../../en/request/ui-request.md) · [简体中文](../../request/ui-request.md) · **日本語**

LCDは1024×600 RGB565でtouchを初期化しません。英語Inter、中国語Source Han Sans、数値JetBrains Monoを使います。機器表示は英語で、文書翻訳はUI言語を変更しません。

接続画面は **easymcucourse camera console**、独立SSID/password/IP行、接続・登録段階、Expansion/コントローラー/Camera/gimbal状態、default-password表示です。LIVEは中央1024×576、SETTINGSは768×432 previewとmenu。初frame、Options/Start、UART `S`、切断で遷移します。

右欄はWIFI、FPS、CAM、FW、BATTERY、選択コントローラーDS4/XBOX電量、MODE、FOCUSです。既知電量>50%緑、21–50%黄、≤20%赤、不明は灰 `--`。menuは実読み戻し、書込可能cursor/target/PENDING/拒否/timeoutを表示し成功を偽装しません。EVは右増加・左減少、完全列挙は循環です。

LIVE full/compact/hiddenは **起動Webで保存し再起動後に読み込みます**。現在serviceはtouchpadで切り替えません。SETTINGSは表示を維持します。RECと枠は実録画読戻し、SIMとGIMBAL FAULTは通常情報非表示でも必要時に表示します。新LCD障害表示はbuild/Host済み、未書き込みです。

MOREはASPECT、DRIVE、EFFECT、DRO、AF AREA、WL FLASH、WB TEMP、WB AB RAW、WB GM RAW。Aで入場・確定、B/EXITで戻ります。Wi-Fiは情報のみ。maintenanceは固定排他画面、起動HTTPだけで、SETTINGS入口はありません。

focus赤/緑位置枠、Select/右stick/R3、拡大、統一NO ZOOMは計画です。状態遷移、Web保存後各情報量、Camera確認REC/menu、raw/不明、全障害、実pixel時限を検証します。旧FPS/layoutは日付付き証拠です。[設計](../design/ui-design.md)、[操作](../user-guide/controller.md)、[現状](../development/current-status.md)を参照してください。
