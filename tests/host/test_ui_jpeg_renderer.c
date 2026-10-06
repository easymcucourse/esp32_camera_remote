#include <assert.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include "../../components/app_ui/ui_jpeg_renderer.c"

atomic_uint ui_model_fps_tenths,ui_model_recording_state;
atomic_bool ui_model_settings_mode;
SemaphoreHandle_t display_mutex=(void *)1;
bool showing_connection;
static alignas(16) uint16_t buffers[2][UI_CANVAS_WIDTH*UI_CANVAS_HEIGHT];
static unsigned front,published,cancelled,opens,closes,overlays,active;
static unsigned allocations,frees;
static bool locked,ready=true,blocked,fail_alloc,fail_publish,fail_acquire;
static display_canvas_t *writer;
static unsigned width=1024,height=576;
static JRESULT prepare_error=JDR_OK,rom_error=JDR_OK;
static jpeg_error_t open_error=JPEG_ERR_OK,header_error=JPEG_ERR_OK;
static jpeg_error_t length_error=JPEG_ERR_OK,process_error=JPEG_ERR_OK;
static bool wrong_dimensions,wrong_length;
static int64_t now;
static const uint8_t input[]={0xff,0xd8,0xff,0xd9};
void fake_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
int64_t esp_timer_get_time(void) { return ++now; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem,TickType_t ticks)
{ assert(sem==display_mutex && ticks==portMAX_DELAY && !locked);locked=true;return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem)
{ assert(sem==display_mutex && locked);locked=false;return pdTRUE; }
void *heap_caps_malloc(size_t bytes,unsigned caps)
{ assert(bytes==4096 && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));void *p=fail_alloc?NULL:malloc(bytes);if(p)++allocations;return p; }
void heap_caps_free(void *p) { if(p)++frees;free(p); }
bool ui_render_enter(void) { if(blocked)return false;++active;return true; }
bool ui_render_surface_ready(void) { return ready; }
void ui_render_leave(void) { assert(active==1);--active; }
esp_err_t display_canvas_acquire(display_canvas_t *c,uint32_t timeout)
{
    assert(locked && !writer && !timeout);
    if(!ready || fail_acquire)return ESP_ERR_INVALID_STATE;
    writer=c;*c=(display_canvas_t){.pixels=buffers[1-front],.width=1024,.height=600,.stride_pixels=1024,.lease=1};
    return ESP_OK;
}
void display_canvas_cancel(display_canvas_t *c)
{ if(c->lease){assert(c==writer);writer=NULL;c->lease=0;++cancelled;} }
esp_err_t ui_render_publish_frame(display_canvas_t *c)
{
    assert(locked && c==writer && c->lease);writer=NULL;c->lease=0;++published;
    if(fail_publish){ready=false;return ESP_ERR_TIMEOUT;}
    front=1-front;return ESP_OK;
}
void ui_render_draw_settings_panel(uint16_t *pixels) { assert(pixels==buffers[1-front]);++overlays; }
void ui_render_draw_preview_status(uint16_t *pixels) { assert(pixels==buffers[1-front]);++overlays; }
void ui_overlay_record_border(uint16_t *pixels,unsigned w,unsigned h,bool recording)
{ assert(pixels==buffers[1-front] && w==1024 && h==600);(void)recording; }
JRESULT jd_prepare(JDEC *d,UINT (*read)(JDEC *,BYTE *,UINT),void *work,UINT size,void *context)
{
    assert(locked && writer && work && size==4096);d->device=context;d->width=width;d->height=height;
    BYTE data[8];assert(read(d,data,sizeof(data))==sizeof(input));assert(!memcmp(data,input,sizeof(input)));
    assert(read(d,data,1)==0);return prepare_error;
}
JRESULT jd_decomp(JDEC *d,UINT (*output)(JDEC *,void *,JRECT *),BYTE scale)
{
    assert(scale<=3);BYTE rgb[]={255,0,0};JRECT tile={0,0,0,0};
    assert(output(d,rgb,&tile)==1); /* A partial tile is written even on failure. */
    return rom_error;
}
jpeg_error_t jpeg_dec_open(jpeg_dec_config_t *c,jpeg_dec_handle_t *out)
{ assert(c->output_type==JPEG_PIXEL_FORMAT_RGB565_LE && !fast_decoder);if(open_error==JPEG_ERR_OK){++opens;*out=(void *)2;}return open_error; }
jpeg_error_t jpeg_dec_close(jpeg_dec_handle_t decoder)
{ assert(decoder==(void *)2);++closes;return JPEG_ERR_OK; }
jpeg_error_t jpeg_dec_parse_header(jpeg_dec_handle_t decoder,jpeg_dec_io_t *io,jpeg_dec_header_info_t *info)
{ assert(decoder==(void *)2 && io->inbuf_len==4);*info=(jpeg_dec_header_info_t){wrong_dimensions?1023:width,height};return header_error; }
jpeg_error_t jpeg_dec_get_outbuf_len(jpeg_dec_handle_t decoder,int *length)
{ assert(decoder==(void *)2);*length=wrong_length?2:(int)(width*height*2);return length_error; }
jpeg_error_t jpeg_dec_process(jpeg_dec_handle_t decoder,jpeg_dec_io_t *io)
{ assert(decoder==(void *)2);uint16_t *pixels=(uint16_t *)io->outbuf;pixels[0]=0xf800;if(process_error==JPEG_ERR_OK)for(unsigned i=0;i<width*height;++i)pixels[i]=0x07e0;return process_error; }
static void reset(void)
{
    assert(!writer && !locked && !active);ui_jpeg_reset();
    width=1024;height=576;prepare_error=rom_error=JDR_OK;
    open_error=header_error=length_error=process_error=JPEG_ERR_OK;
    wrong_dimensions=wrong_length=fail_alloc=fail_publish=blocked=fail_acquire=false;ready=true;
    atomic_store(&ui_model_settings_mode,false);
}
static uint64_t checksum(unsigned buffer);
static void failure(esp_err_t expected)
{
    unsigned pub=published,draw=overlays,previous_front=front;
    uint16_t first=buffers[front][0],image=buffers[front][12*1024];
    uint64_t old_pixels=checksum(front);
    assert(app_ui_show_jpeg(input,sizeof(input))==expected);
    assert(published==pub && overlays==draw && front==previous_front);
    assert(buffers[front][0]==first && buffers[front][12*1024]==image);
    assert(checksum(front)==old_pixels);
    assert(!writer && !locked && !active && !jpeg_pixels);
}
static uint64_t checksum(unsigned buffer)
{
    uint64_t value=UINT64_C(14695981039346656037);
    for(unsigned i=0;i<UI_CANVAS_WIDTH*UI_CANVAS_HEIGHT;++i)
        value=(value^buffers[buffer][i])*UINT64_C(1099511628211);
    return value;
}
static void good(void)
{ unsigned pub=published;assert(app_ui_show_jpeg(input,sizeof(input))==ESP_OK && published==pub+1);assert(!writer && !locked && !active); }
#if CONFIG_REMOTE_DBG_SIM
static jpeg_error_t enc_error;
static unsigned enc_opened,enc_closed;
jpeg_error_t jpeg_enc_open(jpeg_enc_config_t *c,jpeg_enc_handle_t *encoder)
{ assert(c->width==1024 && c->height==576 && c->src_type==JPEG_PIXEL_FORMAT_RGB565_LE && c->subsampling==JPEG_SUBSAMPLE_422 && c->quality==80);*encoder=(void *)3;++enc_opened;return JPEG_ERR_OK; }
jpeg_error_t jpeg_enc_process(const jpeg_enc_handle_t encoder,const uint8_t *source,int bytes,uint8_t *out,int capacity,int *length)
{ assert(encoder==(void *)3 && source==(uint8_t *)buffers[1-front] && bytes==1024*576*2 && capacity==4);memcpy(out,input,4);*length=4;return enc_error; }
jpeg_error_t jpeg_enc_close(jpeg_enc_handle_t encoder)
{ assert(encoder==(void *)3);++enc_closed;return JPEG_ERR_OK; }
#endif
int main(void)
{
    fail_alloc=true;assert(ui_jpeg_init()==ESP_ERR_NO_MEM && !jpeg_work && !fast_decoder);
    fail_alloc=false;open_error=JPEG_ERR_NO_MEM;
    assert(ui_jpeg_init()==ESP_ERR_NO_MEM && !jpeg_work && !fast_decoder);
    open_error=JPEG_ERR_OK;
    assert(ui_jpeg_init()==ESP_OK && jpeg_work && fast_decoder);
    allocations=frees=0;allocations=1;
    assert(ui_jpeg_init()==ESP_ERR_INVALID_STATE);
    memset(buffers,0x55,sizeof(buffers));
    assert(app_ui_show_jpeg(NULL,4)==ESP_ERR_INVALID_ARG);
    assert(app_ui_show_jpeg(input,3)==ESP_ERR_INVALID_RESPONSE);
    reset();blocked=true;failure(ESP_ERR_INVALID_STATE);
    reset();ready=false;failure(ESP_ERR_INVALID_STATE);
    reset();fail_acquire=true;failure(ESP_ERR_INVALID_STATE);
    reset();fail_alloc=true;good();assert(allocations==1);
    reset();prepare_error=JDR_FMT1;failure(ESP_ERR_INVALID_RESPONSE);prepare_error=JDR_OK;good();
    reset();width=65535;failure(ESP_ERR_INVALID_RESPONSE);
    reset();open_error=JPEG_ERR_NO_MEM;good();assert(opens==1);open_error=JPEG_ERR_OK;
    reset();header_error=JPEG_ERR_FAIL;failure(ESP_ERR_INVALID_RESPONSE);assert(fast_decoder);header_error=JPEG_ERR_OK;good();
    reset();wrong_dimensions=true;failure(ESP_ERR_INVALID_RESPONSE);wrong_dimensions=false;good();
    reset();wrong_length=true;failure(ESP_ERR_INVALID_RESPONSE);wrong_length=false;good();
    reset();length_error=JPEG_ERR_NO_MEM;failure(ESP_ERR_NO_MEM);length_error=JPEG_ERR_OK;good();
    reset();process_error=JPEG_ERR_FAIL;failure(ESP_ERR_INVALID_RESPONSE);assert(fast_decoder);process_error=JPEG_ERR_OK;good();
    reset();process_error=JPEG_ERR_NO_MEM;failure(ESP_ERR_NO_MEM);process_error=JPEG_ERR_OK;good();
    reset();good();unsigned before=closes;width=640;height=480;rom_error=JDR_INP;
    failure(ESP_ERR_INVALID_RESPONSE);assert(closes==before && fast_decoder);rom_error=JDR_OK;good();
    reset();good();unsigned old_front=front,pub=published;fail_publish=true;
    assert(app_ui_show_jpeg(input,4)==ESP_ERR_TIMEOUT && published==pub+1 && front==old_front);
    assert(!writer && !locked && !active);failure(ESP_ERR_INVALID_STATE);
    ready=true;fail_publish=false;ui_jpeg_reset();good();
    reset();atomic_store(&ui_model_settings_mode,true);good();
    assert(buffers[front][0]==0x07e0 && buffers[front][431*1024+767]==0x07e0);
    assert(buffers[front][768]==0 && buffers[front][432*1024]==0);
    reset();unsigned alloc_before=allocations,open_before=opens;
    void *work=NULL;
    for(unsigned frame=0;frame<32;++frame){
        bool settings=(frame&1)!=0;unsigned previous=front;
        uint64_t old_pixels=checksum(previous);
        atomic_store(&ui_model_settings_mode,settings);good();
        assert(front==1-previous && checksum(previous)==old_pixels);
        if(!frame)work=jpeg_work;
        assert(work && jpeg_work==work && allocations==alloc_before && opens==open_before);
        assert(!jpeg_pixels);
        if(settings){
            assert(buffers[front][0]==0x07e0 && buffers[front][431*1024+767]==0x07e0);
            assert(buffers[front][768]==0 && buffers[front][432*1024]==0);
        }else{
            assert(buffers[front][0]==0 && buffers[front][12*1024]==0x07e0);
            assert(buffers[front][587*1024+1023]==0x07e0 && buffers[front][588*1024]==0);
        }
    }
    reset();assert(opens==1 && !closes && jpeg_work && fast_decoder && allocations==1 && !frees && cancelled>=8);
#if CONFIG_REMOTE_DBG_SIM
    uint8_t jpeg[4];size_t length=99;unsigned last_pub=published,last_front=front;
    enc_error=JPEG_ERR_NO_MEM;
    assert(app_ui_test_jpeg(jpeg,sizeof(jpeg),&length)==ESP_ERR_NO_MEM && !length);
    assert(published==last_pub && front==last_front && !writer && !locked && !active);
    enc_error=JPEG_ERR_OK;assert(app_ui_test_jpeg(jpeg,sizeof(jpeg),&length)==ESP_OK && length==4);
    assert(published==last_pub && front==last_front && !writer && !locked && !active);
    /* Generated JPEG returns through the production decode/overlay/publish path. */
    assert(app_ui_show_jpeg(jpeg,length)==ESP_OK && published==last_pub+1);
    reset();assert(enc_opened==enc_closed && opens==closes+1);
#else
    uint8_t jpeg[4];size_t length=99;
    assert(app_ui_test_jpeg(jpeg,sizeof(jpeg),&length)==ESP_ERR_NOT_SUPPORTED);
#endif
    assert(allocations==1 && !frees);
    jpeg_dec_close(fast_decoder);fast_decoder=NULL;heap_caps_free(jpeg_work);jpeg_work=NULL;
    return 0;
}
