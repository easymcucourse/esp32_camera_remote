# Documentation

**English** · [简体中文](README.zh-CN.md) · [日本語](ja/README.md)

Start with the [English documentation index](en/README.md), [current implementation/validation](en/development/current-status.md), and [quick start](en/user-guide/quick-start.md). The repository overview is [here](../README.md); ATOM is a separate [ESP-IDF project](../m5_atom_matrix/README.md).

Current guides, requirements and designs describe different things: implemented behavior, intended behavior, and implementation contracts. Unimplemented requirements remain explicit. [Dated records](records/README.md) retain their original evidence and validation boundaries; an old statement of “current” is not a current release claim.

<a id="适配目标"></a>
## Targets

Waveshare ESP32-S3-Touch-LCD-7B, M5Stack ATOM Matrix, Sony ZV-E10, DJI RS 3 Mini and Classic DualShock 4 are the current targets. Ultimate 2 uses a descriptor-specific BLE parser with physical mapping still pending. Classic/BLE transport and Sony/Xbox report layout are separate dimensions; one parser does not support every controller of either family.

## Languages and maintenance

Navigation order is English, Chinese, Japanese. Current usage, development, requirements, design and tooling topics have locale entries. Historical captures and dated records remain source evidence rather than being rewritten into current successes. See the locale indexes for coverage and links to the originals.

<a id="免责声明"></a>
## Project status

This is an unofficial project, with no vendor endorsement. Public references and observations of owned equipment inform the protocol implementation; they are not an official vendor SDK. Follow ordinary device pairing/activation. Software and documents are supplied as-is; validation limits and unfinished work are recorded explicitly.

<a id="通信记录的公开范围"></a>
## Evidence publication

Keep raw captures, UART packets/logs, images of identifiable subjects, device serials/GUIDs/MAC/Bluetooth addresses, pairing records, actual credentials and tokens local and Git-ignored. Do not publish traffic from unrelated devices or confidential vendor material. Capture only owned devices on a private test network.

Public documents may contain operation/property codes, formats, timing/statistics, transaction/capture filenames and redacted test fixtures. Use fixed replacement identities and test-card images when exporting fixtures. Review staged files before publishing. Detailed fixture rules are in [testing](en/development/testing.md).
