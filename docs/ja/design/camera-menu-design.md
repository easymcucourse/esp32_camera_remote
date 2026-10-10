# カメラメニュー

[English](../../en/design/camera-menu-design.md) · [简体中文](../../design/camera-menu-design.md) · **日本語**

UIはcursor/navigation、Inputは意味的route取得後Cameraへproperty方向を送ります。Camera純kernel `camera_menu` / `setting_control` が基本7項と追加9目標を所有します。cursor移動で旧調整を別propertyへ転送しません。

順番はFocus、Shutter、Aperture、ISO、EV、WB、Meter、MORE、Wi-Fi。上下と完全列挙左右は循環。400ms後150ms repeat、複数方向/gap/mode/session変更後解除必須。MOREはASPECT/DRIVE/EFFECT/DRO/AF AREA/WL FLASH/WB TEMP/WB AB RAW/WB GM RAW/EXIT。Wi-Fiは情報、maintenance項なし。

write前に全descriptor、scalar型、write能力、実値、第一完全列挙を検査。不明/重複/切断/過大/current不明は許可しません。第二列挙でreadonlyを回避しません。EVはsignedINT16昇順、右増加・左減少、原16bit wire保持。他の列挙順は保持します。

actual/desired/sentを分け、快速入力はdesired合成、sent読戻し後だけ次を送ります。10s timeout、拒否/能力/安全変更で消去。Mode待機は他writeを止め、pending時約500ms更新。Focus menuとXは同targetです。

Shutter/apertureは列挙があれば絶対値。なければ有効write可能currentを前提にint8 ControlDeviceB一step、方向変化読戻し待ち、最大64 net step合成。絶対値を推測しません。FFFFFFFF/FFFEでは禁止。wireテストは実ZV-E10成立を証明しません。

安全/release/録画/撮影をproperty/frame前に処理し、新snapshot当たり一write、直前安全世代確認。actualとTO/PENDINGを表示、終状態3s。[入力](gamepad-design.md)、[Sony](sony-ptpip-design.md)、[テスト](../development/testing.md)を参照してください。
