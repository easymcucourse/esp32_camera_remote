#pragma once
#include "app_message.h"

#define APP_CONSOLE_API_VERSION 1
#define APP_CONSOLE_PENDING_CAPACITY 16

typedef struct { unsigned control_depth, bulk_depth; } app_endpoint_config_t;
typedef struct {
    bool running, accepting, subscriptions_frozen;
    unsigned endpoints, pending, leases;
    uint32_t sent, rejected, expired, late_replies;
} app_console_status_t;

/* Startup-task lifecycle. Router owns queues/correlation only, never business
 * callbacks. UART gateway has an independent lifecycle. Partial allocation
 * failure unwinds; successful quiesce may restart with the existing queues. */
esp_err_t app_console_router_start(void);
esp_err_t app_console_endpoint_register(app_endpoint_t endpoint, const app_endpoint_config_t *config);
void app_console_endpoint_stop(app_endpoint_t endpoint);
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint);
esp_err_t app_console_subscribe(app_message_type_t type, app_endpoint_t subscriber);
void app_console_freeze_subscriptions(void);
/* Task-safe, nonblocking enqueue. Success transfers the lease to the router;
 * failure releases it too. REQUEST requires a live reservation from request()
 * or request_many(); correlations are router-owned. Never resend/copy a consumed lease. Control and
 * bulk queues are independent; endpoint_receive always drains control first. */
esp_err_t app_console_send(app_message_t *message);
esp_err_t app_console_receive(app_endpoint_t endpoint, app_message_t *message, uint32_t timeout_ms);
/* Blocking request, bounded by absolute deadline_us (mandatory) and generation
 * (nonzero). reply receives one owned message; caller releases it. Endpoint
 * stop/restart cancels pending work; late/type/generation-mismatched replies
 * are dropped with their leases. Do not block an endpoint's sole consumer on
 * a request to itself. All failures consume request's lease. */
esp_err_t app_console_request(app_message_t *request, app_message_t *reply);
/* Independent snapshots: admit/send every request before waiting. count1..16;
 * arrays must be disjoint and remain owned by the caller. Each result is its
 * transport status; an ESP_OK reply still carries the endpoint's result.
 * Deadlines/generations/lease ownership are identical to scalar requests.
 * Invalid array/count arguments return without taking request ownership. */
esp_err_t app_console_request_many(app_message_t *requests,app_message_t *replies,
    esp_err_t *results,size_t count);
/* Optional owner cancellation, checked outside router locks at <=25ms wait
 * slices. Predicate must not block; it may enqueue domain cancellation. A
 * cancelled request returns INVALID_STATE, without revoking consumer leases. */
typedef bool (*app_console_cancel_fn)(void *context);
esp_err_t app_console_request_cancelable(app_message_t *request, app_message_t *reply,
    app_console_cancel_fn cancelled, void *context);
/* Reply waits for the router mutex within the request's original deadline;
 * it never waits for queue capacity or starts a new deadline. */
esp_err_t app_console_reply(const app_message_t *request, app_message_t *reply);
/* Core first quiesces consumers; router closes admission, cancels waiters and
 * drains queues. In-use leases are not revoked under a renderer/TCP writer:
 * success also waits for the housekeeping task to exit. Timeout returns false
 * until consumers release leases and that task acknowledges; restart is denied
 * while either still owns resources. Queues remain for a later normal restart.
 * Never stop UART alone
 * to stop the router. */
bool app_console_router_quiesce(uint32_t timeout_ms);
void app_console_get_status(app_console_status_t *status);

/* Core/startup-task only. Independent UART0 owner; failure never stops router.
 * Quiesce retires UART endpoint first, cancels requests and cooperatively waits
 * for reader/driver cleanup. Timeout preserves worker/resources; retry waits.
 * Configure event subscriptions before Core freezes them. */
esp_err_t app_console_uart_start(void);
bool app_console_uart_quiesce(uint32_t timeout_ms);
