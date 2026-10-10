# ビルドと書き込み

[English](../../en/development/build-and-flash.md) · [简体中文](../../development/build-and-flash.md) · **日本語**

**ESP-IDF 5.5.1** を使用します。LCDは `esp32s3`、ATOM Matrixは従来の `esp32` です。依存版はmanifestとlockfileで固定します。ローカル `sdkconfig` はdefaultsより優先し、`reconfigure` だけでは値を初期化しません。再生成前に元の設定を保存します。

IDF有効化済みterminalで、リポジトリrootから実行します。

```sh
python tools/ci_build.py lcd debug --build-tag review
python tools/ci_build.py lcd release --build-tag review
python tools/ci_build.py atom debug --build-tag review
python tools/ci_build.py atom release --build-tag review
python tools/ci_build.py lcd debug --profile stable --build-tag review
```

これらは書き込みを行いません。各設定の `build/ci-*` に独立したsdkconfigを作ります。新しいtagは現在のdefaultsを使い、再利用tagは旧設定を保持します。LCDは実component graph、直接symbol所有権、5MiB上限、rollback、ReleaseのSIM除外を検査します。Stableはflash/PSRAM 80MHz、通常PSRAM 120MHzは実験設定です。実flash周波数は生成設定と起動ログで確認します。

Windows例：`./tools/idf.ps1 build`、`./tools/idf.ps1 build -Profile stable`、`./tools/idf.ps1 build -ProjectDirectory ./m5_atom_matrix -Port COM6`。一回一actionで、`-IdfPath/-IdfPython` を指定できます。baud指定はありません。

LCD初期導入は `idf.py set-target esp32s3`、`idf.py build`、`idf.py -p COM8 flash`。`m5_atom_matrix` 内ではtarget `esp32`、`idf.py -p COM6 -b 115200 flash` を使用します。ポートは実機に置き換えます。ATOMはBTDM、BLE/GATTC、Classic HID Host、I²C slave v2、IRAM-safe ISRが必要です。旧BR/EDR-only設定ではジンバルが有効になりません。

LCDは6MiBのapp二個（`0x20000`、`0x620000`）、OTA metadata `0xF000`、NVS `0x9000` です。初回partition移行は完全USB書き込みが必要です。導入済み機器のapp単独更新ではNVS/OTA metadataを保持し、実行slotを確認します。完全flashはmetadataを初期化する場合があります。Web OTAは **LCD app binのみ** を受け付け、非実行slotに書き、60秒の正常運転後に確定します。ATOM更新はUSBです。全消去では設定と識別情報を失います。

HostはCMake、GCC/Clang、cJSON（Linux `libcjson-dev` またはSDK source）が必要です。

```sh
cmake -S tests/host -B build/host
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
```

一つのbuildディレクトリでgeneratorを混在させません。[テスト](testing.md)と[現在の状態](current-status.md)を参照してください。2026-10-10にHost267件と新規Debug/Release四構成が合格しましたが、新binは実機へ書いていません。I²C変更時は両ボードを同時更新します。
