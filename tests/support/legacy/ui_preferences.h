#pragma once
#include "app_message.h"
esp_err_t ui_preferences_start(void);
/* Core/UI lifecycle only; close admission, cancel queued writes and wait for
 * active commit/reader before freeing. Timeout retains all worker resources. */
bool ui_preferences_quiesce(uint32_t timeout_ms);
unsigned ui_preferences_level(void);
unsigned ui_preferences_pad(void);
esp_err_t ui_preferences_request(unsigned level,bool next,uint32_t *token);
esp_err_t ui_preferences_set_pad(unsigned mode);
bool ui_preferences_result(uint32_t *token,unsigned *level,esp_err_t *error);
esp_err_t ui_preferences_message(const app_message_t *message,app_message_t *reply,bool *deferred);

/* Original single-key store fixture; excluded from production. */
esp_err_t preferences_store_set(const char *key,unsigned value);
