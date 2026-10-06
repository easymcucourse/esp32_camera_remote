#include "camera_identity_work.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <assert.h>
#include <string.h>
struct fake_semaphore { bool signalled,live; };
static struct fake_semaphore semaphore;
static bool fail_semaphore,fail_task,nvs_result=true;
static unsigned creates,deletes,loads,confirms,forgets,task_finishes;
static camera_identity_t *caller;
SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    if (fail_semaphore) return NULL;
    assert(!semaphore.live); semaphore=(struct fake_semaphore){.live=true}; return &semaphore;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t handle)
{ assert(handle==&semaphore && handle->live && !handle->signalled); handle->signalled=true; return pdTRUE; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t handle,TickType_t timeout)
{ assert(handle==&semaphore && handle->live && handle->signalled && timeout==portMAX_DELAY); return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t handle)
{ assert(handle==&semaphore && handle->live); handle->live=false; ++deletes; }
BaseType_t xTaskCreate(TaskFunction_t function,const char *name,unsigned stack,void *context,unsigned priority,void *handle)
{
    assert(function && !strcmp(name,"camera_nvs") && stack==4096 && priority==4 && !handle);
    ++creates; if (fail_task) return pdFALSE; function(context); return pdPASS;
}
void vTaskDelete(void *task)
{ assert(!task && semaphore.signalled); ++task_finishes; }
bool camera_identity_load(camera_identity_t *identity)
{
    assert(identity!=caller && !identity->paired); ++loads;
    identity->guid[0]=42; return nvs_result;
}
bool camera_identity_confirm(camera_identity_t *identity,const uint8_t mac[6],const uint8_t guid[16])
{
    assert(identity!=caller && identity->guid[0]==42 && mac[0]==8 && guid[0]==9); ++confirms;
    memcpy(identity->peer,mac,6); memcpy(identity->peer+6,guid,16); identity->paired=true;
    return nvs_result;
}
bool camera_identity_forget(void) { ++forgets; return nvs_result; }
int main(void)
{
    camera_identity_t identity={0}; caller=&identity;
    assert(camera_identity_run(&identity,CAMERA_IDENTITY_LOAD,NULL,NULL));
    assert(identity.guid[0]==42 && loads==1 && creates==1 && task_finishes==1 && deletes==1);
    uint8_t mac[6]={8},guid[16]={9};
    assert(camera_identity_run(&identity,CAMERA_IDENTITY_CONFIRM,mac,guid));
    assert(identity.paired && identity.peer[0]==8 && identity.peer[6]==9 && confirms==1);
    camera_identity_t before=identity; nvs_result=false;
    assert(!camera_identity_run(&identity,CAMERA_IDENTITY_LOAD,NULL,NULL));
    assert(!memcmp(&before,&identity,sizeof identity));
    assert(!camera_identity_run(&identity,CAMERA_IDENTITY_CONFIRM,mac,guid));
    assert(!memcmp(&before,&identity,sizeof identity));
    nvs_result=true; fail_task=true; unsigned finished=task_finishes,called=loads;
    assert(!camera_identity_run(&identity,CAMERA_IDENTITY_LOAD,NULL,NULL));
    assert(task_finishes==finished && loads==called && !semaphore.live);
    fail_task=false; fail_semaphore=true; unsigned created=creates,deleted=deletes;
    assert(!camera_identity_run(&identity,CAMERA_IDENTITY_LOAD,NULL,NULL));
    assert(creates==created && deletes==deleted); fail_semaphore=false;
    assert(!camera_identity_run(NULL,CAMERA_IDENTITY_LOAD,NULL,NULL));
    assert(!camera_identity_run(&identity,CAMERA_IDENTITY_CONFIRM,NULL,guid));
    assert(!camera_identity_run(&identity,(camera_identity_operation_t)99,NULL,NULL));
    assert(creates==created && deletes==deleted);
    assert(camera_identity_run(NULL,CAMERA_IDENTITY_FORGET,NULL,NULL) && forgets==1);
    assert(camera_identity_run(&identity,CAMERA_IDENTITY_FORGET,NULL,NULL));
    assert(!identity.paired && !identity.guid[0] && !identity.peer[0] && forgets==2);
    assert(creates==deletes && !semaphore.live);
    return 0;
}
