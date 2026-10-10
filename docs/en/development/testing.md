# Testing and CI

**English** · [简体中文](../../development/testing.md) · [日本語](../../ja/development/testing.md)

The latest local code baseline is **267/267 CTest cases**, plus fresh LCD/ATOM Debug/Release builds on 2026-10-10. See [current status](current-status.md). Counts in dated records describe that batch only. Host fixtures, firmware builds, flashing, physical behavior and long-run acceptance are independent.

```sh
cmake -S tests/host -B build/host
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
python tools/check_doc_links.py
python tools/check_module_boundaries.py
```

Use cJSON for actual maintenance JSON tests; Linux needs `libcjson-dev`, Windows can pass `-DMODULE_CJSON_SOURCE_DIR=<IDF components/json/cJSON>`. Keep assertions enabled. The [workflow](../../../.github/workflows/ci.yml) runs host tests under Clang ASan/UBSan and four IDF 5.5.1 firmware configurations. It checks component/symbol boundaries and Release exclusions. Local passes do not establish a remote Actions pass or enabled branch protection.

Coverage includes router deadlines/correlation/lease retirement, provider epochs and release safety, Wi-Fi channel generations, shared PTP transactions/Sony descriptors, parameter targets/readback, JPEG leases/UI lifecycle, Core startup/drain, no-auth Web/OTA/reset, I²C v2/CRC/reassembly, DS4/Ultimate reports, Matrix model, gimbal protocol/control/TX gating, shared BLE scans and live-view analysis. Some legacy PIN/menu/fd helpers remain tested under `tests/support/legacy`; they are not production features.

Fake SDK, HTTP, NVS, decoder, scheduler or GPIO boundaries do not prove SMP timing, flash/cache-off behavior, actual pixels, radio stability or camera acceptance. Direct-symbol gates cannot observe all indirect callbacks; source review remains necessary.

Hardware acceptance requires cold startup, first pairing and saved reconnection, five-minute live view, stop/resume, controls/menu readback, ATOM reconnect, maintenance claim/save/reset/OTA and rollback. Inject camera/Wi-Fi loss, truncated/oversized/wrong-transaction data, display callback failure, I²C CRC/timeout/gap and queue overflow; releases must precede new actions.

Long-run tests need at least **30 minutes** with input, memory/stack/FPS/latency and failure counts recorded. The gimbal has only approximately 14 minutes of uninterrupted observed concurrency, plus a successful power-cycle test; neither is a 30-minute pass. The latest live-view plan's stronger FPS/read gates were not met. Use the thresholds in [the plan](../design/liveview-memory-fps-plan.md), not an older provisional 3FPS threshold. The log analyzer computes evidence, not acceptance.

Publish only redacted fixtures and conclusions. The four integer property fixtures are documented in [their index](../../../tests/host/fixtures/README.md). Original captures and raw identities stay ignored. See [module checklist](module-split-checklist.md) and [records](../records/README.md).
