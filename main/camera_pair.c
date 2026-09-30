#include "camera_pair.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdatomic.h>
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "board_7b.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "nvs.h"

#define CAMERA_IP "192.168.4.2"
#define CAMERA_PORT 15740
static const char *TAG = "camera_pair";
static atomic_bool busy = false;
static atomic_bool stop_requested = false;
static QueueHandle_t mode_requests;
static atomic_bool mode_control_ready;
static uint32_t mode_values[64], current_mode;
static unsigned mode_count;
static bool mode_writable;

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

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put32(uint8_t *p, uint32_t value)
{
    for (int i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static bool transfer(int fd, void *buffer, size_t length, bool transmit)
{
    uint8_t *p = buffer;
    while (length) {
        int n = transmit ? send(fd, p, length, 0) : recv(fd, p, length, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            ESP_LOGW(TAG, "%s stopped: result=%d errno=%d", transmit ? "send" : "recv", n, errno);
            return false;
        }
        p += n;
        length -= n;
    }
    return true;
}

static bool timeout_set(int fd, int seconds)
{
    struct timeval timeout = {.tv_sec = seconds};
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
           setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
}

static int connect_camera(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(CAMERA_PORT)};
    inet_pton(AF_INET, CAMERA_IP, &address.sin_addr);
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) goto fail;
    int rc = connect(fd, (struct sockaddr *)&address, sizeof(address));
    if (rc < 0 && errno != EINPROGRESS) goto fail;
    if (rc < 0) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        struct timeval wait = {.tv_sec = 5};
        if (select(fd + 1, NULL, &writable, NULL, &wait) <= 0) goto fail;
        int error = 0;
        socklen_t size = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error) goto fail;
    }
    if (fcntl(fd, F_SETFL, 0) < 0 || !timeout_set(fd, 5)) goto fail;
    return fd;
fail:
    ESP_LOGW(TAG, "TCP connect to %s:%d failed", CAMERA_IP, CAMERA_PORT);
    close(fd);
    return -1;
}

// The diagnostic expects small handshake/response packets only, never image data.
static int receive_packet(int fd, uint8_t *packet, size_t capacity)
{
    if (!transfer(fd, packet, 8, false)) return -1;
    uint32_t length = get32(packet);
    if (length < 8 || length > capacity) {
        ESP_LOGE(TAG, "Unexpected packet length=%lu", (unsigned long)length);
        return -1;
    }
    if (!transfer(fd, packet + 8, length - 8, false)) return -1;
    ESP_LOGI(TAG, "RX type=%lu length=%lu", (unsigned long)get32(packet + 4), (unsigned long)length);
    return (int)length;
}

static bool operation(int fd, uint16_t code, uint32_t transaction, bool session_param)
{
    uint8_t packet[128] = {0};
    uint32_t length = session_param ? 22 : 18;
    put32(packet, length);
    put32(packet + 4, 6);
    put32(packet + 8, 1);
    packet[12] = code;
    packet[13] = code >> 8;
    put32(packet + 14, transaction);
    if (session_param) put32(packet + 18, 1);
    if (!transfer(fd, packet, length, true)) return false;
    int n = receive_packet(fd, packet, sizeof(packet));
    if (n < 14 || get32(packet + 4) != 7 || get32(packet + 10) != transaction) return false;
    unsigned response = packet[8] | (unsigned)packet[9] << 8;
    ESP_LOGI(TAG, "Operation 0x%04x response=0x%04x transaction=%lu", code, response, (unsigned long)transaction);
    return response == 0x2001;
}

static bool request_data(int fd, uint16_t opcode, uint32_t transaction,
                         const uint32_t *params, unsigned num_params,
                         uint8_t *output, size_t capacity, size_t *output_size)
{
    uint8_t packet[64] = {0};
    if (num_params > 5) return false;
    uint32_t length = 18 + num_params * 4;
    put32(packet, length);
    put32(packet + 4, 6);
    put32(packet + 8, 1); // Same data-phase field as the recorded Remote requests.
    packet[12] = opcode;
    packet[13] = opcode >> 8;
    put32(packet + 14, transaction);
    for (unsigned i = 0; i < num_params; ++i) put32(packet + 18 + i * 4, params[i]);
    if (!transfer(fd, packet, length, true)) return false;
    *output_size = 0;
    size_t expected = 0;
    bool started = false, ended = false;
    // TCP packet boundaries do not correspond to PTP/IP messages.
    for (unsigned packets = 0; packets < 256; ++packets) {
        if (!transfer(fd, packet, 8, false)) return false;
        length = get32(packet);
        uint32_t type = get32(packet + 4);
        if (length < 8 || length > capacity + 12) return false;
        if (type == 13 && length == 8) { // ProbeRequest
            put32(packet + 4, 14);
            if (!transfer(fd, packet, 8, true)) return false;
        } else if (type == 9 && length == 20 && !started) {
            if (!transfer(fd, packet + 8, 12, false) || get32(packet + 8) != transaction) return false;
            if (get32(packet + 16) || get32(packet + 12) > capacity) return false;
            expected = get32(packet + 12);
            started = true;
        } else if ((type == 10 || type == 12) && length >= 12 && started && !ended) {
            if (!transfer(fd, packet + 8, 4, false) || get32(packet + 8) != transaction) return false;
            size_t payload_size = length - 12;
            if (payload_size > expected - *output_size) return false;
            if (!transfer(fd, output + *output_size, payload_size, false)) return false;
            *output_size += payload_size;
            if (type == 12) {
                ended = true;
                if (*output_size != expected) return false;
            }
        } else if (type == 7 && length >= 14 && length <= sizeof(packet)) {
            if (!transfer(fd, packet + 8, length - 8, false) || get32(packet + 10) != transaction) return false;
            unsigned response = packet[8] | (unsigned)packet[9] << 8;
            if (opcode != 0x1009 || response != 0x2001) {
                ESP_LOGI(TAG, "Read 0x%04x transaction=%lu response=0x%04x bytes=%u",
                         opcode, (unsigned long)transaction, response, (unsigned)*output_size);
            }
            return response == 0x2001 && (!started || ended);
        } else {
            ESP_LOGE(TAG, "Unexpected data packet type=%lu length=%lu", (unsigned long)type, (unsigned long)length);
            return false;
        }
    }
    return false;
}

static bool ptp_string(const uint8_t *data, size_t size, size_t *offset,
                       char *output, size_t capacity)
{
    if (*offset >= size || !capacity) return false;
    unsigned count = data[(*offset)++];
    if (count > (size - *offset) / 2) return false;
    size_t written = 0;
    for (unsigned i = 0; i < count; ++i) {
        uint16_t ch = get16(data + *offset + i * 2);
        if (!ch) break;
        if (written + 1 < capacity) output[written++] = ch < 128 ? (char)ch : '?';
    }
    output[written] = 0;
    *offset += count * 2;
    return true;
}

static void parse_device_info(const uint8_t *data, size_t size)
{
    size_t offset = 8;
    char scratch[64], model[24], firmware[24];
    if (size < offset || !ptp_string(data, size, &offset, scratch, sizeof(scratch)) ||
        offset + 2 > size) return;
    offset += 2;
    for (int array = 0; array < 5; ++array) {
        if (offset + 4 > size) return;
        uint32_t count = get32(data + offset);
        offset += 4;
        if (count > (size - offset) / 2) return;
        offset += count * 2;
    }
    if (!ptp_string(data, size, &offset, scratch, sizeof(scratch)) ||
        !ptp_string(data, size, &offset, model, sizeof(model)) ||
        !ptp_string(data, size, &offset, firmware, sizeof(firmware))) return;
    board_7b_set_camera_info(model, firmware);
    ESP_LOGI(TAG, "DeviceInfo: model=%s firmware=%s", model, firmware);
}

static bool sony_property_value(const uint8_t *data, size_t size, uint16_t code,
                                uint16_t type, uint32_t *value)
{
    // Sony 0x9209 entries are not standard PTP DevicePropDesc records:
    // code:u16, type:u16, getset:u8, enabled:u8, default, current, form...
    // The capture starts with entry_count:u32 and reserved:u32.
    unsigned width = type <= 2 ? 1 : type <= 4 ? 2 : type <= 6 ? 4 : 0;
    if (!width || size < 8 || get32(data + 4) != 0) return false;
    for (size_t i = 8; i + 6 + width * 2 < size; ++i) {
        if (get16(data + i) == code && get16(data + i + 2) == type &&
            (data[i + 4] <= 1 || (data[i + 4] & 0x80)) && data[i + 5] <= 2) {
            const uint8_t *current = data + i + 6 + width;
            *value = width == 1 ? current[0] :
                     width == 2 ? get16(current) : get32(current);
            return true;
        }
    }
    return false;
}

static void parse_sony_properties(const uint8_t *data, size_t size)
{
    mode_count = 0;
    mode_writable = false;
    // Sony UINT32 ExposureProgram: header(6), default(4), current(4), enum form.
    if (size >= 8 && get32(data + 4) == 0) {
        for (size_t i = 8; i + 17 <= size; ++i) {
            if (get16(data + i) != 0x500e || get16(data + i + 2) != 6 ||
                data[i + 5] > 2 || data[i + 14] != 2) continue;
            unsigned count = get16(data + i + 15);
            if (!count || count > 64 || count > (size - i - 17) / 4) continue;
            current_mode = get32(data + i + 10);
            mode_count = count;
            mode_writable = data[i + 4] == 1 && data[i + 5] == 1;
            for (unsigned j = 0; j < count; ++j) mode_values[j] = get32(data + i + 17 + j * 4);
            break;
        }
    }
    static const struct { uint16_t code, type; } properties[] = {
        {0x5005, 4}, {0x5007, 4}, {0x500a, 4}, {0x500b, 4},
        {0x500c, 4}, {0x5010, 3}, {0xd20d, 6}, {0xd21e, 6},
    };
    uint32_t value;
    if (sony_property_value(data, size, 0x500e, 6, &value)) {
        current_mode = value;
        board_7b_set_exposure_mode(value);
        ESP_LOGI(TAG, "Exposure mode=0x%08lx", (unsigned long)value);
    }
    for (unsigned i = 0; i < sizeof(properties) / sizeof(properties[0]); ++i) {
        if (sony_property_value(data, size, properties[i].code, properties[i].type, &value))
            board_7b_set_camera_property(properties[i].code, value);
    }
}

// Sony SetExtDevicePropValue (0x9205), UINT32 ExposureProgram (0x500e).
// Called exclusively by the socket owner between live-view transactions.
static bool set_exposure_mode(int fd, uint32_t transaction, uint32_t value, bool *accepted)
{
    uint8_t packet[128] = {0};
    *accepted = false;
    put32(packet, 22); put32(packet + 4, 6); put32(packet + 8, 2);
    packet[12] = 0x05; packet[13] = 0x92;
    put32(packet + 14, transaction); put32(packet + 18, 0x500e);
    if (!transfer(fd, packet, 22, true)) return false;
    memset(packet, 0, 20);
    put32(packet, 20); put32(packet + 4, 9); put32(packet + 8, transaction);
    put32(packet + 12, 4);
    if (!transfer(fd, packet, 20, true)) return false;
    put32(packet, 16); put32(packet + 4, 10); put32(packet + 8, transaction);
    put32(packet + 12, value);
    if (!transfer(fd, packet, 16, true)) return false;
    put32(packet, 12); put32(packet + 4, 12); put32(packet + 8, transaction);
    if (!transfer(fd, packet, 12, true)) return false;
    for (unsigned i = 0; i < 16; ++i) {
        int n = receive_packet(fd, packet, sizeof(packet));
        if (n == 8 && get32(packet + 4) == 13) {
            put32(packet + 4, 14);
            if (!transfer(fd, packet, 8, true)) return false;
            continue;
        }
        if (n < 14 || get32(packet + 4) != 7 || get32(packet + 10) != transaction) return false;
        uint16_t response = get16(packet + 8);
        *accepted = response == 0x2001;
        ESP_LOGI(TAG, "Set Mode=0x%08lx response=0x%04x", (unsigned long)value, response);
        return true; // A camera rejection does not invalidate the TCP session.
    }
    return false;
}

static bool drain_events(int event)
{
    // Drain events between frames so the independent event connection cannot fill.
    for (unsigned i = 0; i < 32; ++i) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(event, &readable);
        struct timeval poll = {0};
        int ready = select(event + 1, &readable, NULL, NULL, &poll);
        if (ready == 0) return true;
        if (ready < 0) return false;
        uint8_t packet[512];
        int length = receive_packet(event, packet, sizeof(packet));
        if (length < 8) return false;
        uint32_t type = get32(packet + 4);
        if (type == 13 && length == 8) {
            put32(packet + 4, 14);
            if (!transfer(event, packet, 8, true)) return false;
        } else if (type != 8 || length < 14) {
            ESP_LOGW(TAG, "Unexpected event type=%lu", (unsigned long)type);
            return false;
        }
    }
    return true;
}

static bool display_object(const uint8_t *data, size_t size, size_t *jpeg_size)
{
    size_t offset = size >= 4 ? get32(data) : size;
    if (offset > size || size - offset < 3 || data[offset] != 0xff || data[offset + 1] != 0xd8 || data[offset + 2] != 0xff) {
        ESP_LOGE(TAG, "Live-view JPEG header mismatch: object=%u offset=%u", (unsigned)size, (unsigned)offset);
        return false;
    }
    size_t end = offset + 2;
    while (end + 1 < size && !(data[end] == 0xff && data[end + 1] == 0xd9)) ++end;
    if (end + 1 >= size) {
        ESP_LOGE(TAG, "Live-view JPEG missing EOI");
        return false;
    }
    *jpeg_size = end + 2 - offset;
    return board_7b_show_jpeg(data + offset, *jpeg_size) == ESP_OK;
}

typedef struct {
    int slot; // -1 terminates the worker after all submitted frames.
    size_t size;
    int64_t read_ms;
} jpeg_job_t;

typedef struct {
    uint8_t *data[2];
    QueueHandle_t free_slots;
    QueueHandle_t ready;
    SemaphoreHandle_t done;
    atomic_bool failed;
} jpeg_pipeline_t;

static void jpeg_decode_task(void *arg)
{
    jpeg_pipeline_t *pipeline = arg;
    unsigned frames = 0, window_frames = 0;
    int64_t window_start = esp_timer_get_time();
    ESP_LOGI(TAG, "JPEG worker: core=%d, ESP32-S3 SIMD RGB565 decoder", xPortGetCoreID());
    for (;;) {
        jpeg_job_t job;
        xQueueReceive(pipeline->ready, &job, portMAX_DELAY);
        if (job.slot < 0) break;
        if (!atomic_load(&pipeline->failed)) {
            int64_t start = esp_timer_get_time();
            size_t jpeg_size = 0;
            if (!display_object(pipeline->data[job.slot], job.size, &jpeg_size)) {
                atomic_store(&pipeline->failed, true);
            } else {
                int64_t shown = esp_timer_get_time();
                ++frames;
                ++window_frames;
                if (frames == 1 || shown - window_start >= 5000000) {
                    ESP_LOGI(TAG, "LIVEVIEW frames=%u fps=%.2f JPEG=%u read=%ldms display=%ldms stack_free=%u",
                             frames, (double)window_frames * 1000000 / (shown - window_start),
                             (unsigned)jpeg_size, (long)job.read_ms, (long)((shown - start) / 1000),
                             (unsigned)uxTaskGetStackHighWaterMark(NULL));
                    window_start = shown;
                    window_frames = 0;
                }
            }
        }
        // Ownership returns only after decode and LCD publication have finished.
        xQueueSend(pipeline->free_slots, &job.slot, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "JPEG worker drained: displayed=%u", frames);
    // No pipeline access after this signal: the producer may free its context.
    xSemaphoreGive(pipeline->done);
    vTaskDeleteWithCaps(NULL);
}

static bool read_liveview(int command, int event, uint32_t *next_transaction)
{
    atomic_store(&mode_control_ready, false);
    mode_count = 0;
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
    // Exact read/initialization sequence from the ZV-E10 Remote capture.
    const struct { uint16_t opcode; unsigned count; uint32_t params[3]; } steps[] = {
        {0x9201, 3, {1, 0, 0}},
        {0x9201, 3, {2, 0, 0}},
        {0x1001, 1, {0}},
        {0x9202, 1, {300}},
        {0x9201, 3, {3, 0, 0}},
        {0x9202, 1, {300}},
        {0x9209, 1, {0}},
        {0x1008, 1, {0xffffc002}},
    };
    ok = true;
    size_t size = 0;
    for (unsigned i = 0; i < sizeof(steps) / sizeof(steps[0]) && ok; ++i) {
        ok = request_data(command, steps[i].opcode, (*next_transaction)++, steps[i].params,
                          steps[i].count, pipeline.data[0], OBJECT_CAPACITY, &size);
        if (ok && steps[i].opcode == 0x1001)
            parse_device_info(pipeline.data[0], size);
        if (ok && steps[i].opcode == 0x9209)
            parse_sony_properties(pipeline.data[0], size);
    }
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
    const uint32_t handle = 0xffffc002;
    ESP_LOGI(TAG, "LIVEVIEW RUNNING: RX core=%d, decode core=1, 2x1MiB pipeline; s stops, j starts", xPortGetCoreID());
    while (ok && !atomic_load(&stop_requested) && !atomic_load(&pipeline.failed)) {
        int slot;
        if (xQueueReceive(pipeline.free_slots, &slot, pdMS_TO_TICKS(100)) != pdTRUE) continue;
        if (atomic_load(&stop_requested) || atomic_load(&pipeline.failed)) break;
        ok = drain_events(event);
        if (!ok) break;
        int direction;
        bool mode_changed = mode_requests && xQueueReceive(mode_requests, &direction, 0) == pdTRUE;
        if (mode_changed || esp_timer_get_time() >= next_exposure_read) {
            const uint32_t property_group = 0;
            ok = request_data(command, 0x9209, (*next_transaction)++, &property_group, 1,
                              pipeline.data[slot], OBJECT_CAPACITY, &size);
            if (!ok) break;
            parse_sony_properties(pipeline.data[slot], size);
            next_exposure_read = esp_timer_get_time() + 5000000;
            if (mode_changed) {
                unsigned index = 0;
                while (index < mode_count && mode_values[index] != current_mode) ++index;
                if (!mode_writable || mode_count < 2 || index == mode_count) {
                    ESP_LOGW(TAG, "Mode change unavailable: writable=%d choices=%u", mode_writable, mode_count);
                } else {
                    unsigned next = direction > 0 ? (index + 1) % mode_count :
                        (index + mode_count - 1) % mode_count;
                    uint32_t target = mode_values[next];
                    bool accepted;
                    ok = set_exposure_mode(command, (*next_transaction)++, target, &accepted);
                    if (!ok) break;
                    if (accepted) {
                        ok = request_data(command, 0x9209, (*next_transaction)++, &property_group, 1,
                                          pipeline.data[slot], OBJECT_CAPACITY, &size);
                        if (!ok) break;
                        parse_sony_properties(pipeline.data[slot], size);
                        ESP_LOGI(TAG, "Mode readback: requested=0x%08lx actual=0x%08lx",
                                 (unsigned long)target, (unsigned long)current_mode);
                    }
                }
            }
        }
        int64_t start = esp_timer_get_time();
        ok = request_data(command, 0x1009, (*next_transaction)++, &handle, 1,
                          pipeline.data[slot], OBJECT_CAPACITY, &size);
        if (!ok) break;
        jpeg_job_t job = {.slot = slot, .size = size, .read_ms = (esp_timer_get_time() - start) / 1000};
        xQueueSend(pipeline.ready, &job, portMAX_DELAY);
    }
cleanup:
    atomic_store(&mode_control_ready, false);
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

static bool handshake(int command, const uint8_t guid[16], bool first, bool jpeg)
{
    uint8_t packet[512] = {0};
    const char name[] = "ESP32-Camera-Remote";
    uint32_t length = 24 + sizeof(name) * 2 + 4;
    put32(packet, length);
    put32(packet + 4, 1);
    memcpy(packet + 8, guid, 16);
    for (size_t i = 0; i < sizeof(name); ++i) packet[24 + i * 2] = name[i];
    put32(packet + length - 4, 0x00010000);
    if (!timeout_set(command, first ? 120 : 10)) return false;
    ESP_LOGI(TAG, "Sending InitCommandRequest as %s; confirm on camera if prompted", name);
    connection_status("Pairing - confirm on camera...");
    if (!transfer(command, packet, length, true)) return false;
    int n = receive_packet(command, packet, sizeof(packet));
    if (n >= 12 && get32(packet + 4) == 5) {
        ESP_LOGE(TAG, "InitFail reason=%lu", (unsigned long)get32(packet + 8));
        return false;
    }
    if (n < 30 || get32(packet + 4) != 2) return false;
    uint32_t connection = get32(packet + 8);
    char camera_name[64] = {0};
    for (int i = 0; i < sizeof(camera_name) - 1 && 28 + i * 2 + 1 < n - 4; ++i) {
        camera_name[i] = packet[28 + i * 2];
        if (!camera_name[i]) break;
    }
    ESP_LOGI(TAG, "INIT ACK: camera=%s connection=%lu", camera_name, (unsigned long)connection);
    board_7b_set_camera_info(camera_name, NULL);
    int event = connect_camera();
    if (event < 0) return false;
    put32(packet, 12);
    put32(packet + 4, 3);
    put32(packet + 8, connection);
    bool ok = transfer(event, packet, 12, true);
    if (ok) ok = receive_packet(event, packet, sizeof(packet)) == 8 && get32(packet + 4) == 4;
    if (ok) {
        ESP_LOGI(TAG, "EVENT ACK received");
        ok = timeout_set(command, 5) && operation(command, 0x1002, 2, true);
        if (ok) {
            ESP_LOGI(TAG, "SESSION VERIFIED: OpenSession accepted");
            connection_status("Session connected - preparing preview...");
            if (jpeg) {
                uint32_t next_transaction = 3;
                ok = read_liveview(command, event, &next_transaction);
                // On parsing/network failure, close sockets instead of sending into
                // an unconsumed data phase. On success, end the session normally.
                if (ok) ok = operation(command, 0x1003, next_transaction, false);
            } else {
                ok = operation(command, 0x1003, 3, false);
            }
        }
    }
    close(event);
    return ok;
}

static void pair_task(void *unused)
{
    bool jpeg = (uintptr_t)unused != 0;
    uint8_t guid[16];
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("sony_remote", NVS_READWRITE, &nvs));
    size_t size = sizeof(guid);
    esp_err_t result = nvs_get_blob(nvs, "guid", guid, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        esp_fill_random(guid, sizeof(guid));
        ESP_ERROR_CHECK(nvs_set_blob(nvs, "guid", guid, sizeof(guid)));
        ESP_ERROR_CHECK(nvs_commit(nvs));
    } else {
        ESP_ERROR_CHECK(result);
        ESP_ERROR_CHECK(size == sizeof(guid) ? ESP_OK : ESP_ERR_INVALID_SIZE);
    }
    nvs_close(nvs);
    bool ok = false;
    do {
        ESP_LOGI(TAG, "Persistent ESP32 GUID loaded; waiting for camera %s", CAMERA_IP);
        connection_status("Wi-Fi ready - waiting for camera...");
        const uint8_t camera_mac[6] = {0xd0, 0x40, 0xef, 0xde, 0x59, 0x9f};
        bool associated = false;
        while (!associated && !atomic_load(&stop_requested)) {
            wifi_sta_list_t clients = {0};
            if (esp_wifi_ap_get_sta_list(&clients) == ESP_OK) {
                for (int i = 0; i < clients.num; ++i) {
                    if (memcmp(clients.sta[i].mac, camera_mac, 6) == 0) associated = true;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (atomic_load(&stop_requested)) break;
        ESP_LOGI(TAG, "Camera associated; starting PTP/IP connection");
        connection_status("Camera joined Wi-Fi - connecting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        int fd = -1;
        for (int attempt = 0; attempt < 12 && fd < 0 && !atomic_load(&stop_requested); ++attempt) {
            fd = connect_camera();
            if (fd < 0) {
                connection_status("Camera connection failed - retrying...");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
        ok = fd >= 0 && !atomic_load(&stop_requested) && handshake(fd, guid, true, jpeg);
        if (fd >= 0) close(fd);
        if (ok && !jpeg) {
            ESP_LOGI(TAG, "Initial handshake/session passed; verifying reconnect with same GUID");
            vTaskDelay(pdMS_TO_TICKS(3000));
            fd = connect_camera();
            ok = fd >= 0 && handshake(fd, guid, false, false);
            if (fd >= 0) close(fd);
            if (ok) ESP_LOGI(TAG, "PAIRING VERIFIED: same GUID reconnect and PTP session succeeded");
        }
        if (jpeg && !atomic_load(&stop_requested)) {
            ESP_LOGW(TAG, "Live-view disconnected; retrying in 5 seconds");
            connection_status("Disconnected - retrying in 5 seconds...");
            for (int i = 0; i < 50 && !atomic_load(&stop_requested); ++i) vTaskDelay(pdMS_TO_TICKS(100));
        }
    } while (jpeg && !atomic_load(&stop_requested));
    if (jpeg) ESP_LOGI(TAG, "LIVEVIEW STOPPED: last frame remains on LCD");
    if (!jpeg) connection_status(ok ? "Pairing verified - press j for preview" : "Pairing failed - press p to retry");
    if (!ok) ESP_LOGW(TAG, "Camera request incomplete; inspect mode and preceding response");
    ESP_LOGI(TAG, "Camera task finished; sockets closed");
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
    // This task is mostly blocked on sockets or delays. Keeping its large stack
    // in PSRAM leaves internal RAM available for lwIP packet buffers.
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

static void console_task(void *unused)
{
    while (true) {
        uint8_t command;
        if (uart_read_bytes(UART_NUM_0, &command, 1, pdMS_TO_TICKS(1000)) == 1) {
            if (command == 'p' || command == 'P') camera_pair_start();
            if (command == 'j' || command == 'J') camera_jpeg_start();
            if (command == 'S') {
                bool enabled = board_7b_toggle_settings_mode();
                ESP_LOGI(TAG, "Settings display %s: preview=%s",
                         enabled ? "enabled" : "disabled",
                         enabled ? "768x432" : "1024x576");
            }
            if (command == 's') {
                atomic_store(&stop_requested, true);
                ESP_LOGI(TAG, "Stop requested; finishing current transaction");
            }
        }
    }
}

void camera_pair_console_init(void)
{
    mode_requests = xQueueCreate(32, sizeof(int));
    ESP_ERROR_CHECK(mode_requests ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(xTaskCreate(console_task, "pair_console", 4096, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "UART: j = live-view, S = settings display, s = stop, p = pairing");
}
