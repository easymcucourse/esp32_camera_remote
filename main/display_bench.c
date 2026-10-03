#include "display_bench.h"
#include "sdkconfig.h"
#if CONFIG_REMOTE_DBG_SIM
#include <stdatomic.h>
#include <string.h>
#include "board_7b.h"
#include "camera_pair.h"
#include "maint_mode.h"
#include "atom_link.h"
#include "lcd_sim.h"
#include "debug_console.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"

static atomic_bool busy, done, ready;
void display_bench_ready(void) { atomic_store(&ready,true); }
static uint32_t token;
static esp_err_t result;
static unsigned frames, bytes;
static int64_t elapsed;

static void worker(void *arg)
{
    (void)arg;
    camera_debug_status_t camera; camera_debug_get_status(&camera);
    bool resume = camera.busy && !camera.stopped;
    bool lease = false;
    uint8_t *jpeg = NULL;
    result = ESP_ERR_INVALID_STATE; frames = bytes = 0; elapsed = 0;
    if (maint_mode_is_on() || lcd_sim_enabled() || !camera_maintenance_acquire(3000)) goto finished;
    lease = true;
    jpeg = heap_caps_aligned_alloc(16, 512*1024, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    result = ESP_ERR_NO_MEM;
    if (!jpeg) goto finished;
    size_t length=0;
    result = board_7b_test_jpeg(jpeg,512*1024,&length);
    if (result != ESP_OK) goto finished;
    bytes=(unsigned)length;
    board_7b_set_sim(true);
    int64_t start=esp_timer_get_time();
    for (unsigned i=0; i<20; ++i) {
        result=board_7b_show_jpeg(jpeg,bytes);
        if (result!=ESP_OK) break;
        ++frames;
        vTaskDelay(1);
    }
    elapsed=esp_timer_get_time()-start;
    board_7b_set_sim(false);
finished:
    heap_caps_free(jpeg);
    if (lease) {
        /* Discard the synthetic image and restore normal connection metadata. */
        board_7b_show_connection("Display benchmark finished - waiting for camera...");
        camera_maintenance_release();
        if (resume) camera_jpeg_start();
    }
    atomic_store(&done,true);
    vTaskDeleteWithCaps(NULL);
}
bool display_bench_command(int argc, char **argv)
{
    if (argc==3 && !strcmp(argv[0],"atom") && !strcmp(argv[1],"sim") && atomic_load(&busy)) {
        debug_printf("[dbg] ERR SIM transport cannot change during display benchmark\n");return true;
    }
    if (argc!=2 || strcmp(argv[0],"display") || strcmp(argv[1],"bench")) return false;
    atom_link_status_t atom; atom_link_get_status(&atom);
    bool expected=false;
    if (!atomic_load(&ready) || maint_mode_is_on() || lcd_sim_enabled() || atom.sim || !atomic_compare_exchange_strong(&busy,&expected,true)) {
        debug_printf("[dbg] ERR display bench requires maintenance off, SIM off and idle benchmark\n");return true;
    }
    token=debug_async_token();
    if (xTaskCreatePinnedToCoreWithCaps(worker,"display_bench",32768,NULL,4,NULL,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)!=pdPASS) {
        atomic_store(&busy,false);debug_printf("[dbg] ERR display bench memory\n");return true;
    }
    debug_printf("[dbg] OK display bench queued token=%lu duration=30000ms synthetic=1\n",(unsigned long)token);
    return true;
}
void display_bench_poll(void)
{
    if (!atomic_exchange(&done,false)) return;
    debug_printf("[dbg] %s display_bench token=%lu synthetic=1 frames=%u JPEG=%u elapsed_ms=%lu fps=%.2f error=%s\n",
                 result==ESP_OK&&frames==20?"DONE":"FAIL",(unsigned long)token,frames,bytes,
                 (unsigned long)(elapsed/1000),elapsed?(double)frames*1000000/elapsed:0,esp_err_to_name(result));
    atomic_store(&busy,false);
}
#else
bool display_bench_command(int argc,char **argv) { (void)argc;(void)argv;return false; }
void display_bench_poll(void) {}
void display_bench_ready(void) {}
#endif
