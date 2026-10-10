# テストとCI

[English](../../en/development/testing.md) · [简体中文](../../development/testing.md) · **日本語**

最新ローカル基準は2026-10-10の **CTest267/267件** とLCD/ATOM Debug/Release四構成です。[現在の状態](current-status.md)を参照してください。日付付き記録の件数はその回だけの結果です。Host、ビルド、書き込み、物理効果、長期合格は独立しています。

```sh
cmake -S tests/host -B build/host
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
python tools/check_doc_links.py
python tools/check_module_boundaries.py
```

maintenanceの実JSONテストにはcJSONが必要です。Linuxは `libcjson-dev`、Windowsは `-DMODULE_CJSON_SOURCE_DIR=<IDF components/json/cJSON>`。assertを有効にします。[workflow](../../../.github/workflows/ci.yml)はClang ASan/UBSanとIDF 5.5.1四構成、component/symbol境界、Release除外を検査します。ローカル合格から遠隔Actionsやbranch protectionを推定しません。

router期限・照合・lease、provider epochとrelease、Wi-Fi channel世代、共通PTP/Sony descriptor、目標・読み戻し、JPEG lease/UI、Core起動・停止、認証なしWeb/OTA/reset、I²C v2/CRC、DS4/Ultimate、Matrix、gimbal protocol/control/TX、共有BLE scan、取景解析を検査します。旧PIN/menu/fd helperの一部は `tests/support/legacy` で検査しますが、本番機能ではありません。

SDK/HTTP/NVS/decoder/scheduler/GPIOのfakeはSMP時限、Flash/cache-off、実pixel、無線安定性、カメラの受理を証明しません。直接symbol検査は間接callback全体を観測できず、sourceレビューも必要です。

実機では冷起動、初回登録・保存再接続、5分取景、停止・再開、ボタンとmenu読み戻し、ATOM再接続、maintenance保存・reset・OTA・rollbackを確認します。カメラ/Wi-Fi切断、異常packet、表示callback消失、I²C CRC/timeout/gap、overflowを注入し、新動作より先にreleaseを行うことを確認します。

長期テストは **30分以上** とし、入力、heap/stack、FPS、遅延、障害数を保存します。ジンバルの並行観測は約14分と正常な電源再投入の短測であり、30分合格ではありません。取景の最新FPS/read基準は未達です。旧暫定3FPSではなく[計画](../design/liveview-memory-fps-plan.md)の基準を使います。解析ツールは証拠を計算し、合格判定を代行しません。

公開するfixtureと結論は匿名化します。四つの整数property fixtureは[一覧](../../../tests/host/fixtures/README.md)を参照。raw captureや識別はignoredに保持します。[モジュール一覧](module-split-checklist.md)と[記録](../records/README.md)も参照してください。
