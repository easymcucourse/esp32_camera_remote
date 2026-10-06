#include "camera_frames.h"
#include <stdint.h>
static void apply_completed(camera_frames_t *f);
static void returned(void *context)
{
    camera_frame_slot_t *slot = context;
    /* Last context access: producer may free it once this release is visible. */
    atomic_store_explicit(&slot->returned, true, memory_order_release);
}
bool camera_frames_init(camera_frames_t *f, uint32_t generation,
    uint8_t *first, uint8_t *second, size_t capacity)
{
    if (!f || !generation || !first || !second || !capacity) return false;
    uintptr_t first_address=(uintptr_t)first, second_address=(uintptr_t)second;
    uintptr_t distance=first_address>second_address ? first_address-second_address : second_address-first_address;
    if (distance<capacity) return false;
    *f = (camera_frames_t){.generation=generation,.next_token=1,.next_result=1};
    f->slots[0].buffer=first; f->slots[1].buffer=second;
    for (unsigned i=0;i<CAMERA_FRAME_SLOTS;++i) {
        f->slots[i].capacity=capacity; atomic_init(&f->slots[i].returned,true);
    }
    return true;
}
int camera_frames_acquire(camera_frames_t *f)
{
    if (!f || f->failed) return -1;
    for (unsigned i=0;i<CAMERA_FRAME_SLOTS;++i) {
        camera_frame_slot_t *s=&f->slots[i];
        if (!s->acquired && !s->awaiting && atomic_load_explicit(&s->returned,memory_order_acquire)) {
            s->acquired=true; return (int)i;
        }
    }
    return -1;
}
void camera_frames_discard(camera_frames_t *f, int slot)
{
    if (f && slot>=0 && slot<CAMERA_FRAME_SLOTS && !f->slots[slot].awaiting)
        f->slots[slot].acquired=false;
}
bool camera_frames_drop(camera_frames_t *f, int index)
{
    if (!f || index<0 || index>=CAMERA_FRAME_SLOTS || f->failed) return false;
    if (!f->next_token) { f->failed=true; return false; }
    camera_frame_slot_t *s=&f->slots[index];
    if (!s->acquired || s->awaiting || !atomic_load(&s->returned)) return false;
    s->token=f->next_token++; s->acquired=false; s->awaiting=true; s->completed=true;
    s->result=ESP_ERR_INVALID_RESPONSE; apply_completed(f); return true;
}
esp_err_t camera_frames_publish(camera_frames_t *f, int index, const uint8_t *jpeg, size_t length, uint32_t read_ms)
{
    if (!f || index<0 || index>=CAMERA_FRAME_SLOTS || !jpeg || !length) return ESP_ERR_INVALID_ARG;
    camera_frame_slot_t *s=&f->slots[index];
    uintptr_t address=(uintptr_t)jpeg, base=(uintptr_t)s->buffer;
    if (f->failed || !s->acquired || s->awaiting || !atomic_load(&s->returned)) return ESP_ERR_INVALID_STATE;
    if (address<base || address-base>s->capacity || length>s->capacity-(address-base)) return ESP_ERR_INVALID_ARG;
    /* Never wrap an identifier while an older frame may still be delivered. */
    if (!f->next_token) { f->failed=true; return ESP_ERR_INVALID_STATE; }
    app_message_t message={.type=APP_MESSAGE_CAMERA_FRAME,.source=APP_ENDPOINT_CAMERA,
        .flags=APP_MESSAGE_EVENT|APP_MESSAGE_BULK,.generation=f->generation};
    message.payload.command.token=f->next_token;
    message.payload.command.duration_ms=read_ms;
    atomic_store(&s->returned,false);
    esp_err_t error=app_message_lease_create((void *)jpeg,length,false,returned,s,&message.lease);
    if (error!=ESP_OK) { atomic_store(&s->returned,true); return error; }
    s->token=f->next_token; s->awaiting=true; s->completed=false; s->acquired=false;
    error=app_console_send(&message);
    ++f->next_token;
    if (error!=ESP_OK) {
        /* send consumes its reference even on a partial fan-out failure. Keep
         * the slot pinned until the remaining consumers return their leases.
         * Commit a dropped token so a late result cannot match a later frame. */
        s->result=ESP_ERR_NOT_FINISHED; s->completed=true; apply_completed(f);
        return error;
    }
    return ESP_OK;
}
static void apply_completed(camera_frames_t *f)
{
    for (;;) {
        camera_frame_slot_t *s=NULL;
        for (unsigned i=0;i<CAMERA_FRAME_SLOTS;++i)
            if (f->slots[i].awaiting && f->slots[i].completed && f->slots[i].token==f->next_result)
                s=&f->slots[i];
        if (!s) return;
        if (s->result==ESP_OK) { ++f->shown; f->bad_streak=0; }
        else if (s->result==ESP_ERR_INVALID_RESPONSE) {
            ++f->dropped;
            if (++f->bad_streak>=10) f->failed=true;
        } else if (s->result==ESP_ERR_NOT_FINISHED) ++f->dropped;
        else f->failed=true;
        s->awaiting=false; s->completed=false; ++f->next_result;
    }
}
bool camera_frames_result(camera_frames_t *f, const app_message_t *message)
{
    if (!f || !message || message->type!=APP_MESSAGE_UI_FRAME_RESULT ||
        message->source!=APP_ENDPOINT_UI || message->generation!=f->generation || message->lease) return false;
    for (unsigned i=0;i<CAMERA_FRAME_SLOTS;++i) {
        camera_frame_slot_t *s=&f->slots[i];
        if (s->awaiting && !s->completed && s->token==message->payload.command.token) {
            s->result=message->result; s->completed=true; apply_completed(f); return true;
        }
    }
    return false;
}
bool camera_frames_drained(const camera_frames_t *f)
{
    if (!f) return true;
    for (unsigned i=0;i<CAMERA_FRAME_SLOTS;++i)
        if (!atomic_load_explicit(&f->slots[i].returned,memory_order_acquire)) return false;
    return true;
}
