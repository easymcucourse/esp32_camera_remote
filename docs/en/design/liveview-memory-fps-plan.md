# Live-view memory and FPS plan

**English** · [简体中文](../../design/liveview-memory-fps-plan.md) · [日本語](../../ja/design/liveview-memory-fps-plan.md)

This preserves the staged2026-10-07 plan with corrections from execution. Estimates and the old assumption of no build artifacts are not current facts. See [original detailed stages](../../design/liveview-memory-fps-plan.md) and [current status](../development/current-status.md).

Targets in comparable pure10-minute full-screen windows: meanFPS≥5, minimum5-second window≥4, read P50≤150ms/P95≤300ms; SETTINGS≥2.4 and no regression. Internal minimum≥40KB and no worse than baseline, largest block≥16KB, PSRAM minimum≥1MB (record exact byte units). Thirty minutes without NO_MEM/stream exit is a distinct stability gate.

Stage 0: record generated sdkconfig/size/heap, pure full/SETTINGS FPS and phase timings, optional TCP retransmission/window capture. Reject mixed-mode baselines. Stage 1: enable Wi-Fi/lwIP PSRAM preference, retain32768 internal reserve and compare real DMA/static-buffer consequences. Current defaults explicitly preserve RX10/RX BA6, static TX6/cache32, out-of-order pbuf limit4, main stack24576. PSRAM preference forces static TX; initial16 buffers and unlimited ooseq harmed internal memory, corrected before continuing.

Stage 2: increase receive window/mailboxes/RX buffers in separate measured configurations only after stage1 passes. SDK ooseq pbuf range is 0..12; proposed16/32 values are invalid. Check actual constraints and all sockets, including HTTP. Fresh build tags regenerate defaults; reconfigure alone does not override existing sdkconfig.

Stage 3: planned third512KiB JPEG slot only after prior gates. Current source remains **two slots**, with readonly lease/result/drain ownership. More slots absorb jitter rather than independently increasing throughput. Stage 4: individually measure decode/copy/overlay/publish improvements; synthetic bench excludes network/camera and cannot certify live FPS. Stage 5/independent work: recover camera streaming after LCD restart without requiring camera power-cycle, preserving identity and correct protocol cleanup.

Latest stopped30-minute full-screen sample had mean4.76081, minimum3.34 and sampled readP50=171ms: no NO_MEM/stream exit in that run, but final performance targets **failed**. Stable visual short tests do not establish all hardware/profile/OTA/NVS/reconnect gates. Raw samples are last-frame logs about every 5s, not every frame. Use [analyzer](../development/serial-log.md), retain each stage and rollback independently; do not declare the full plan complete.
