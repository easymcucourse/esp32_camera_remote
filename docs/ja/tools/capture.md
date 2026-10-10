# キャプチャと解析

[English](../../en/tools/capture.md) · [简体中文](../../tools/capture.md) · **日本語**

`dumpcap` と `tshark` を含むWiresharkを導入します。Windowsは有効なcapture driverが必要です。`dumpcap -D` でinterfaceを調べ、captureネットワークでの実カメラIPを使います。

```powershell
./tools/capture.ps1 -CameraIP 192.168.110.46 -Interface 1 -Seconds 120
```

IPは過去の例でありLCD側カメラIPではありません。`-CameraIP/-Interface` は必須、時間10–600秒、初期値120秒です。出力は `captures/remote-<time>.pcapng`。カメラ通信、SSDP UDP1900、mDNS UDP5353を採取します。再接続・初期化後、一動作ずつ行い時刻を記録します。

```sh
python tools/analyze.py captures/your-capture.pcapng
python tools/extract_property_sample.py captures/your-capture.pcapng captures/props.bin --stream 4 --transaction 9
```

`analyze.py` は方向別TCP Follow再構成とPTP/IP length/typeで `.ptpip.csv` を作ります。方向順は時系列ではありません。`--tshark` で実行fileを指定します。property抽出は `--stream` 必須、`--transaction`、`--max-packets`（50000）、`--tshark` は任意です。transaction未指定は最初の完全成功 `0x9209` を使います。全datasetには未確認・識別propertyが含まれる可能性がありignoredへ保存します。

`extract_liveview_sample.py` は9月27日の特定local capture、stream0、transaction11を再現する履歴ツールです。一般parameter抽出器ではなくraw captureも配布しません。PillowはJPEG寸法検査に任意使用します。

性能には[シリアルログ](../development/serial-log.md)と `tools/analyze_liveview.py` を使い、全画面/SETTINGSを分けます。packet時限と最後のframeのログ標本は異なる量です。解析だけでは物理効果や長期安定性を証明しません。

公開は匿名化した整数fixtureと結論に限定し、raw capture、実MAC/GUID/address、認証情報、画像を除外します。[fixture出典](../../../tests/host/fixtures/README.md)と[過去の証拠](../records/README.md)を参照してください。
