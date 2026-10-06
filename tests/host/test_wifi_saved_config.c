#include "wifi_saved_config.h"
#include "nvs.h"
#include <assert.h>
#include <string.h>

static uint8_t durable[128], staged[128];
static size_t durable_size, staged_size;
static unsigned opens, closes, commits, erases, writes;
static bool fail_open, missing_namespace, fail_commit, fail_erase, fail_write;
static unsigned unrelated_key = 0x9876;
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *out)
{
    assert(!strcmp(name, "wifi_ap") && (mode == NVS_READONLY || mode == NVS_READWRITE)); ++opens;
    if (fail_open) return ESP_FAIL;
    if (missing_namespace && mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
    memcpy(staged, durable, sizeof(staged)); staged_size = durable_size; *out = 1; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *data, size_t *size)
{
    assert(handle == 1 && !strcmp(key, "cfg"));
    if (!staged_size) return ESP_ERR_NVS_NOT_FOUND;
    if (*size < staged_size) return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(data, staged, staged_size); *size = staged_size; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *data, size_t size)
{
    assert(handle == 1 && !strcmp(key, "cfg") && size == NETWORK_CONFIG_RECORD_SIZE); ++writes;
    if (fail_write) return ESP_FAIL;
    memcpy(staged, data, size); staged_size = size; return ESP_OK;
}
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
{
    assert(handle == 1 && !strcmp(key, "cfg")); ++erases;
    if (fail_erase) return ESP_FAIL;
    if (!staged_size) return ESP_ERR_NVS_NOT_FOUND;
    staged_size = 0; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle)
{
    assert(handle == 1); ++commits;
    if (fail_commit) return ESP_FAIL;
    memcpy(durable, staged, sizeof(durable)); durable_size = staged_size; return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { assert(handle == 1); ++closes; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "fake"; }
void wifi_test_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
int main(void)
{
    network_config_t config, defaults; network_config_make_default(&defaults);
    missing_namespace = true;
    assert(wifi_saved_read(&config) == ESP_OK && network_config_equal(&config, &defaults) && !commits && !closes);
    missing_namespace = false;
    assert(wifi_saved_read(&config) == ESP_OK && !commits && closes == 1);
    config = defaults; strcpy(config.ssid, "host-test"); config.show_password = false;
    assert(wifi_saved_write(&config) == ESP_OK && writes == 1 && durable_size == 100);
    network_config_t loaded; assert(wifi_saved_read(&loaded) == ESP_OK && network_config_equal(&loaded, &config) && commits == 1);
    fail_write = true; config.channel = 7;
    assert(wifi_saved_write(&config) == ESP_FAIL); fail_write = false;
    assert(wifi_saved_read(&loaded) == ESP_OK && loaded.channel == 6);
    fail_commit = true;
    assert(wifi_saved_write(&config) == ESP_FAIL); fail_commit = false;
    assert(wifi_saved_read(&loaded) == ESP_OK && loaded.channel == 6);
    durable[0] = 99; unsigned before = erases;
    assert(wifi_saved_read(&loaded) == ESP_OK && network_config_equal(&loaded, &defaults) && erases == before + 1 && !durable_size);
    durable_size = 120; fail_erase = true;
    assert(wifi_saved_read(&loaded) == ESP_FAIL && network_config_equal(&loaded, &defaults) && durable_size == 120);
    fail_erase = false; fail_commit = true;
    assert(wifi_saved_read(&loaded) == ESP_FAIL && durable_size == 120); fail_commit = false;
    assert(wifi_saved_read(&loaded) == ESP_OK && !durable_size);
    before = commits; assert(wifi_saved_write(&defaults) == ESP_OK && commits == before + 1);
    fail_open = true; assert(wifi_saved_read(&loaded) == ESP_FAIL && network_config_equal(&loaded, &defaults));
    assert(wifi_saved_write(&defaults) == ESP_FAIL); fail_open = false;
    before = opens; config.ssid[0] = 0;
    assert(wifi_saved_write(&config) == ESP_ERR_INVALID_ARG && opens == before);
    assert(wifi_saved_read(NULL) == ESP_ERR_INVALID_ARG && unrelated_key == 0x9876);
    return 0;
}
