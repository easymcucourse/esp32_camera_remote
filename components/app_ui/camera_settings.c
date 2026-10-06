#include "camera_settings.h"
#include <stdio.h>


static const char *const labels[CAMERA_EXTRA_COUNT] = {
    "ASPECT", "DRIVE", "EFFECT", "DRO", "AF AREA", "WL FLASH", "WB TEMP", "WB AB RAW", "WB GM RAW"
};
void camera_extra_format(unsigned index, uint32_t value, char *text, size_t size)
{
    if (index >= CAMERA_EXTRA_COUNT) { if (size) text[0] = 0; return; }
    const char *name = NULL;
    if (value == UINT32_MAX) { snprintf(text, size, "%s --", labels[index]); return; }
    switch (index) {
    case 0:
        switch (value) { case 1: name="3:2"; break; case 2: name="16:9"; break;
            case 3: name="4:3"; break; case 4: name="1:1"; break; }
        break;
    case 1:
        switch (value) { case 1: name="SINGLE"; break; case 0x00010002: name="HI"; break;
            case 0x00018010: name="HI+"; break; case 0x00018012: name="LO"; break;
            case 0x00018015: name="MID"; break; }
        break;
    case 2: if (value == 0x8000) name="OFF"; break;
    case 3:
        if (value >= 0x11 && value <= 0x15) {
            snprintf(text, size, "DRO LV%u", (unsigned)value - 0x10); return;
        }
        switch (value) { case 1: name="OFF"; break; case 2: name="DRO"; break;
            case 0x10: name="DRO+"; break; case 0x1f: name="AUTO"; break;
            case 0x20: name="HDR AUTO"; break; }
        break;
    case 4:
        switch (value) { case 1: name="WIDE"; break; case 2: name="ZONE"; break;
            case 3: name="CENTER"; break; case 0x101: name="SPOT S"; break;
            case 0x102: name="SPOT M"; break; case 0x103: name="SPOT L"; break;
            case 0x104: name="EXPAND"; break; }
        break;
    case 6: snprintf(text, size, "WB TEMP %uK", (unsigned)value); return;
    }
    if (name) snprintf(text, size, "%s %s", labels[index], name);
    else snprintf(text, size, "%s 0X%08X", labels[index], (unsigned)value);
}
