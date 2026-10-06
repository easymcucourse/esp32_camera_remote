#include "camera_identity_work.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
typedef struct {
    camera_identity_t *identity;
    camera_identity_operation_t operation;
    uint8_t mac[6], peer_guid[16];
    SemaphoreHandle_t done;
    bool result;
} identity_work_t;
static void worker(void *context)
{
    identity_work_t *work=context;
    /* These values, including NVS's input blob, stay in the internal worker
     * stack while caches may be disabled. The producer may have a PSRAM stack. */
    camera_identity_operation_t operation=work->operation;
    camera_identity_t identity={0};
    uint8_t mac[6], guid[16];
    memcpy(mac,work->mac,sizeof mac); memcpy(guid,work->peer_guid,sizeof guid);
    if (operation==CAMERA_IDENTITY_CONFIRM) identity=*work->identity;
    bool result=operation==CAMERA_IDENTITY_LOAD ? camera_identity_load(&identity) :
        operation==CAMERA_IDENTITY_CONFIRM ? camera_identity_confirm(&identity,mac,guid) : camera_identity_forget();
    if (result && work->identity) *work->identity=identity;
    work->result=result;
    SemaphoreHandle_t done=work->done;
    /* No context access after the completion signal: caller can release it. */
    xSemaphoreGive(done);
    vTaskDelete(NULL);
}
bool camera_identity_run(camera_identity_t *identity, camera_identity_operation_t operation,
    const uint8_t mac[6], const uint8_t peer_guid[16])
{
    if ((unsigned)operation>CAMERA_IDENTITY_FORGET ||
        (operation!=CAMERA_IDENTITY_FORGET && !identity) ||
        (operation==CAMERA_IDENTITY_CONFIRM && (!mac || !peer_guid))) return false;
    identity_work_t work={.identity=identity,.operation=operation,.done=xSemaphoreCreateBinary()};
    if (!work.done) return false;
    if (operation==CAMERA_IDENTITY_CONFIRM) {
        memcpy(work.mac,mac,sizeof work.mac); memcpy(work.peer_guid,peer_guid,sizeof work.peer_guid);
    }
    bool started=xTaskCreate(worker,"camera_nvs",4096,&work,4,NULL)==pdPASS;
    if (started) xSemaphoreTake(work.done,portMAX_DELAY);
    vSemaphoreDelete(work.done);
    return started && work.result;
}
