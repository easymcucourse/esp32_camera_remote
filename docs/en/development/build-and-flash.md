# Build and flash

**English** · [简体中文](../../development/build-and-flash.md) · [日本語](../../ja/development/build-and-flash.md)

Use **ESP-IDF 5.5.1**. The LCD target is `esp32s3`; ATOM Matrix is classic `esp32`. Manifests and lockfiles fix dependencies. A local `sdkconfig` overrides defaults: `reconfigure` does not reset its values. Preserve the original before regenerating it.

From an activated IDF terminal at the repository root:

```sh
python tools/ci_build.py lcd debug --build-tag review
python tools/ci_build.py lcd release --build-tag review
python tools/ci_build.py atom debug --build-tag review
python tools/ci_build.py atom release --build-tag review
python tools/ci_build.py lcd debug --profile stable --build-tag review
```

These commands build without flashing. Each configuration has its own `build/ci-*` directory and sdkconfig. A fresh tag reads current defaults; reusing a tag retains existing settings. LCD builds enforce the actual component graph, direct-symbol ownership, a 5MiB image limit, rollback, and Release SIM exclusions. Stable uses 80MHz flash/PSRAM; default PSRAM 120MHz is experimental. The effective flash frequency must be checked in generated configuration and boot logs.

Windows wrapper examples: `./tools/idf.ps1 build`, `./tools/idf.ps1 build -Profile stable`, or `./tools/idf.ps1 build -ProjectDirectory ./m5_atom_matrix -Port COM6`. It accepts one action per invocation and explicit `-IdfPath/-IdfPython`, but no baud-rate option.

Initial LCD setup uses `idf.py set-target esp32s3`, `idf.py build`, then `idf.py -p COM8 flash`. In `m5_atom_matrix`, use target `esp32` and `idf.py -p COM6 -b 115200 flash`. Replace example ports. ATOM needs BTDM, BLE/GATTC, Classic HID Host, I²C slave driver v2 and IRAM-safe ISR; an old BR/EDR-only sdkconfig will not enable the gimbal.

The LCD layout has two 6MiB applications at `0x20000` and `0x620000`, OTA metadata at `0xF000`, NVS at `0x9000`. Initial partition migration requires full USB flashing. On an existing device, preserve NVS/OTA metadata and verify the active slot before application-only flashing; full flash can initialize OTA metadata. Web OTA accepts the **LCD application bin only**, writes the inactive slot and confirms it after 60 healthy seconds. ATOM updates still use USB. Full erasure loses configuration and identities.

Host tests require CMake, a host GCC/Clang compiler and cJSON (Linux package `libcjson-dev`, or SDK cJSON source):

```sh
cmake -S tests/host -B build/host
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
```

Keep one generator per build directory. See [testing](testing.md) and [current status](current-status.md): 267 host tests and four fresh Debug/Release builds passed on 2026-10-10; these newly built binaries were not flashed. I²C changes require both boards to update together.
