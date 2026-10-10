# Resource ownership

**English** · [简体中文](../../design/module-resource-ownership.md) · [日本語](../../ja/design/module-resource-ownership.md)

Resource lifetime, not just module names, determines safe shutdown. The [Chinese detailed task/queue ledger](../../design/module-resource-ownership.md) retains exact dated task priorities/stacks and paths. Current main boot stack is 24576 bytes; its older32768 entry is superseded. Values are configuration, not measured stack/SMP guarantees.

| Resource | Owner and lifetime |
| --- | --- |
| Router |16 request waiters/32 lease slots; endpoint control/bulk queues retain allocation to reboot. Stop closes admission, cancels owner requests and waits for last references. |
| Input |16 copied reports, opaque provider handle/epoch; release before unregister/stop or source rearm. |
| Camera | Single backend/PTP lifecycle; two 512KiB boot PSRAM JPEG slots remain until reboot, reusable only after lease and result completion. |
| UI | CPU1 endpoint/refresh renderer, one decoder/4096-byte work area boot allocated; pixels borrowed from back canvas. |
| Display | Board two 1024×600 RGB565 frames plus40960 internal bounce bytes; surface permits one writer and rejects stale owner/generation. Maintenance retains hardware for fixed screen. |
| Network | Two sole TCP lane owners, jobs/control queues and cancelable channels; free only when I/O/leases retire. AP object/config history survives for maintenance. |
| HTTP/OTA | Internal6144-byte HTTP stack;4096-byte PSRAM upload chunk; SDK OTA handle end/abort. SDK synchronous join has no strict project-wide bound. |
| Storage | wifi_ap primitive/storage mutex; Sony identity internal-stack worker; Web/Core phase-isolated UI/identity callbacks. No single atomic transaction across namespaces. |

Core closes normal admission while allowing releases/completions. Stop order: UART/Input/providers/bench → Camera physical → Camera endpoint → normal Wi-Fi config/bridge → preferences/UI endpoint → renderer → System/router. Failed prerequisites retain dependent owners and enter restart; no force deletion or normal recovery. Budgets1000/3000ms are per-step, not one whole-system deadline.

PTP transaction is owned by Camera producer through one embedded Sony client. Channel buffers stay alive until cancellation/completion, even after caller timeout. Surface validates object/lease/generation/address/size; publish/cancel returns exact ownership, recovery refuses an active writer. Lease callbacks may run in another holder's task.

Direct NVS writes in LCD are restricted to three storage primitives by source gate; backend reads can repair records. This lexical gate cannot prove aliases/function pointers/Flash SMP. Physical cache-off/blocked HTTP/RGB/full drain timing remains to be measured. See [messages](module-message-contracts.md), [graph](module-dependency-graph.md) and [acceptance](../development/module-split-checklist.md).
