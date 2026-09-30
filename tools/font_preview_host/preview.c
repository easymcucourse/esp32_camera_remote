/* Run the actual firmware text renderer on Windows with a host FreeType DLL.
 * Compile from the repo root so these .incbin paths match the ESP-IDF assets.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_fonts.h"

#define FONT_BLOB(name, file) __asm__( ".section .rdata\n" \
    ".global _binary_" name "_start\n" \
    "_binary_" name "_start:\n.incbin \"components/board_7b/fonts/" file "\"\n" \
    ".global _binary_" name "_end\n" \
    "_binary_" name "_end:\n.text\n")
FONT_BLOB("inter_ui_ttf", "inter_ui.ttf");
FONT_BLOB("han_ui_otf", "han_ui.otf");
FONT_BLOB("mono_ui_ttf", "mono_ui.ttf");

#define WIDTH 1024
#define HEIGHT 600
#define GUARD 64
static uint16_t storage[WIDTH * HEIGHT + GUARD * 2];
static uint16_t *pixels = storage + GUARD;

static void fill(uint16_t value)
{
    for (int i = 0; i < WIDTH * HEIGHT; ++i) pixels[i] = value;
}

static void draw(int x, int y, const char *text, int size, uint16_t color, bool numbers)
{
    ui_fonts_draw(pixels, WIDTH, HEIGHT, x, y, text, size, color, numbers, WIDTH - 24);
}

static void save(const char *path)
{
    FILE *out = fopen(path, "wb");
    assert(out);
    fprintf(out, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
        uint16_t p = pixels[i];
        unsigned char rgb[] = {(p >> 11) * 255 / 31,
            ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31};
        fwrite(rgb, 1, sizeof(rgb), out);
    }
    fclose(out);
}

static void verify(void)
{
    for (int i = 0; i < GUARD; ++i) storage[i] = storage[GUARD + WIDTH * HEIGHT + i] = 0xA55A;
    /* Mono numeric strings retain identical widths as their values change. */
    for (int size = 8; size <= 96; size += 4) {
        assert(ui_fonts_measure("1111", size, true) == ui_fonts_measure("8888", size, true));
        assert(ui_fonts_measure("Wi-Fi 相机", size, false) > 0);
        draw(-35, -40, "中文 ISO 1/250", size, 0xFFFF, true);
        draw(1000, 585, "Camera", size, 0xFFFF, false);
    }
    /* Exercise UTF-8 errors, fallback glyphs and more cache keys than slots. */
    draw(0, 0, "\xE4\xB8", 18, 0xFFFF, false);
    draw(0, 0, "\xFF\xF0\x80\x80\x80", 18, 0xFFFF, false);
    for (int size = 8; size <= 96; ++size)
        ui_fonts_measure("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789中文", size, true);
    for (int i = 0; i < GUARD; ++i) {
        assert(storage[i] == 0xA55A);
        assert(storage[GUARD + WIDTH * HEIGHT + i] == 0xA55A);
    }
    fill(0);
    ui_fonts_draw(pixels, WIDTH, HEIGHT, 24, 20, "Camera 相机", 40, 0xFFFF, false, 180);
    int partial = 0;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (x < 24 || x >= 180) assert(pixels[y * WIDTH + x] == 0);
            if (pixels[y * WIDTH + x] != 0 && pixels[y * WIDTH + x] != 0xFFFF) ++partial;
        }
    }
    assert(partial > 0);
    printf("PASS: numeric alignment, UTF-8, cache eviction, framebuffer guards, clipping and antialiasing\n");
}

int main(void)
{
    assert(ui_fonts_init() == ESP_OK);
    verify();
    fill(0x1082);
    draw(48, 40, "easymcucourse camera station", 40, 0xFFFF, false);
    draw(48, 130, "SSID: esp32camap", 32, 0xFFFF, false);
    draw(48, 190, "Password: 00000000", 32, 0xFFFF, false);
    draw(48, 270, "Expend unit (ATOM): Connected", 30, 0x07E0, false);
    draw(48, 330, "Controller (DS4): Disconnected", 30, 0xF800, false);
    draw(48, 410, "Starting Wi-Fi hotspot...", 24, 0x07FF, false);
    draw(48, 466, "Connect camera to this Wi-Fi.", 24, 0x7BEF, false);
    draw(48, 516, "Enable PC Remote on camera.", 24, 0x7BEF, false);
    save("build/font-connection.ppm");

    fill(0x1082);
    draw(40, 16, "Inter + Source Han Sans + JetBrains Mono", 28, 0xFFFF, false);
    draw(40, 66, "18 px   Camera ready / 相机已连接", 18, 0xFFFF, false);
    draw(40, 110, "24 px   Camera ready / 相机已连接", 24, 0xFFFF, false);
    draw(40, 164, "32 px   Camera ready / 相机已连接", 32, 0xFFFF, false);
    draw(40, 230, "40 px   Camera ready / 相机已连接", 40, 0xFFFF, false);
    draw(40, 310, "ISO 100   1/250   F2.80   +0.3 EV", 28, 0xFFE0, true);
    draw(40, 366, "ISO 888   1/888   F8.88   +8.8 EV", 28, 0xFFE0, true);
    draw(40, 436, "12.3 FPS / WIFI -56 DBM", 20, 0x07FF, true);
    draw(40, 490, "灰度抗锯齿 · 字形按字号重新渲染", 24, 0x7BEF, false);
    save("build/font-scales.ppm");

    fill(0x1082);
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 768; x < WIDTH; ++x) pixels[y * WIDTH + x] = x == 768 ? 0x7BEF : 0x0841;
    const char *lines[] = {"CAMERA SETTINGS", "WIFI -56 DBM", "FPS 12.3", "CAM ILCE-7M4",
        "FW 2.01", "MODE APERTURE PRIORITY", "ISO 100", "SHUTTER 1/250", "APERTURE F2.80",
        "EV +0.3", "WB AUTO", "FOCUS AF-C", "METER MULTI", "FLASH OFF"};
    for (int i = 0; i < 14; ++i) {
        int size = 18;
        while (size > 12 && ui_fonts_measure(lines[i], size, true) > 240) --size;
        ui_fonts_draw(pixels, WIDTH, HEIGHT, 776, 12 + i * 38, lines[i], size,
            i == 0 ? 0x07FF : i >= 5 ? 0xFFE0 : 0xFFFF, true, 1016);
    }
    save("build/font-settings.ppm");
    return 0;
}
