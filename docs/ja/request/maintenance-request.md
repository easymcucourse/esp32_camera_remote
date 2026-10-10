# メンテナンス要件

[English](../../en/request/maintenance-request.md) · [简体中文](../../request/maintenance-request.md) · **日本語**

旧可逆modeとPIN/login設計を置き換えます。LCDはAP上（通常 `192.168.4.1:80`）で **起動限定・認証なしmaintenance** を提供します。APへ接続したclientは全操作が可能です。ATOM OTAとInternet更新確認は範囲外です。

1. STARTUPでHTTP triggerを開き、最初のrequestが排他modeを取得します。NORMALでは閉じ、再起動まで再入場できません。
2. Coreは入力release、Camera/JPEGと通常service排出、token取消、固定 **MAINTENANCE** 画面への凍結後にWeb全routeを有効にします。失敗は再起動し、部分停止serviceを戻しません。
3. Webは版/build/partition/IDF、稼働情報、AP・controller・表示設定、確認付きAP/全reset、LCD OTA、exit/rebootを提供します。PIN/token/cookie/loginはありません。
4. APは[ネットワーク要件](wifi-ap-request.md)に従います。staged応答成功後にcommitし、Coreが結果を追跡して再起動します。UI設定は保存後再起動で読み込みます。
5. resetはscopeとconfirm必須です。APだけはCamera識別を保持、全resetはLCD Camera/UIも消去し、ATOM登録と無関係storageを保持します。namespace横断失敗は部分変更を残し得ます。
6. OTAは検証済みLCD app、5MiB上限、非実行6MiB slotを使い、検証後だけboot選択します。誤chip/過大/壊れた/中断imageをbootにしません。pending imageは正常60秒後に確定。exit、保存成功、OTA成功で再起動します。

初request競合、NORMAL拒否、AP隔離、実browser、完全排出・固定画面、ACK消失・保存/reset失敗、upload切断・電源断、rollback、識別保持を検証します。Host/build証拠はありますが、新分割版の全面実機合格は未完です。過去のdevice-loop OTA成功は当時のbinだけの証拠です。[設計](../design/maintenance-design.md)と[現状](../development/current-status.md)を参照してください。
