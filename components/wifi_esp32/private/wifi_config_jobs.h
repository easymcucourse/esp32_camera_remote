#pragma once
#include "app_wifi.h"
typedef struct wifi_config_jobs wifi_config_jobs_t;
typedef app_wifi_result_t (*wifi_job_io_t)(void *, const network_config_t *);
wifi_config_jobs_t *wifi_jobs_create(void *context, wifi_job_io_t save, wifi_job_io_t restart);
void wifi_jobs_destroy(wifi_config_jobs_t *jobs); /* Only after successful stop. */
void wifi_jobs_set(wifi_config_jobs_t *jobs, const network_config_t *config, unsigned max_channel);
app_wifi_result_t wifi_jobs_start(wifi_config_jobs_t *jobs);
app_wifi_result_t wifi_jobs_stop(wifi_config_jobs_t *jobs, uint32_t timeout_ms);
app_wifi_result_t wifi_jobs_get(wifi_config_jobs_t *jobs, network_config_t *config);
app_wifi_result_t wifi_jobs_apply(wifi_config_jobs_t *jobs, const network_config_t *config, bool staged, uint32_t *token);
app_wifi_result_t wifi_jobs_commit(wifi_config_jobs_t *jobs, uint32_t token, unsigned delay_ms);
app_wifi_result_t wifi_jobs_cancel(wifi_config_jobs_t *jobs, uint32_t token);
app_wifi_result_t wifi_jobs_result(wifi_config_jobs_t *jobs, uint32_t token, app_wifi_result_t *result);
app_wifi_result_t wifi_jobs_freeze(wifi_config_jobs_t *jobs, uint32_t timeout_ms);
void wifi_jobs_resume(wifi_config_jobs_t *jobs);
