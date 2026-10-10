# Typed message contracts

**English** · [简体中文](../../design/module-message-contracts.md) · [日本語](../../ja/design/module-message-contracts.md)

All IDs have the APP_MESSAGE_ prefix. The table covers46 current/retired/reserved IDs, grouping only adjacent IDs with identical status. See [full field ledger](../../design/module-message-contracts.md) and `app_message.h`/handlers for exact payload fields. Maintenance Web does not use this normal bus.

Task-only requests reserve correlation, nonzero generation and absolute esp_timer deadline. Router separately tracks source/target endpoint lifetimes and stamps destination epoch. Request/reply mutex waits respect the original deadline; ordinary sends remain zero-wait. Transport ESP_OK still requires checking reply.result. Timeout does not roll back executed actions or revoke delivered leases.

Values are copied. BULK requires lease: readonly cannot mutate, writable only by assigned receiver. Send consumes its reference on both success/failure; receive/reply caller releases. Buffer/context remains alive until last callback, including partial fanout, cancellation and timeout. Control has priority over bulk. Network generation, safety generation, report epoch/ID, channel token and endpoint lifetime are different identifiers.

| ID(s) | Completion / ownership |
| --- | --- |
| `CAMERA_DISCOVER` | Copy up to4 DHCP candidates; no session creation. |
| `WIFI_RSSI` | Query selected peer or periodic2s event; missing RSSI−127. |
| `WIFI_CHANNEL_OPEN` | Return token/network generation; late open is closed. |
| `WIFI_CHANNEL_SEND` | Borrowed bulk lease; partial length/status returned, hold until release. |
| `WIFI_CHANNEL_RECEIVE` | Writable lease bounded by capacity; poll has no lease/consumption. |
| `WIFI_CHANNEL_CLOSE` | Cancel I/O, drain jobs and close before ACK; opening correlation supported. |
| `WIFI_CONFIG_GET` | Copied readonly configuration. |
| `WIFI_CONFIG_PREPARE / WIFI_CONFIG_COMMIT / WIFI_CONFIG_CANCEL / WIFI_CONFIG_RESULT` | Retired normal IDs: NOT_SUPPORTED; Web uses isolated facade. |
| `WIFI_SELECT_CAMERA` | Set RSSI tracking target; does not persist pairing. |
| `WIFI_NETWORK_CHANGED` | Control event retries current generation; not synchronous drain ACK. |
| `WIFI_STATUS` | Copied network state, periodic200ms event. |
| `CAMERA_START` | Preview/diagnostic flag; producer started, not connected. |
| `CAMERA_STOP` | UART flag admits stop; normal lifecycle ACK waits physical exit. |
| `CAMERA_FORGET` | Retired; identity removal only in maintenance storage. |
| `CAMERA_DISPLAY_SESSION` | Debug stop/resume reservation; timeout still requires same-token release. |
| `CAMERA_ACTION` | Safety generation/action admission; physical outcome via readback. |
| `CAMERA_SETTING_ADJUST / CAMERA_MENU_ACTION` | Semantic property/direction, desired-target admission. |
| `CAMERA_FRAME` | Readonly JPEG bulk lease/token/frame generation/read duration. |
| `UI_FRAME_RESULT` | Original frame generation/token, no lease; at most2 pending metadata. |
| `CAMERA_STATE` | Copied producer-generation state; not reliable event history. |
| `CAMERA_CAPABILITIES` | Safety generation separate; current publication is within properties lease. |
| `CAMERA_PROPERTIES` | Exact-size readonly view snapshot, no borrowed backend pointers. |
| `CAMERA_COMMAND_STATUS` | Control status0..5; failed publication does not undo action. |
| `CAMERA_STATUS` | Instant debug snapshot; may change during reply wait. |
| `UI_MENU_ACTION` | 500ms input menu deadline, semantic property reply; safe RELEASE_ALL. |
| `UI_STATE` | Unused reserved ID: NOT_SUPPORTED. |
| `UI_STATUS` | Copied UI state, not a persistent page guarantee. |
| `UI_PREFERENCES` | GET-only flag=false, no lease; old setters unsupported. |
| `UI_PROPERTY_STATUS` | Displayed actual/target/status by semantic property. |
| `DISPLAY_BENCH` | Debug tokenized worker,30s budget; completed event is separate. |
| `DISPLAY_FAULT` | Debug renderer injection, not canvas ownership. |
| `INPUT_STATE / INPUT_STATUS` | Input lifetime separate from source_epoch/report_id; copied state. |
| `INPUT_SELECT` | ATOM0/SIM1; release must finish before rearm. |
| `INPUT_ATOM_COMMAND` | Input-only pad kind; UART monitor/stats are copied values. |
| `INPUT_SIM_COMMAND` | Debug readonly sequence copied to4 jobs/8 completions; lifetime cancellation. |
| `SYSTEM_STATUS` | Mode/uptime/heap/restart snapshot. |
| `SYSTEM_RESTART` | Prepare/commit500–10000ms; lost ACK does not cancel scheduled restart. |
| `SYSTEM_FACTORY_RESET / SYSTEM_FACTORY_RESULT` | Retired normal IDs, Web-only factory. |
| `SYSTEM_CAMERA_SESSION` | Camera attempt-generation policy; no maintenance/canvas lease. |
| `SYSTEM_ENTER_NORMAL` | UI lifetime/page request closes trigger permanently for this boot. |

Handler source/range checks are domain contracts, not hotspot authentication. Capacity/lifecycle/parameter/unsupported/timeout errors do not imply transaction rollback. Cooperative fake-RTOS integration, direct symbol/source checks and builds cover software behavior; real SMP, blocked HTTP, camera effects, cache-off and stop timing remain separate. See [resources](module-resource-ownership.md) and [acceptance](../development/module-split-checklist.md).
