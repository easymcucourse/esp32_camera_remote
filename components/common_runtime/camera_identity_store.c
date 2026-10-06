#include "camera_identity.h"
#include <string.h>
#include "nvs.h"
#include "esp_random.h"
#include "esp_log.h"
static const char *TAG = "camera_identity";
bool camera_identity_load(camera_identity_t *identity)
{
    if (!identity) return false;
    memset(identity, 0, sizeof(*identity));
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sony_remote", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;
    size_t size = sizeof(identity->guid);
    err = nvs_get_blob(nvs, "guid", identity->guid, &size);
    bool fresh = err == ESP_ERR_NVS_NOT_FOUND;
    if (fresh) {
        size_t peer_size = 0;
        err = nvs_get_blob(nvs, "peer", NULL, &peer_size);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            esp_fill_random(identity->guid, sizeof(identity->guid));
            err = nvs_set_blob(nvs, "guid", identity->guid, sizeof(identity->guid));
            if (err == ESP_OK) err = nvs_commit(nvs);
        } else if (err == ESP_OK) err = ESP_ERR_INVALID_SIZE; /* orphan peer */
    } else if (err == ESP_OK && size != sizeof(identity->guid)) err = ESP_ERR_INVALID_SIZE;
    if (err == ESP_OK) {
        size = sizeof(identity->peer);
        err = nvs_get_blob(nvs, "peer", identity->peer, &size);
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK; /* migrate existing GUID */
        else if (err == ESP_OK && size == sizeof(identity->peer) && !fresh) identity->paired = true;
        else if (err == ESP_OK) err = ESP_ERR_INVALID_SIZE;
    }
    nvs_close(nvs);
    if (err != ESP_OK) ESP_LOGE(TAG, "Pairing identity invalid: %s; reset pairing from maintenance Web", esp_err_to_name(err));
    return err == ESP_OK;
}
bool camera_identity_confirm(camera_identity_t *identity, const uint8_t mac[6], const uint8_t camera_guid[16])
{
    if (!identity || !mac || !camera_guid) return false;
    if (identity->paired) return !memcmp(identity->peer, mac, 6) && !memcmp(identity->peer + 6, camera_guid, 16);
    uint8_t peer[22];
    memcpy(peer, mac, 6); memcpy(peer + 6, camera_guid, 16);
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sony_remote", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs, "peer", peer, sizeof(peer));
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (err != ESP_OK) { ESP_LOGE(TAG, "Saving confirmed pairing failed: %s", esp_err_to_name(err)); return false; }
    memcpy(identity->peer, peer, sizeof(peer)); identity->paired = true;
    ESP_LOGI(TAG, "PAIRING SAVED: Sony initialization completed; reconnect will use stored peer");
    return true;
}


bool camera_identity_forget(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sony_remote", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_erase_all(nvs);
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (err != ESP_OK) ESP_LOGE(TAG, "Pair reset failed: %s", esp_err_to_name(err));
    return err == ESP_OK;
}
