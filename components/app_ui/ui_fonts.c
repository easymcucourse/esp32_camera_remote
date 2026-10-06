#include "ui_fonts.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_SYSTEM_H

#define CACHE_ENTRIES 384
#define CACHE_BYTES (256 * 1024)
#define MIN_SIZE 8
#define MAX_SIZE 96

extern const uint8_t inter_start[] asm("_binary_inter_ui_ttf_start");
extern const uint8_t inter_end[] asm("_binary_inter_ui_ttf_end");
extern const uint8_t han_start[] asm("_binary_han_ui_otf_start");
extern const uint8_t han_end[] asm("_binary_han_ui_otf_end");
extern const uint8_t mono_start[] asm("_binary_mono_ui_ttf_start");
extern const uint8_t mono_end[] asm("_binary_mono_ui_ttf_end");

typedef struct {
    uint32_t codepoint, used;
    uint16_t size, face_id, width, height;
    int16_t left, top, advance;
    FT_UInt index;
    uint8_t *alpha;
    bool valid;
} cached_glyph_t;

static FT_Library library;
static FT_Face faces[3];
static struct FT_MemoryRec_ memory;
static cached_glyph_t *cache;
static size_t cache_bytes;
static uint32_t use_counter;
static const char *TAG = "ui_fonts";

static void *ft_alloc(FT_Memory mem, long size)
{
    return size > 0 ? heap_caps_malloc_prefer(size, 2,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) : NULL;
}

static void ft_free(FT_Memory mem, void *block)
{
    heap_caps_free(block);
}

static void *ft_realloc(FT_Memory mem, long current, long requested, void *block)
{
    if (requested <= 0) {
        heap_caps_free(block);
        return NULL;
    }
    void *new_block = ft_alloc(mem, requested);
    if (new_block) {
        if (block) memcpy(new_block, block, current < requested ? current : requested);
        heap_caps_free(block);
    }
    return new_block;
}

static int size_clamped(int size)
{
    return size < MIN_SIZE ? MIN_SIZE : size > MAX_SIZE ? MAX_SIZE : size;
}

/* Invalid UTF-8 consumes at least one byte and yields a replacement glyph. */
static uint32_t next_codepoint(const char **cursor)
{
    const unsigned char *p = (const unsigned char *)*cursor;
    uint32_t value = *p++;
    unsigned count;
    if (value < 0x80) {
        *cursor = (const char *)p;
        return value;
    }
    uint32_t minimum;
    if (value >= 0xC2 && value <= 0xDF) { count = 1; minimum = 0x80; value &= 0x1F; }
    else if (value >= 0xE0 && value <= 0xEF) { count = 2; minimum = 0x800; value &= 0x0F; }
    else if (value >= 0xF0 && value <= 0xF4) { count = 3; minimum = 0x10000; value &= 7; }
    else { *cursor = (const char *)p; return 0xFFFD; }
    for (unsigned i = 0; i < count; ++i) {
        if (*p < 0x80 || *p > 0xBF) {
            *cursor = (const char *)p;
            return 0xFFFD;
        }
        value = (value << 6) | (*p++ & 0x3F);
    }
    *cursor = (const char *)p;
    return value < minimum || value > 0x10FFFF ||
        (value >= 0xD800 && value <= 0xDFFF) ? 0xFFFD : value;
}

static int face_for(uint32_t cp, bool numbers)
{
    if (cp >= 0x2E80 && FT_Get_Char_Index(faces[1], cp)) return 1;
    if (numbers && ((cp >= '0' && cp <= '9') || cp == '.' || cp == '/' ||
                    cp == '+' || cp == '-')) return 2;
    return FT_Get_Char_Index(faces[0], cp) ? 0 : 1;
}

static void evict(cached_glyph_t *glyph)
{
    cache_bytes -= (size_t)glyph->width * glyph->height;
    heap_caps_free(glyph->alpha);
    memset(glyph, 0, sizeof(*glyph));
}

static cached_glyph_t *oldest(void)
{
    cached_glyph_t *result = NULL;
    for (int i = 0; i < CACHE_ENTRIES; ++i)
        if (cache[i].valid && (!result || cache[i].used < result->used)) result = &cache[i];
    return result;
}

static cached_glyph_t *get_glyph(uint32_t cp, int size, bool numbers)
{
    int face_id = face_for(cp, numbers);
    cached_glyph_t *entry = NULL;
    for (int i = 0; i < CACHE_ENTRIES; ++i) {
        if (cache[i].valid && cache[i].codepoint == cp && cache[i].size == size &&
            cache[i].face_id == face_id) {
            cache[i].used = ++use_counter;
            return &cache[i];
        }
        if (!cache[i].valid && !entry) entry = &cache[i];
    }
    FT_Face face = faces[face_id];
    FT_UInt index = FT_Get_Char_Index(face, cp);
    if (!index) index = FT_Get_Char_Index(face, '?');
    if (FT_Set_Pixel_Sizes(face, 0, size) ||
        FT_Load_Glyph(face, index, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) return NULL;
    FT_GlyphSlot slot = face->glyph;
    FT_Bitmap *bitmap = &slot->bitmap;
    if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY && bitmap->width) return NULL;
    const size_t bytes = (size_t)bitmap->width * bitmap->rows;
    if (bytes > CACHE_BYTES) return NULL;
    if (!entry) { entry = oldest(); if (!entry) return NULL; evict(entry); }
    while (cache_bytes + bytes > CACHE_BYTES) {
        cached_glyph_t *victim = oldest();
        if (!victim) return NULL;
        evict(victim);
    }
    uint8_t *alpha = bytes ? heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
    if (bytes && !alpha) return NULL;
    for (unsigned row = 0; row < bitmap->rows; ++row) {
        const uint8_t *source = bitmap->pitch >= 0 ? bitmap->buffer + row * bitmap->pitch :
            bitmap->buffer + (bitmap->rows - row - 1) * -bitmap->pitch;
        memcpy(alpha + row * bitmap->width, source, bitmap->width);
    }
    *entry = (cached_glyph_t){
        .codepoint = cp, .used = ++use_counter, .size = size, .face_id = face_id,
        .width = bitmap->width, .height = bitmap->rows, .left = slot->bitmap_left,
        .top = slot->bitmap_top, .advance = (slot->advance.x + 32) >> 6,
        .index = index, .alpha = alpha, .valid = true,
    };
    cache_bytes += bytes;
    return entry;
}

static int ascender(int size)
{
    int result = 0;
    for (int i = 0; i < 3; ++i) {
        FT_Set_Pixel_Sizes(faces[i], 0, size);
        int value = (faces[i]->size->metrics.ascender + 63) >> 6;
        if (value > result) result = value;
    }
    return result;
}

int ui_fonts_line_height(int pixel_size)
{
    if (!library) return 0;
    int size = size_clamped(pixel_size), result = 0;
    for (int i = 0; i < 3; ++i) {
        FT_Set_Pixel_Sizes(faces[i], 0, size);
        int value = (faces[i]->size->metrics.height + 63) >> 6;
        if (value > result) result = value;
    }
    return result;
}

static int kerning(int previous_face, FT_UInt previous_index,
                   const cached_glyph_t *glyph, int size)
{
    if (previous_face != glyph->face_id || !previous_index) return 0;
    FT_Face face = faces[glyph->face_id];
    if (!FT_HAS_KERNING(face)) return 0;
    FT_Set_Pixel_Sizes(face, 0, size);
    FT_Vector delta;
    if (FT_Get_Kerning(face, previous_index, glyph->index, FT_KERNING_DEFAULT, &delta)) return 0;
    return delta.x >> 6;
}

/* Grayscale coverage composited into the existing RGB565 background. */
static inline uint16_t blend(uint16_t background, uint16_t foreground, unsigned alpha)
{
    if (alpha == 255) return foreground;
    unsigned inverse = 255 - alpha;
    unsigned r = (((foreground >> 11) * alpha + (background >> 11) * inverse + 127) / 255);
    unsigned g = ((((foreground >> 5) & 63) * alpha + ((background >> 5) & 63) * inverse + 127) / 255);
    unsigned b = (((foreground & 31) * alpha + (background & 31) * inverse + 127) / 255);
    return (r << 11) | (g << 5) | b;
}

static int render(uint16_t *pixels, int width, int height, int left, int top,
                  const char *text, int pixel_size, uint16_t color,
                  bool numbers, int clip_right)
{
    if (!library || !text) return 0;
    const int size = size_clamped(pixel_size);
    int x = left, maximum = 0, previous_face = -1;
    FT_UInt previous_index = 0;
    int baseline = top + ascender(size);
    const int line_height = ui_fonts_line_height(size);
    if (clip_right > width) clip_right = width;
    while (*text) {
        uint32_t cp = next_codepoint(&text);
        if (cp == '\n') {
            if (x - left > maximum) maximum = x - left;
            x = left; baseline += line_height; previous_face = -1; previous_index = 0;
            continue;
        }
        if (cp == '\r') continue;
        cached_glyph_t *glyph = get_glyph(cp, size, numbers);
        if (!glyph) { x += size / 2; previous_face = -1; previous_index = 0; continue; }
        x += kerning(previous_face, previous_index, glyph, size);
        if (pixels) {
            for (int row = 0; row < glyph->height; ++row) {
                int y = baseline - glyph->top + row;
                if (y < 0 || y >= height) continue;
                for (int column = 0; column < glyph->width; ++column) {
                    int px = x + glyph->left + column;
                    if (px < left || px < 0 || px >= clip_right) continue;
                    unsigned alpha = glyph->alpha[row * glyph->width + column];
                    if (alpha) {
                        uint16_t *destination = pixels + (size_t)y * width + px;
                        *destination = blend(*destination, color, alpha);
                    }
                }
            }
        }
        x += glyph->advance;
        previous_face = glyph->face_id;
        previous_index = glyph->index;
    }
    return x - left > maximum ? x - left : maximum;
}

int ui_fonts_measure(const char *text, int pixel_size, bool parameter_numbers)
{
    return render(NULL, 0, 0, 0, 0, text, pixel_size, 0, parameter_numbers, 0);
}

void ui_fonts_draw(uint16_t *pixels, int width, int height, int left, int top,
                   const char *text, int pixel_size, uint16_t color,
                   bool parameter_numbers, int clip_right)
{
    if (pixels && width > 0 && height > 0)
        render(pixels, width, height, left, top, text, pixel_size, color,
               parameter_numbers, clip_right);
}

esp_err_t ui_fonts_init(void)
{
    if (library) return ESP_OK;
    memory = (struct FT_MemoryRec_){.alloc = ft_alloc, .free = ft_free, .realloc = ft_realloc};
    if (FT_New_Library(&memory, &library)) return ESP_ERR_NO_MEM;
    FT_Add_Default_Modules(library);
    const uint8_t *starts[] = {inter_start, han_start, mono_start};
    const uint8_t *ends[] = {inter_end, han_end, mono_end};
    cache = heap_caps_calloc(CACHE_ENTRIES, sizeof(*cache), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!cache) goto failed;
    for (int i = 0; i < 3; ++i) {
        if (FT_New_Memory_Face(library, starts[i], ends[i] - starts[i], 0, &faces[i]) ||
            FT_Select_Charmap(faces[i], FT_ENCODING_UNICODE)) goto failed;
        ESP_LOGI(TAG, "loaded %s: %u bytes", faces[i]->family_name,
                 (unsigned)(ends[i] - starts[i]));
    }
    /* Warm recurring live-view labels/digits outside the JPEG frame path. */
    ui_fonts_measure("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .:/+-", 18, true);
    ui_fonts_measure("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .:/+-", 17, true);
    ESP_LOGI(TAG, "grayscale antialiasing ready; glyph cache <=256 KiB in PSRAM");
    return ESP_OK;
failed:
    for (int i = 0; i < 3; ++i) { if (faces[i]) FT_Done_Face(faces[i]); faces[i] = NULL; }
    heap_caps_free(cache); cache = NULL;
    FT_Done_Library(library); library = NULL;
    return ESP_FAIL;
}
