#include "sony_codes.h"
#include "ptp_codes.h"
#include "camera_pair.h"
#include "camera_console.h"
#include "camera_link.h"
#include "camera_identity.h"
#include "wifi_ap.h"
#include "ptpip_packet.h"
#include "ptpip_transport.h"
#include "ptp_session.h"
#include "ptp_dataset.h"
#include "sony_props.h"
#include "sony_ext.h"
#include "liveview_pipeline.h"
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "board_7b.h"
#include "camera_settings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"

#define CAMERA_PORT PTPIP_PORT
static const char *TAG = "camera_pair";
static atomic_bool busy = false;
static atomic_bool stop_requested = false;
static QueueHandle_t mode_requests;
static atomic_bool mode_control_ready;
static sony_mode_state_t mode;
static QueueHandle_t focus_requests;
static atomic_bool focus_eligible;
static atomic_uint focus_epoch;
static atomic_uint focus_updated_ms;
typedef struct { int direction; unsigned epoch; uint32_t queued_ms; } focus_request_t;

static uint32_t focus_now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

void camera_focus_cancel(void)
{
    atomic_fetch_add(&focus_epoch, 1);
    if (focus_requests) xQueueReset(focus_requests);
}

bool camera_focus_ready(void)
{
    return atomic_load(&mode_control_ready) && !atomic_load(&stop_requested) &&
        atomic_load(&focus_eligible) && !board_7b_settings_mode() &&
        (uint32_t)(focus_now_ms() - atomic_load(&focus_updated_ms)) < 6000;
}

void camera_focus_step(int direction)
{
    if (!focus_requests || !camera_focus_ready() || (direction != 1 && direction != -1)) return;
    focus_request_t request = {direction, atomic_load(&focus_epoch), focus_now_ms()};
    xQueueOverwrite(focus_requests, &request);
}

static void parse_focus_caps(const uint8_t *data, size_t size)
{
    sony_focus_caps_t caps;
    bool valid = sony_parse_focus_caps(data, size, &caps);
    bool eligible = valid && caps.focus_known && caps.focus_mode == SONY_FOCUS_MODE_MANUAL &&
        caps.zoom_known && caps.zoom_enabled == 0;
    atomic_store(&focus_eligible, eligible);
    atomic_store(&focus_updated_ms, focus_now_ms());
    if (!eligible) camera_focus_cancel();
    static uint32_t last_status = UINT32_MAX;
    uint32_t status = (uint32_t)valid << 24 | (uint32_t)caps.focus_known << 25 |
        (uint32_t)caps.zoom_known << 26 | (uint32_t)caps.zoom_enabled << 16 | caps.focus_mode;
    if (last_status != status) {
        last_status = status;
        ESP_LOGI(TAG, "X/Y MF %s: dataset=%s focus=0x%04x (%s) zoom=%u (%s)",
                 eligible ? "enabled" : "disabled", valid ? "valid" : "invalid",
                 caps.focus_mode, caps.focus_known ? "known" : "unknown",
                 caps.zoom_enabled, caps.zoom_known ? "known" : "unknown");
    }
}

void camera_mode_step(int direction)
{
    if (direction != -1 && direction != 1) return;
    if (!mode_requests || !atomic_load(&mode_control_ready)) {
        ESP_LOGW(TAG, "Mode change ignored: camera session not ready");
        return;
    }
    if (xQueueSend(mode_requests, &direction, 0) != pdTRUE)
        ESP_LOGW(TAG, "Mode change queue full");
}

// The producer updates this screen only while no JPEG worker owns the LCD.
static void connection_status(const char *status)
{
    esp_err_t err = board_7b_show_connection(status);
    if (err != ESP_OK) ESP_LOGW(TAG, "Connection screen: %s", esp_err_to_name(err));
}

// Keep headroom for larger live-view JPEG objects in PSRAM.
#define OBJECT_CAPACITY (1024 * 1024)

static camera_identity_t identity; /* camera task only */
static wifi_ap_client_t target;
static uint8_t pending_camera_guid[16];
static bool pairing_required;
static bool link_had_frame;

typedef struct {
    SemaphoreHandle_t done;
    bool confirm;
    bool result;
} identity_work_t;

static void identity_worker(void *argument)
{
    identity_work_t *work = argument;
    work->result = work->confirm ?
        camera_identity_confirm(&identity, target.mac, pending_camera_guid) :
        camera_identity_load(&identity);
    xSemaphoreGive(work->done);
    vTaskDelete(NULL);
}

// NVS may disable caches. Use a small internal worker stack while keeping
// the controller's font-rendering stack in PSRAM.
static bool identity_operation(bool confirm)
{
    identity_work_t work = {.done = xSemaphoreCreateBinary(), .confirm = confirm};
    if (!work.done) return false;
    bool started = xTaskCreate(identity_worker, "camera_nvs", 4096, &work, 4, NULL) == pdPASS;
    if (started) xSemaphoreTake(work.done, portMAX_DELAY);
    vSemaphoreDelete(work.done);
    return started && work.result;
}

static bool cancelled(void *context)
{
    (void)context;
    return atomic_load(&stop_requested);
}
static bool wait_cancel(unsigned ms)
{
    while (ms && !atomic_load(&stop_requested)) {
        unsigned slice = ms > 100 ? 100 : ms;
        vTaskDelay(pdMS_TO_TICKS(slice)); ms -= slice;
    }
    return !atomic_load(&stop_requested);
}
/* Probe only DHCP leases of associated AP clients; never scan the subnet. */
static int discover_camera(bool *connect_failed)
{
    *connect_failed = false;
    wifi_ap_client_t clients[WIFI_AP_CLIENT_CAPACITY];
    size_t count = 0;
    if (!wifi_ap_get_clients(clients, WIFI_AP_CLIENT_CAPACITY, &count)) return -1;
    int sockets[WIFI_AP_CLIENT_CAPACITY];
    for (unsigned i = 0; i < WIFI_AP_CLIENT_CAPACITY; ++i) sockets[i] = -1;
    uint32_t reachable = 0;
    for (size_t i = 0; i < count && !atomic_load(&stop_requested); ++i) {
        if (!camera_candidate_allowed(identity.paired, identity.peer, clients[i].mac)) continue;
        *connect_failed = true;
        sockets[i] = ptpip_connect_timeout(clients[i].ip, CAMERA_PORT, 800);
        if (sockets[i] >= 0) reachable |= 1u << i;
    }
    int selected = camera_select_candidate(reachable), fd = -1;
    if (atomic_load(&stop_requested)) selected = -1;
    if (selected >= 0) {
        *connect_failed = false;
        target = clients[selected]; fd = sockets[selected]; sockets[selected] = -1;
        wifi_ap_select_camera(target.mac);
        board_7b_set_wifi_rssi(target.rssi);
        ESP_LOGI(TAG, "CAMERA DISCOVERED: IP=%s stored_peer=%d", target.ip, identity.paired);
    } else if (selected == -2) {
        *connect_failed = false;
        connection_status("Multiple cameras - connect only one");
    }
    for (unsigned i = 0; i < WIFI_AP_CLIENT_CAPACITY; ++i) if (sockets[i] >= 0) close(sockets[i]);
    return fd;
}

static bool parse_device_info(const uint8_t *data, size_t size)
{
    char model[24], firmware[24];
    if (!ptp_parse_device_info(data, size, model, firmware)) {
        ESP_LOGE(TAG, "Invalid GetDeviceInfo dataset"); return false;
    }
    board_7b_set_camera_info(model, firmware);
    ESP_LOGI(TAG, "DeviceInfo: model=%s firmware=%s", model, firmware);
    return true;
}

static void show_property(void *context, uint16_t code, uint32_t value)
{
    (void)context;
    if (code == SONY_DPC_EXPOSURE_PROGRAM) {
        board_7b_set_exposure_mode(value);
        ESP_LOGI(TAG, "Exposure mode=0x%08lx", (unsigned long)value);
    } else {
        board_7b_set_camera_property(code, value);
    }
}

/* Missing extra properties in a valid snapshot must not retain old values. */
static void collect_scalar_property(void *context, uint16_t code, uint32_t value)
{
    uint32_t *extra = context;
    for (unsigned i = 0; i < CAMERA_EXTRA_COUNT; ++i) {
        if (code == camera_extra_codes[i]) { extra[i] = value; return; }
    }
    show_property(NULL, code, value);
}

static void publish_scalar_properties(const uint8_t *data, size_t size)
{
    uint32_t extra[CAMERA_EXTRA_COUNT];
    for (unsigned i = 0; i < CAMERA_EXTRA_COUNT; ++i) extra[i] = UINT32_MAX;
    if (!sony_parse_scalar_properties(data, size, collect_scalar_property, extra)) return;
    for (unsigned i = 0; i < CAMERA_EXTRA_COUNT; ++i)
        board_7b_set_camera_property(camera_extra_codes[i], extra[i]);
}

static bool initialize_sony(int command, uint32_t *next_transaction, uint8_t *buffer, size_t capacity)
{
    // Exact read/initialization sequence from the ZV-E10 Remote capture.
    const struct { uint16_t opcode; unsigned count; uint32_t params[3]; } steps[] = {
        {SONY_OC_SDIO_CONNECT, 3, {1, 0, 0}},
        {SONY_OC_SDIO_CONNECT, 3, {2, 0, 0}},
        {PTP_OC_GET_DEVICE_INFO, 1, {0}},
        {SONY_OC_SDIO_GET_EXT_DEVICE_INFO, 1, {300}},
        {SONY_OC_SDIO_CONNECT, 3, {3, 0, 0}},
        {SONY_OC_SDIO_GET_EXT_DEVICE_INFO, 1, {300}},
        {SONY_OC_GET_ALL_EXT_PROP_INFO, 1, {0}},
        {PTP_OC_GET_OBJECT_INFO, 1, {SONY_LIVEVIEW_HANDLE}},
    };
    bool ok = true;
    size_t size = 0;
    for (unsigned i = 0; i < sizeof(steps) / sizeof(steps[0]) && ok; ++i) {
        uint16_t response = 0;
        ptp_data_status_t result = ptp_request_data_result(command, steps[i].opcode,
                          (*next_transaction)++, steps[i].params, steps[i].count,
                          buffer, capacity, &size, &response);
        ok = result == PTP_DATA_OK;
        // The preview handle may not exist yet. A complete refusal leaves the
        // session usable; let GetObject retry until the camera has a frame.
        if (steps[i].opcode == PTP_OC_GET_OBJECT_INFO && result == PTP_DATA_REFUSED &&
            (response == 0x2009 || response == PTP_RC_ACCESS_DENIED)) ok = true;
        if (ok && steps[i].opcode == PTP_OC_GET_DEVICE_INFO)
            ok = parse_device_info(buffer, size);
        if (ok && steps[i].opcode == SONY_OC_GET_ALL_EXT_PROP_INFO) {
            sony_parse_properties(buffer, size, &mode, show_property, NULL);
            publish_scalar_properties(buffer, size);
            parse_focus_caps(buffer, size);
        }
    }
    return ok && !atomic_load(&stop_requested) && identity_operation(true);
}

static bool read_liveview(int command, int event, uint32_t *next_transaction)
{
    atomic_store(&mode_control_ready, false);
    atomic_store(&focus_eligible, false);
    camera_focus_cancel();
    mode.count = 0;
    if (mode_requests) xQueueReset(mode_requests);
    jpeg_pipeline_t pipeline = {0};
    atomic_init(&pipeline.failed, false);
    bool worker_started = false;
    bool ok = false;
    for (int i = 0; i < 2; ++i) {
        pipeline.data[i] = heap_caps_malloc(OBJECT_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!pipeline.data[i]) goto cleanup;
    }
    pipeline.free_slots = xQueueCreate(2, sizeof(int));
    pipeline.ready = xQueueCreate(2, sizeof(jpeg_job_t));
    pipeline.done = xSemaphoreCreateBinary();
    if (!pipeline.free_slots || !pipeline.ready || !pipeline.done) goto cleanup;
    ok = initialize_sony(command, next_transaction, pipeline.data[0], OBJECT_CAPACITY);
    size_t size = 0;
    if (!ok) goto cleanup;
    int64_t next_exposure_read = esp_timer_get_time() + 5000000;
    connection_status("Connected - waiting for preview...");
    for (int i = 0; i < 2; ++i) xQueueSend(pipeline.free_slots, &i, 0);
    // The decoder task does not call flash-disabled code or DMA from its stack,
    // so its large stack can live in PSRAM and leave internal RAM for TCP/Wi-Fi.
    // FreeType's grayscale rasterizer uses a 16 KiB stack-local work pool.
    if (xTaskCreatePinnedToCoreWithCaps(jpeg_decode_task, "jpeg_decode", 32768,
                                        &pipeline, 4, NULL, 1,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ok = false;
        goto cleanup;
    }
    worker_started = true;
    atomic_store(&mode_control_ready, true);
    const uint32_t handle = SONY_LIVEVIEW_HANDLE;
    ESP_LOGI(TAG, "LIVEVIEW RUNNING: RX core=%d, decode core=1, 2x1MiB pipeline; s stops, j starts", xPortGetCoreID());
    unsigned refused_frames = 0;
    while (ok && !atomic_load(&stop_requested) && !atomic_load(&pipeline.failed)) {
        int slot;
        if (xQueueReceive(pipeline.free_slots, &slot, pdMS_TO_TICKS(100)) != pdTRUE) continue;
        if (atomic_load(&stop_requested) || atomic_load(&pipeline.failed)) break;
        bool properties_changed;
        ok = ptp_drain_events_changed(event, &properties_changed);
        if (!ok) break;
        int direction;
        bool mode_changed = mode_requests && xQueueReceive(mode_requests, &direction, 0) == pdTRUE;
        if (mode_changed || properties_changed || esp_timer_get_time() >= next_exposure_read) {
            const uint32_t property_group = 0;
            ok = ptp_request_data(command, SONY_OC_GET_ALL_EXT_PROP_INFO, (*next_transaction)++, &property_group, 1,
                              pipeline.data[slot], OBJECT_CAPACITY, &size);
            if (!ok) break;
            sony_parse_properties(pipeline.data[slot], size, &mode, show_property, NULL);
            publish_scalar_properties(pipeline.data[slot], size);
            parse_focus_caps(pipeline.data[slot], size);
            next_exposure_read = esp_timer_get_time() + 5000000;
            if (mode_changed) {
                unsigned index = 0;
                while (index < mode.count && mode.values[index] != mode.current) ++index;
                if (!mode.writable || mode.count < 2 || index == mode.count) {
                    ESP_LOGW(TAG, "Mode change unavailable: writable=%d choices=%u", mode.writable, mode.count);
                } else {
                    unsigned next = direction > 0 ? (index + 1) % mode.count :
                        (index + mode.count - 1) % mode.count;
                    uint32_t target = mode.values[next];
                    bool accepted;
                    ok = sony_set_exposure_mode(command, (*next_transaction)++, target, &accepted);
                    if (!ok) break;
                    if (accepted) {
                        ok = ptp_request_data(command, SONY_OC_GET_ALL_EXT_PROP_INFO, (*next_transaction)++, &property_group, 1,
                                          pipeline.data[slot], OBJECT_CAPACITY, &size);
                        if (!ok) break;
                        sony_parse_properties(pipeline.data[slot], size, &mode, show_property, NULL);
                        publish_scalar_properties(pipeline.data[slot], size);
                        parse_focus_caps(pipeline.data[slot], size);
                        ESP_LOGI(TAG, "Mode readback: requested=0x%08lx actual=0x%08lx",
                                 (unsigned long)target, (unsigned long)mode.current);
                    }
                }
            }
        }
        focus_request_t focus;
        if (xQueueReceive(focus_requests, &focus, 0) == pdTRUE && camera_focus_ready() &&
            focus.epoch == atomic_load(&focus_epoch) &&
            (uint32_t)(focus_now_ms() - focus.queued_ms) < 500) {
            // Recheck body/menu changes before every step; never write using a stale MF value.
            const uint32_t group = 0;
            ok = ptp_request_data(command, SONY_OC_GET_ALL_EXT_PROP_INFO, (*next_transaction)++,
                                  &group, 1, pipeline.data[slot], OBJECT_CAPACITY, &size);
            if (!ok) break;
            sony_parse_properties(pipeline.data[slot], size, &mode, show_property, NULL);
            publish_scalar_properties(pipeline.data[slot], size);
            parse_focus_caps(pipeline.data[slot], size);
            next_exposure_read = esp_timer_get_time() + 5000000;
            if (camera_focus_ready() && focus.epoch == atomic_load(&focus_epoch) &&
                (uint32_t)(focus_now_ms() - focus.queued_ms) < 1000) {
                bool accepted;
                ok = sony_manual_focus_step(command, (*next_transaction)++, focus.direction, &accepted);
                if (!ok) break;
                if (!accepted) {
                    atomic_store(&focus_eligible, false);
                    camera_focus_cancel();
                    ESP_LOGW(TAG, "Manual focus rejected; waiting for fresh properties and a new press");
                }
            }
        }
        int64_t start = esp_timer_get_time();
        uint16_t response;
        ptp_data_status_t status = ptp_request_data_result(command, PTP_OC_GET_OBJECT,
            (*next_transaction)++, &handle, 1, pipeline.data[slot], OBJECT_CAPACITY, &size, &response);
        if (status == PTP_DATA_REFUSED && response == PTP_RC_ACCESS_DENIED && ++refused_frames <= 50) {
            /* Response is fully consumed: return the slot and reuse this session. */
            xQueueSend(pipeline.free_slots, &slot, 0);
            if (!wait_cancel(100)) break;
            continue;
        }
        ok = status == PTP_DATA_OK && size > 0;
        if (!ok) { ESP_LOGW(TAG, "GetObject failed: status=%d response=0x%04x refusals=%u", status, response, refused_frames); break; }
        refused_frames = 0;
        link_had_frame = true;
        jpeg_job_t job = {.slot = slot, .size = size, .read_ms = (esp_timer_get_time() - start) / 1000};
        xQueueSend(pipeline.ready, &job, portMAX_DELAY);
    }
cleanup:
    atomic_store(&mode_control_ready, false);
    atomic_store(&focus_eligible, false);
    camera_focus_cancel();
    if (mode_requests) xQueueReset(mode_requests);
    if (worker_started) {
        jpeg_job_t end = {.slot = -1};
        xQueueSend(pipeline.ready, &end, portMAX_DELAY);
        xSemaphoreTake(pipeline.done, portMAX_DELAY);
        if (atomic_load(&pipeline.failed)) ok = false;
    }
    if (pipeline.ready) vQueueDelete(pipeline.ready);
    if (pipeline.free_slots) vQueueDelete(pipeline.free_slots);
    if (pipeline.done) vSemaphoreDelete(pipeline.done);
    for (int i = 0; i < 2; ++i) heap_caps_free(pipeline.data[i]);
    return ok;
}

static int initialization_exchange(int fd, uint8_t *packet, size_t request_length, size_t capacity)
{
    if (!ptpip_transaction_begin(fd)) return -1;
    int result = -1;
    if (ptpip_transfer(fd, packet, request_length, true)) {
        for (unsigned i = 0; i < 16; ++i) {
            result = ptp_receive_packet(fd, packet, capacity);
            if (result != 8 || get32(packet + 4) != PTPIP_PROBE_REQUEST) break;
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!ptpip_transfer(fd, packet, 8, true)) { result = -1; break; }
            result = -1;
        }
    }
    ptpip_transaction_end(); return result;
}

static bool handshake(int command, bool jpeg)
{
    uint8_t packet[512] = {0};
    const char name[] = "ESP32-Camera-Remote";
    uint32_t length = 24 + sizeof(name) * 2 + 4;
    put32(packet, length);
    put32(packet + 4, PTPIP_INIT_COMMAND_REQUEST);
    memcpy(packet + 8, identity.guid, 16);
    for (size_t i = 0; i < sizeof(name); ++i) packet[24 + i * 2] = name[i];
    put32(packet + length - 4, PTPIP_PROTOCOL_VERSION);
    if (!ptpip_timeout_set(command, identity.paired ? 10 : 120)) return false;
    ESP_LOGI(TAG, "Sending InitCommandRequest as %s; confirm on camera if prompted", name);
    connection_status(identity.paired ? "Reconnecting to paired camera..." : "Pairing - confirm on camera...");
    int n = initialization_exchange(command, packet, length, sizeof(packet));
    if (n >= 12 && get32(packet + 4) == PTPIP_INIT_FAIL) {
        ESP_LOGE(TAG, "InitFail reason=%lu", (unsigned long)get32(packet + 8));
        pairing_required = true;
        return false;
    }
    if (n < 34 || (n - 32) % 2 || get32(packet + 4) != PTPIP_INIT_COMMAND_ACK ||
        get16(packet + n - 6) != 0 || get32(packet + n - 4) != PTPIP_PROTOCOL_VERSION) return false;
    memcpy(pending_camera_guid, packet + 12, 16);
    if (identity.paired && memcmp(pending_camera_guid, identity.peer + 6, 16)) {
        ESP_LOGE(TAG, "Camera identity changed; stop then u to pair a replacement");
        pairing_required = true; return false;
    }
    uint32_t connection = get32(packet + 8);
    char camera_name[64] = {0};
    for (int i = 0; i < sizeof(camera_name) - 1 && 28 + i * 2 + 1 < n - 4; ++i) {
        camera_name[i] = packet[28 + i * 2];
        if (!camera_name[i]) break;
    }
    ESP_LOGI(TAG, "INIT ACK: camera=%s connection=%lu", camera_name, (unsigned long)connection);
    board_7b_set_camera_info(camera_name, NULL);
    int event = ptpip_connect(target.ip, CAMERA_PORT);
    if (event < 0) return false;
    put32(packet, 12);
    put32(packet + 4, PTPIP_INIT_EVENT_REQUEST);
    put32(packet + 8, connection);
    bool ok = initialization_exchange(event, packet, 12, sizeof(packet)) == 8 &&
              get32(packet + 4) == PTPIP_INIT_EVENT_ACK;
    if (ok) {
        ESP_LOGI(TAG, "EVENT ACK received");
        ok = ptpip_timeout_set(command, 5) && ptp_operation(command, PTP_OC_OPEN_SESSION, 2, true);
        if (ok) {
            ESP_LOGI(TAG, "SESSION VERIFIED: OpenSession accepted");
            connection_status("Session connected - preparing preview...");
            if (jpeg) {
                uint32_t next_transaction = 3;
                ok = read_liveview(command, event, &next_transaction);
                // On parsing/network failure, close sockets instead of sending into
                // an unconsumed data phase. On success, end the session normally.
                if (ok && !atomic_load(&stop_requested)) ok = ptp_operation(command, PTP_OC_CLOSE_SESSION, next_transaction, false);
            } else {
                uint8_t *buffer = heap_caps_malloc(OBJECT_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                uint32_t next_transaction = 3;
                ok = buffer && initialize_sony(command, &next_transaction, buffer, OBJECT_CAPACITY);
                heap_caps_free(buffer);
                if (ok && !atomic_load(&stop_requested)) ok = ptp_operation(command, PTP_OC_CLOSE_SESSION, next_transaction, false);
            }
        }
    }
    close(event);
    return ok;
}

static void pair_task(void *unused)
{
    bool jpeg = (uintptr_t)unused != 0;
    bool ok = false;
    unsigned failures = 0;
    if (!identity_operation(false)) { connection_status("Pairing data invalid - stop then u"); goto finished; }
    while (!atomic_load(&stop_requested)) {
        connection_status(identity.paired ? "Waiting for paired camera..." : "Wi-Fi ready - discovering camera...");
        int fd = -1;
        while (fd < 0 && !atomic_load(&stop_requested)) {
            bool connect_failed;
            fd = discover_camera(&connect_failed);
            if (fd < 0) {
                unsigned delay = 1;
                if (connect_failed) {
                    delay = camera_retry_delay(failures);
                    if (failures < 5) ++failures;
                    ESP_LOGW(TAG, "Camera service unavailable; retry in %u seconds", delay);
                }
                if (!wait_cancel(delay * 1000)) break;
            }
        }
        if (fd < 0) break;
        pairing_required = false;
        link_had_frame = false;
        ok = handshake(fd, jpeg);
        close(fd);
        if (atomic_load(&stop_requested)) break;
        if (pairing_required) {
            connection_status("Pairing rejected - confirm camera then j/p");
            break; /* Explicit user action is required; no automatic InitFail loop. */
        }
        if (ok && !jpeg) {
            ESP_LOGI(TAG, "Sony session confirmed; verifying reconnect with stored identity");
            if (!wait_cancel(300)) break;
            bool connect_failed;
            fd = discover_camera(&connect_failed);
            ok = fd >= 0 && handshake(fd, false);
            if (fd >= 0) close(fd);
            connection_status(ok ? "Pairing verified - press j for preview" : "Reconnect failed - press p to retry");
            break;
        }
        if (!jpeg) { connection_status("Pairing failed - press p to retry"); break; }
        if (link_had_frame) failures = 0;
        unsigned delay = camera_retry_delay(failures);
        if (failures < 5) ++failures;
        ESP_LOGW(TAG, "Camera disconnected: IO=%d; retry in %u seconds", ptpip_last_status(), delay);
        char message[80]; snprintf(message, sizeof(message), "Disconnected - retrying in %u seconds...", delay);
        connection_status(message);
        if (!wait_cancel(delay * 1000)) break;
    }
finished:
    atomic_store(&mode_control_ready, false);
    atomic_store(&focus_eligible, false);
    camera_focus_cancel();
    if (mode_requests) xQueueReset(mode_requests);
    if (atomic_load(&stop_requested)) ESP_LOGI(TAG, "LIVEVIEW STOPPED: sockets closed; last frame retained");
    ESP_LOGI(TAG, "Camera task finished");
    atomic_store(&busy, false);
    vTaskDeleteWithCaps(NULL);
}

static void start_request(bool jpeg)
{
    if (atomic_exchange(&busy, true)) {
        ESP_LOGI(TAG, "Camera request already active");
        return;
    }
    atomic_store(&stop_requested, false);
    // NVS writes use a separate internal worker; rendering needs this large stack.
    if (xTaskCreatePinnedToCoreWithCaps(pair_task, "camera_pair", 32768,
                                        (void *)(uintptr_t)jpeg, 4, NULL, 0,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGE(TAG, "Cannot allocate pairing task");
        connection_status("Cannot start camera task - press j");
        atomic_store(&busy, false);
    }
}

void camera_pair_start(void) { start_request(false); }
void camera_jpeg_start(void) { start_request(true); }

// Console requests stop; the socket owner cancels waits and closes its sockets.
void camera_stop_request(void)
{
    atomic_store(&stop_requested, true);
    camera_focus_cancel();
    ESP_LOGI(TAG, "Stop requested; cancelling network waits");
}

void camera_forget_pairing(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&busy, &expected, true)) {
        ESP_LOGW(TAG, "Pair reset ignored: send s and wait for Camera task finished first"); return;
    }
    if (camera_identity_forget()) {
        wifi_ap_select_camera(NULL);
        ESP_LOGI(TAG, "Pairing identity cleared; press j/p and confirm on camera");
    } else ESP_LOGE(TAG, "Pair reset failed");
    atomic_store(&busy, false);
}

void camera_pair_console_init(void)
{
    mode_requests = xQueueCreate(32, sizeof(int));
    ESP_ERROR_CHECK(mode_requests ? ESP_OK : ESP_ERR_NO_MEM);
    focus_requests = xQueueCreate(1, sizeof(focus_request_t));
    ESP_ERROR_CHECK(focus_requests ? ESP_OK : ESP_ERR_NO_MEM);
    ptpip_set_cancel(cancelled, NULL);
    camera_console_init();
}
