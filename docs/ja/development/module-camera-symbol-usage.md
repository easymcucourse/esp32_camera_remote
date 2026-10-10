# カメラシンボル監査

[English](../../en/development/module-camera-symbol-usage.md) · [简体中文](../../development/module-camera-symbol-usage.md) · **日本語**

2026-10-06の[詳細台帳](../../development/module-camera-symbol-usage.md)は **本番global PTP/Sony関数39個**、当時Default ELF内37個、parser fixture用2個を記録します。旧47個・40個snapshotは履歴です。最新binの全symbolを再計数した値ではありません。

Sony固有parser/format/wire値encoderは `camera_backend_sony/private`、そのbackendは唯一の `ptpip_client_t` を含みます。共通PTP wire/sessionは `ptpip`、generic runtime/controlは `app_camera` です。fixture使用の旧fd・exposure転送wrapperはテスト専用へ移し、テストを削除しません。定数・helperは別途source参照を調べます。

本番caller、要件に根拠のあるAPI、有意なfixture依存のいずれかがあれば保持します。削除・統合は三条件を確認しbyte assertionを維持します。ELF非存在はdead strippingの可能性があり、source未使用とは限りません。archive直接辺とheader gateも使いますが間接callbackは観測できません。

`tools/ci_build.py` で現構成をビルドし、ELF/MAPと `module-symbol-edges.json` を調べ、契約テストを実行します。10月10日の四構成監査は合格しましたが、protocol変更による実挙動はカメラsmokeが必要です。[依存](../design/module-dependency-graph.md)と[検証](current-status.md)を参照してください。
