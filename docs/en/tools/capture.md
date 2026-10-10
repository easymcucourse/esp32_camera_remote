# Capture and analysis

**English** · [简体中文](../../tools/capture.md) · [日本語](../../ja/tools/capture.md)

Install Wireshark with `dumpcap` and `tshark`; Windows needs a working capture driver. Query interfaces with `dumpcap -D`. Use the camera's actual address on the capture network:

```powershell
./tools/capture.ps1 -CameraIP 192.168.110.46 -Interface 1 -Seconds 120
```

The address is a historical example, not the LCD camera address. `-CameraIP/-Interface` are required; duration is 10–600 seconds, default 120. Output is `captures/remote-<time>.pcapng`. The filter includes that camera plus SSDP UDP1900 and mDNS UDP5353. Reconnect, allow initialization, perform one action at a time and record timestamps.

```sh
python tools/analyze.py captures/your-capture.pcapng
python tools/extract_property_sample.py captures/your-capture.pcapng captures/props.bin --stream 4 --transaction 9
```

`analyze.py` uses TCP Follow reassembly per direction and PTP/IP length/type framing to produce `.ptpip.csv`; direction ordering is **not** a chronological timeline. `--tshark` selects the executable. Property extraction requires `--stream`, optionally `--transaction`, `--max-packets` (default 50000) and `--tshark`; absent transaction selects the first complete successful `0x9209` dataset. The full output can contain unknown or identifying properties: keep it ignored.

`extract_liveview_sample.py` is a historical reproduction tied to one local September 27 capture, stream 0 and transaction 11. It is not a general parameterized extractor and its raw capture is not distributed. Optional Pillow validates JPEG dimensions.

For firmware performance, use [serial logging](../development/serial-log.md) and `tools/analyze_liveview.py`, with separate full/SETTINGS windows. Network packet timing and last-frame log samples measure different things. No analyzer alone proves physical actions or long stability.

Publish only redacted integer fixtures and evidence summaries. Exclude original captures, real MAC/GUID/addresses, credentials and frame content. See [fixture provenance](../../../tests/host/fixtures/README.md) and [historical evidence](../records/README.md).
