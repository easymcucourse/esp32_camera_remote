#include "sony_codes.h"
#include "ptp_codes.h"
#include "camera_pair.h"
#include "maint_mode.h"
#include "setting_control.h"
#include "camera_menu.h"
#include "camera_actions.h"
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
static atomic_int last_io;
static char debug_phase[96] = "idle";
static portMUX_TYPE debug_mux = portMUX_INITIALIZER_UNLOCKED;
static bool maintenance_gate;
static portMUX_TYPE lifecycle_mux = portMUX_INITIALIZER_UNLOCKED;
static atomic_bool stop_requested = false;
static atomic_bool releasing_controls;
static atomic_uint stop_requested_ms;
static atomic_int mode_steps, focus_mode_steps;
static setting_control_t mode_control;
static camera_menu_t menu;
static atomic_int menu_steps[CAMERA_MENU_COUNT];
static atomic_bool mode_control_ready;
static sony_mode_state_t mode;
static QueueHandle_t focus_requests;
static portMUX_TYPE controls_mux = portMUX_INITIALIZER_UNLOCKED;
static camera_actions_t controls;
static gamepad_caps_t published_caps;
static atomic_uint input_generation;
static bool record_executing;
static bool record_wait; /* Camera owner only. */
static bool record_target;
static uint32_t record_deadline;
static atomic_bool focus_eligible;
static atomic_uint focus_epoch;
static atomic_uint focus_updated_ms;
typedef struct { int direction; unsigned epoch; uint32_t queued_ms, generation; } focus_request_t;

static uint32_t focus_now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

void camera_focus_cancel(void)
{
    atomic_fetch_add(&focus_epoch, 1);
    if (focus_requests) xQueueReset(focus_requests);
}

bool camera_focus_ready(void)
{
    return atomic_load(&mode_control_ready) && !atomic_load(&stop_requested) &&
        atomic_load(&focus_eligible) &&
        (uint32_t)(focus_now_ms() - atomic_load(&focus_updated_ms)) < 6000;
}

void camera_gamepad_caps(gamepad_caps_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&controls_mux);
    *out = published_caps;
    portEXIT_CRITICAL(&controls_mux);
    if ((uint32_t)(focus_now_ms() - atomic_load(&focus_updated_ms)) >= 6000) {
        out->mf_known = out->zoom_known = out->recording_known = false;
    }
}
static void focus_step_for_generation(int direction, uint32_t generation)
{
    if (!focus_requests || !camera_focus_ready() || (direction != 1 && direction != -1)) return;
    focus_request_t request = {direction, atomic_load(&focus_epoch), focus_now_ms(), generation};
    xQueueOverwrite(focus_requests, &request);
}
void camera_focus_step(int direction)
{
    focus_step_for_generation(direction, atomic_load(&input_generation));
}
static void controls_session(bool open)
{
    portENTER_CRITICAL(&controls_mux);
    camera_actions_session(&controls, open);
    published_caps.session = open;
    published_caps.generation = controls.generation;
    published_caps.record_pending = false;
    atomic_store(&input_generation, controls.generation);
    portEXIT_CRITICAL(&controls_mux);
    record_wait = record_executing = false;
}
static void parse_focus_caps(const uint8_t *data, size_t size)
{
    sony_focus_caps_t sony;
    bool valid = sony_parse_focus_caps(data, size, &sony);
    gamepad_caps_t caps = {
        .mf_known = sony.focus_known, .mf = sony.focus_mode == SONY_FOCUS_MODE_MANUAL,
        .zoom_known = sony.zoom_known, .zoom_enabled = sony.zoom_enabled == 1,
        .recording_known = sony.recording_known, .recording = sony.recording,
    };
    if (record_wait && caps.recording_known && caps.recording == record_target) {
        record_wait = false;
        board_7b_set_command_status(SONY_DPC_MOVIE_RECORD, SETTING_APPLIED);
    } else if (record_wait && (int32_t)(focus_now_ms() - record_deadline) >= 0) {
        record_wait = false;
        board_7b_set_command_status(SONY_DPC_MOVIE_RECORD, SETTING_TIMEOUT);
    }
    portENTER_CRITICAL(&controls_mux);
    caps.session = controls.session;
    caps.generation = controls.generation;
    /* No confirmed PZ lens field is present in the current captures. */
    caps.lens = published_caps.lens;
    caps.record_pending = record_wait || record_executing || camera_actions_record_queued(&controls);
    published_caps = caps;
    portEXIT_CRITICAL(&controls_mux);
    bool eligible = valid && caps.mf_known && caps.mf && caps.lens == PAD_LENS_NON_POWER_ZOOM;
    atomic_store(&focus_eligible, eligible);
    atomic_store(&focus_updated_ms, focus_now_ms());
    if (!eligible) camera_focus_cancel();
    board_7b_set_recording_status(caps.recording_known, caps.recording);
}

static void enqueue_setting_step(atomic_int *steps, int direction)
{
    if ((direction != -1 && direction != 1) || !atomic_load(&mode_control_ready) ||
        atomic_load(&stop_requested)) return;
    /* Bound pathological input without building a per-command queue. */
    int current = atomic_load(steps);
    while (current > -1000000 && current < 1000000 &&
           !atomic_compare_exchange_weak(steps, &current, current + direction)) {}
}
void camera_mode_step(int direction) { enqueue_setting_step(&mode_steps, direction); }
void camera_focus_mode_step(int direction)
{
    camera_focus_cancel();
    enqueue_setting_step(&focus_mode_steps, direction);
}
static void reset_setting_inputs(void)
{
    atomic_store(&mode_steps, 0);
    atomic_store(&focus_mode_steps, 0);
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) atomic_store(&menu_steps[i], 0);
}

bool camera_gamepad_action(pad_action_t action)
{
    if (action.type == PAD_ACTION_MF_CANCEL) { camera_focus_cancel(); return true; }
    portENTER_CRITICAL(&controls_mux);
    bool current = controls.session && controls.generation == action.generation;
    if (current && action.type == PAD_ACTION_MODE_NEXT) enqueue_setting_step(&mode_steps, 1);
    if (current && action.type == PAD_ACTION_FOCUS_MODE_NEXT) enqueue_setting_step(&focus_mode_steps, 1);
    if (current && action.type == PAD_ACTION_MENU_STEP && board_7b_settings_mode()) {
        unsigned selected = board_7b_menu_selected();
        if (selected < CAMERA_MENU_COUNT) enqueue_setting_step(&menu_steps[selected], action.value);
    }
    portEXIT_CRITICAL(&controls_mux);
    if (!current) return false;
    if (action.type == PAD_ACTION_MODE_NEXT || action.type == PAD_ACTION_MENU_STEP) return true;
    if (action.type == PAD_ACTION_FOCUS_MODE_NEXT) { camera_focus_cancel(); return true; }
    if (action.type == PAD_ACTION_MF_STEP) { focus_step_for_generation(action.value, action.generation); return true; }
    if (action.type == PAD_ACTION_RECORD_UNAVAILABLE) {
        ESP_LOGW(TAG, "Record ignored: state unknown or pending confirmation");
        return true;
    }
    bool release = action.type == PAD_ACTION_RELEASE_ALL ||
        ((action.type == PAD_ACTION_S1 || action.type == PAD_ACTION_S2 || action.type == PAD_ACTION_ZOOM) && !action.value);
    if (!release && atomic_load(&stop_requested)) return false;
    portENTER_CRITICAL(&controls_mux);
    bool record_busy = action.type == PAD_ACTION_RECORD && published_caps.record_pending;
    bool ok = !record_busy && camera_actions_submit(&controls, action, focus_now_ms());
    if (ok && action.type == PAD_ACTION_RECORD) published_caps.record_pending = true;
    published_caps.generation = controls.generation;
    atomic_store(&input_generation, controls.generation);
    if (release && action.type == PAD_ACTION_RELEASE_ALL) published_caps.record_pending = false;
    portEXIT_CRITICAL(&controls_mux);
    if (action.type == PAD_ACTION_RELEASE_ALL || !ok) {
        camera_focus_cancel();
        reset_setting_inputs();
    }
    return ok;
}

// The producer updates this screen only while no JPEG worker owns the LCD.
static void connection_status(const char *status)
{
    portENTER_CRITICAL(&debug_mux); snprintf(debug_phase, sizeof(debug_phase), "%s", status); portEXIT_CRITICAL(&debug_mux);
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
    if (!atomic_load(&stop_requested)) return false;
    return !atomic_load(&releasing_controls) ||
        (uint32_t)(focus_now_ms() - atomic_load(&stop_requested_ms)) >= 900;
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

static void publish_setting_status(void)
{
    board_7b_set_command_status(SONY_DPC_EXPOSURE_PROGRAM, (unsigned)mode_control.status);
    board_7b_set_command_status(SONY_DPC_FOCUS_MODE, (unsigned)camera_menu_status(&menu, MENU_FOCUS));
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        uint32_t target = 0; bool target_valid = camera_menu_target(&menu, i, &target);
        board_7b_set_menu_item(i, menu.items[i].writable, camera_menu_status(&menu, i), target_valid, target);
    }
}
static void cancel_pending_settings(void)
{
    if (mode_control.dirty || mode_control.awaiting) setting_control_response(&mode_control, false);
    camera_menu_cancel(&menu);
    publish_setting_status();
}
static void refresh_properties(const uint8_t *data, size_t size)
{
    sony_parse_properties(data, size, &mode, show_property, NULL);
    publish_scalar_properties(data, size);
    parse_focus_caps(data, size);
    uint32_t now = focus_now_ms();
    if (!camera_menu_snapshot(&menu, data, size, now))
        ESP_LOGW(TAG, "Invalid Sony properties: bytes=%u; retaining displayed values", (unsigned)size);
    setting_control_snapshot(&mode_control, &mode, now);

    publish_setting_status();
}
static bool write_setting(int command, uint32_t *transaction,
                          setting_control_t *control, uint16_t code, uint16_t type,
                          uint32_t generation)
{
    if (generation != atomic_load(&input_generation)) { cancel_pending_settings(); return true; }
    uint32_t value;
    if (!setting_control_next(control, focus_now_ms(), &value)) return true;
    camera_focus_cancel();
    bool accepted;
    bool ok = sony_set_scalar(command, (*transaction)++, code, type, value, &accepted);
    setting_control_response(control, ok && accepted);
    publish_setting_status();
    ESP_LOGI(TAG, "Setting 0x%04x target=0x%08lx %s", code, (unsigned long)value,
             ok && accepted ? "PENDING readback" : "REJECTED");
    return ok;
}

static bool write_menu_settings(int command, uint32_t *transaction, uint32_t generation)
{
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        if (generation != atomic_load(&input_generation)) { cancel_pending_settings(); return true; }
        camera_menu_write_t write;
        if (!camera_menu_next(&menu, i, focus_now_ms(), &write)) continue;
        if (i == MENU_FOCUS) camera_focus_cancel();
        bool accepted = false;
        bool ok = write.relative ? sony_setting_step(command, (*transaction)++, write.code,
                       write.value == 1 ? 1 : -1, &accepted) :
                       sony_set_scalar(command, (*transaction)++, write.code, write.type, write.value, &accepted);
        camera_menu_response(&menu, i, ok && accepted);
        publish_setting_status();
        ESP_LOGI(TAG, "Menu 0x%04x %s=0x%08lx %s", write.code, write.relative ? "step" : "target",
                 (unsigned long)write.value, ok && accepted ? "PENDING readback" : "REJECTED");
        if (!ok) return false;
        /* Refresh capabilities before the next parameter write. */
        break;
    }
    return true;
}

/* High-priority actions drain before property writes and the next JPEG.
 * Safety releases bypass queue capacity; every popped action is revalidated
 * after any network read, before its write starts. */
static bool execute_actions(int command, uint32_t *transaction, uint8_t *buffer, size_t *size)
{
    for (unsigned drained = 0; drained < CAMERA_ACTION_CAPACITY + 3; ++drained) {
        camera_action_t action;
        portENTER_CRITICAL(&controls_mux);
        bool found = camera_actions_next(&controls, focus_now_ms(), &action);
        published_caps.generation = controls.generation;
        atomic_store(&input_generation, controls.generation);
        if (found && action.action.type == PAD_ACTION_RECORD) record_executing = true;
        published_caps.record_pending = record_wait || record_executing || camera_actions_record_queued(&controls);
        portEXIT_CRITICAL(&controls_mux);
        if (!found) return true;
        pad_action_type_t type = action.action.type;
        if (type == PAD_ACTION_RECORD || (type == PAD_ACTION_ZOOM && action.action.value)) {
            const uint32_t group = 0;
            if (!ptp_request_data(command, SONY_OC_GET_ALL_EXT_PROP_INFO, (*transaction)++, &group, 1,
                                  buffer, OBJECT_CAPACITY, size)) return false;
            refresh_properties(buffer, *size);
            gamepad_caps_t caps; camera_gamepad_caps(&caps);
            bool available = type == PAD_ACTION_RECORD ? caps.recording_known && !record_wait &&
                caps.recording != (action.action.value != 0) :
                caps.zoom_known && caps.zoom_enabled &&
                !(caps.mf_known && caps.mf && caps.lens == PAD_LENS_NON_POWER_ZOOM);
            if (!available) {
                record_executing = false;
                board_7b_set_command_status(type == PAD_ACTION_RECORD ? SONY_DPC_MOVIE_RECORD : SONY_DPC_ZOOM_OPERATION,
                                            SETTING_REJECTED);
                camera_gamepad_action((pad_action_t){PAD_ACTION_RELEASE_ALL, 0, caps.generation});
                continue;
            }
        }
        portENTER_CRITICAL(&controls_mux);
        bool current = camera_actions_current(&controls, &action);
        portEXIT_CRITICAL(&controls_mux);
        if (!current) { record_executing = false; continue; }
        bool accepted = false, ok;
        if (type == PAD_ACTION_S1 || type == PAD_ACTION_S2)
            ok = sony_shutter_button(command, (*transaction)++, type == PAD_ACTION_S2, action.action.value != 0, &accepted);
        else if (type == PAD_ACTION_ZOOM)
            ok = sony_zoom(command, (*transaction)++, action.action.value, &accepted);
        else if (type == PAD_ACTION_RECORD)
            ok = sony_movie_record(command, (*transaction)++, action.action.value != 0, &accepted);
        else return false;
        record_executing = false;
        portENTER_CRITICAL(&controls_mux);
        camera_actions_complete(&controls, &action, ok && accepted);
        portEXIT_CRITICAL(&controls_mux);
        if (!ok) return false;
        if (!accepted && action.release) {
            ESP_LOGE(TAG, "Control release rejected; closing session to recover");
            return false;
        }
        uint16_t property = type == PAD_ACTION_RECORD ? SONY_DPC_MOVIE_RECORD :
            type == PAD_ACTION_ZOOM ? SONY_DPC_ZOOM_OPERATION :
            type == PAD_ACTION_S2 ? SONY_DPC_SHUTTER_RELEASE : SONY_DPC_SHUTTER_HALF_RELEASE;
        board_7b_set_command_status(property, accepted ? SETTING_ACCEPTED : SETTING_REJECTED);
        if (type == PAD_ACTION_RECORD && accepted) {
            record_wait = true; record_target = action.action.value != 0;
            record_deadline = focus_now_ms() + 10000;
            portENTER_CRITICAL(&controls_mux);
            published_caps.record_pending = true;
            portEXIT_CRITICAL(&controls_mux);
            board_7b_set_command_status(property, SETTING_PENDING);
        } else if (!accepted && !action.release) {
            gamepad_caps_t caps; camera_gamepad_caps(&caps);
            camera_gamepad_action((pad_action_t){PAD_ACTION_RELEASE_ALL, 0, caps.generation});
        }
        ESP_LOGI(TAG, "High-priority control property=0x%04x value=%d accepted=%d", property, action.action.value, accepted);
    }
    return true;
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
            refresh_properties(buffer, size);
        }
    }
    return ok && !atomic_load(&stop_requested) && identity_operation(true);
}

static bool read_liveview(int command, int event, uint32_t *next_transaction)
{
    atomic_store(&mode_control_ready, false);
    controls_session(false);
    atomic_store(&focus_eligible, false);
    camera_focus_cancel();
    mode.count = 0;
    mode_control = (setting_control_t){0};
    menu = (camera_menu_t){0};
    publish_setting_status();
    reset_setting_inputs();
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
    controls_session(true);
    atomic_store(&mode_control_ready, true);
    const uint32_t handle = SONY_LIVEVIEW_HANDLE;
    ESP_LOGI(TAG, "LIVEVIEW RUNNING: RX core=%d, decode core=1, 2x1MiB pipeline; s stops, j starts", xPortGetCoreID());
    unsigned refused_frames = 0;
    uint32_t setting_generation = atomic_load(&input_generation);
    while (ok && !atomic_load(&stop_requested) && !atomic_load(&pipeline.failed)) {
        int slot;
        if (xQueueReceive(pipeline.free_slots, &slot, pdMS_TO_TICKS(100)) != pdTRUE) continue;
        if (atomic_load(&stop_requested) || atomic_load(&pipeline.failed)) break;
        ok = execute_actions(command, next_transaction, pipeline.data[slot], &size);
        if (!ok) break;
        uint32_t generation = atomic_load(&input_generation);
        if (generation != setting_generation) {
            cancel_pending_settings();
            setting_generation = generation;
        }
        bool properties_changed;
        ok = ptp_drain_events_changed(event, &properties_changed);
        if (!ok) break;
        int steps = atomic_exchange(&mode_steps, 0);
        int focus_steps = atomic_exchange(&focus_mode_steps, 0);
        int edits[CAMERA_MENU_COUNT];
        bool setting_changed = steps || focus_steps;
        bool properties_fresh = false;
        for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
            edits[i] = atomic_exchange(&menu_steps[i], 0);
            setting_changed |= edits[i] != 0;
        }
        if (setting_changed || properties_changed || esp_timer_get_time() >= next_exposure_read) {
            const uint32_t group = 0;
            ok = ptp_request_data(command, SONY_OC_GET_ALL_EXT_PROP_INFO, (*next_transaction)++, &group, 1,
                                  pipeline.data[slot], OBJECT_CAPACITY, &size);
            if (!ok) break;
            refresh_properties(pipeline.data[slot], size);
            properties_fresh = true;
            if (generation == atomic_load(&input_generation)) {
                if (steps) setting_control_step(&mode_control, steps);
                if (focus_steps) camera_menu_step(&menu, MENU_FOCUS, focus_steps, true);
                for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i)
                    if (edits[i]) camera_menu_step(&menu, i, edits[i], false);
            } else cancel_pending_settings();
            publish_setting_status();
            next_exposure_read = esp_timer_get_time() + 5000000;
        }
        /* Input may arrive during the property transaction. Drain releases
         * before any settings write, then revalidate the captured generation. */
        ok = execute_actions(command, next_transaction, pipeline.data[slot], &size);
        if (!ok) break;
        ok = write_setting(command, next_transaction, &mode_control, SONY_DPC_EXPOSURE_PROGRAM, 6, generation);
        if (!ok) break;
        /* An exposure change can change the selectable Focus modes. Refresh
         * before issuing a Focus target while Mode is awaiting confirmation. */
        if (!mode_control.awaiting && properties_fresh) {
            ok = write_menu_settings(command, next_transaction, generation);
            if (!ok) break;
        }
        if (mode_control.awaiting || camera_menu_pending(&menu)) {
            int64_t deadline = esp_timer_get_time() + 500000;
            if (next_exposure_read > deadline) next_exposure_read = deadline;
        }
        focus_request_t focus;
        if (xQueueReceive(focus_requests, &focus, 0) == pdTRUE && camera_focus_ready() &&
            focus.epoch == atomic_load(&focus_epoch) && focus.generation == atomic_load(&input_generation) &&
            (uint32_t)(focus_now_ms() - focus.queued_ms) < 500) {
            // Recheck body/menu changes before every step; never write using a stale MF value.
            const uint32_t group = 0;
            ok = ptp_request_data(command, SONY_OC_GET_ALL_EXT_PROP_INFO, (*next_transaction)++,
                                  &group, 1, pipeline.data[slot], OBJECT_CAPACITY, &size);
            if (!ok) break;
            refresh_properties(pipeline.data[slot], size);
            next_exposure_read = esp_timer_get_time() + 5000000;
            if (camera_focus_ready() && focus.epoch == atomic_load(&focus_epoch) && focus.generation == atomic_load(&input_generation) &&
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
        ok = execute_actions(command, next_transaction, pipeline.data[slot], &size);
        if (!ok) break;
        if (record_wait) {
            int64_t deadline = esp_timer_get_time() + 500000;
            if (next_exposure_read > deadline) next_exposure_read = deadline;
        }
        portENTER_CRITICAL(&controls_mux);
        bool actions_pending = controls.count != 0 || controls.release_pending;
        portEXIT_CRITICAL(&controls_mux);
        if (actions_pending) { xQueueSend(pipeline.free_slots, &slot, 0); continue; }
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
    /* The release transaction below reuses data[0]: the decoder must return
     * both slots before that buffer can receive a PTP/IP response. */
    if (worker_started) {
        jpeg_job_t end = {.slot = -1};
        xQueueSend(pipeline.ready, &end, portMAX_DELAY);
        xSemaphoreTake(pipeline.done, portMAX_DELAY);
    }
    /* A partial/cancelled PTP transaction cannot safely accept another write.
     * At a clean boundary, give releases a bounded stop window before close. */
    if (worker_started && ok && (atomic_load(&stop_requested) || atomic_load(&pipeline.failed)) &&
        ptpip_last_status() == PTPIP_IO_OK) {
        gamepad_caps_t caps; camera_gamepad_caps(&caps);
        camera_gamepad_action((pad_action_t){PAD_ACTION_RELEASE_ALL, 0, caps.generation});
        atomic_store(&releasing_controls, true);
        size_t unused = 0;
        execute_actions(command, next_transaction, pipeline.data[0], &unused);
        atomic_store(&releasing_controls, false);
    }
    controls_session(false);
    cancel_pending_settings();
    atomic_store(&focus_eligible, false);
    camera_focus_cancel();
    reset_setting_inputs();
    if (atomic_load(&pipeline.failed)) ok = false;
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
            maint_mode_on_camera_session(true);
            uint32_t maint_wait=focus_now_ms();
            while (maint_mode_is_on() && !atomic_load(&stop_requested) &&
                   (uint32_t)(focus_now_ms()-maint_wait)<2000) wait_cancel(10);
            if (maint_mode_is_on() || atomic_load(&stop_requested)) { maint_mode_on_camera_session(false);close(event);return false; }
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
    maint_mode_on_camera_session(false);
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
        atomic_store(&last_io, ptpip_last_status());
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
    controls_session(false);
    atomic_store(&focus_eligible, false);
    camera_focus_cancel();
    reset_setting_inputs();
    if (atomic_load(&stop_requested)) ESP_LOGI(TAG, "LIVEVIEW STOPPED: sockets closed; last frame retained");
    ESP_LOGI(TAG, "Camera task finished");
    maint_mode_on_camera_session(false);
    atomic_store(&busy, false);
    vTaskDeleteWithCaps(NULL);
}

static void start_request(bool jpeg)
{
    portENTER_CRITICAL(&lifecycle_mux);
    bool blocked = maintenance_gate || atomic_load(&busy);
    if (!blocked) { atomic_store(&busy, true); atomic_store(&stop_requested, false); }
    portEXIT_CRITICAL(&lifecycle_mux);
    if (blocked) {
        ESP_LOGI(TAG, "Camera request already active");
        return;
    }
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
    atomic_store(&stop_requested_ms, focus_now_ms());
    atomic_store(&stop_requested, true);
    camera_focus_cancel();
    ESP_LOGI(TAG, "Stop requested; cancelling network waits");
}

bool camera_maintenance_acquire(uint32_t timeout_ms)
{
    portENTER_CRITICAL(&lifecycle_mux);
    bool blocked = maintenance_gate;
    if (!blocked) {
        maintenance_gate = true;
        atomic_store(&stop_requested_ms, focus_now_ms());
        atomic_store(&stop_requested, true);
    }
    portEXIT_CRITICAL(&lifecycle_mux);
    if (blocked) return false;
    camera_focus_cancel();
    uint32_t started = focus_now_ms();
    while (atomic_load(&busy)) {
        if ((uint32_t)(focus_now_ms() - started) >= timeout_ms) {
            portENTER_CRITICAL(&lifecycle_mux); maintenance_gate = false; portEXIT_CRITICAL(&lifecycle_mux);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    /* Start and forget paths check this gate under the same lock. */
    portENTER_CRITICAL(&lifecycle_mux); atomic_store(&busy, true); portEXIT_CRITICAL(&lifecycle_mux);
    ESP_LOGI(TAG, "Maintenance lease acquired: camera sockets and decoder drained");
    return true;
}
void camera_maintenance_release(void)
{
    portENTER_CRITICAL(&lifecycle_mux);
    atomic_store(&busy, false); maintenance_gate = false;
    portEXIT_CRITICAL(&lifecycle_mux);
}
void camera_debug_get_status(camera_debug_status_t *out)
{
    gamepad_caps_t caps; camera_gamepad_caps(&caps);
    out->busy = atomic_load(&busy); out->stopped = atomic_load(&stop_requested);
    out->session = caps.session; out->last_io = atomic_load(&last_io);
    portENTER_CRITICAL(&debug_mux); memcpy(out->phase, debug_phase, sizeof(debug_phase)); portEXIT_CRITICAL(&debug_mux);
}
bool camera_forget_pairing(void)
{
    portENTER_CRITICAL(&lifecycle_mux);
    bool blocked = maintenance_gate || atomic_load(&busy);
    if (!blocked) atomic_store(&busy, true);
    portEXIT_CRITICAL(&lifecycle_mux);
    if (blocked) {
        ESP_LOGW(TAG, "Pair reset ignored: send s and wait for Camera task finished first"); return false;
    }
    bool ok = camera_identity_forget();
    if (ok) {
        wifi_ap_select_camera(NULL);
        ESP_LOGI(TAG, "Pairing identity cleared; press j/p and confirm on camera");
    } else ESP_LOGE(TAG, "Pair reset failed");
    atomic_store(&busy, false);
    return ok;
}

void camera_pair_console_init(void)
{
    focus_requests = xQueueCreate(1, sizeof(focus_request_t));
    ESP_ERROR_CHECK(focus_requests ? ESP_OK : ESP_ERR_NO_MEM);
    ptpip_set_cancel(cancelled, NULL);
    camera_console_init();
}
