#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "camera_identity.h"
#include "nvs.h"
typedef struct { uint8_t guid[32],peer[32]; size_t guid_size,peer_size; } store_t;
static store_t store, staged;
static unsigned random_calls, commits;
static bool fail_commit, fail_open, fail_erase;
int nvs_open(const char *name,int mode,nvs_handle_t *out) {
    assert(!strcmp(name,"sony_remote") && mode==NVS_READWRITE);
    if (fail_open) return 9;
    staged=store;*out=1;return ESP_OK;
}
int nvs_get_blob(nvs_handle_t handle,const char *key,void *data,size_t *size) {
    assert(handle==1); bool guid=!strcmp(key,"guid");assert(guid||!strcmp(key,"peer"));
    size_t stored=guid?staged.guid_size:staged.peer_size;
    if (!stored) return ESP_ERR_NVS_NOT_FOUND;
    if (!data) { *size=stored;return ESP_OK; }
    if (*size<stored) { *size=stored;return ESP_ERR_NVS_INVALID_LENGTH; }
    memcpy(data,guid?staged.guid:staged.peer,stored);*size=stored;return ESP_OK;
}
int nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size) {
    assert(handle==1 && size<=32);bool guid=!strcmp(key,"guid");assert(guid||!strcmp(key,"peer"));
    memcpy(guid?staged.guid:staged.peer,data,size);
    if (guid) staged.guid_size=size; else staged.peer_size=size;
    return ESP_OK;
}
int nvs_commit(nvs_handle_t handle) { assert(handle==1);++commits;if(fail_commit)return 9;store=staged;return ESP_OK; }
int nvs_erase_all(nvs_handle_t handle) { assert(handle==1);if(fail_erase)return 9;memset(&staged,0,sizeof(staged));return ESP_OK; }
void nvs_close(nvs_handle_t handle) { assert(handle==1); }
void esp_fill_random(void *data,size_t size) { ++random_calls;memset(data,0x40+random_calls,size); }
int main(void) {
    camera_identity_t a,b;
    uint8_t mac[6]={1,2,3,4,5,6},peer[16]={9}, other[6]={2};
    assert(camera_identity_load(&a)&&!a.paired&&random_calls==1);
    assert(camera_identity_load(&b)&&!b.paired&&!memcmp(a.guid,b.guid,16)&&random_calls==1);
    /* Existing guid-only installations must retain their authorized GUID. */
    fail_commit=true;
    assert(!camera_identity_confirm(&a,mac,peer)&&!a.paired&&!store.peer_size);
    fail_commit=false;
    assert(camera_identity_confirm(&a,mac,peer)&&a.paired);
    assert(camera_identity_load(&b)&&b.paired&&!memcmp(a.guid,b.guid,16));
    unsigned before=commits;
    assert(camera_identity_confirm(&b,mac,peer)&&commits==before);
    assert(!camera_identity_confirm(&b,other,peer));
    peer[0]++;assert(!camera_identity_confirm(&b,mac,peer));
    store.peer_size=21;assert(!camera_identity_load(&b));
    store.peer_size=23;assert(!camera_identity_load(&b));
    store.peer_size=22;store.guid_size=15;assert(!camera_identity_load(&b));
    store.guid_size=0;before=commits;assert(!camera_identity_load(&b)&&commits==before);
    fail_erase=true;before=commits;
    assert(!camera_identity_forget()&&commits==before&&store.peer_size==22);
    fail_erase=false;fail_commit=true;
    assert(!camera_identity_forget()&&store.peer_size==22);
    fail_commit=false;
    assert(camera_identity_forget()&&!store.guid_size&&!store.peer_size);
    assert(camera_identity_load(&b)&&random_calls==2&&!b.paired&&memcmp(a.guid,b.guid,16));
    fail_open=true;assert(!camera_identity_load(&b)&&!camera_identity_forget());
    puts("identity migration, pairing persistence, corruption and reset tests passed");return 0;
}
