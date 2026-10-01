#include "ptp_codes.h"
#include "ptp_dataset.h"
#include "ptpip_packet.h"

static bool ptp_string(const uint8_t *data, size_t size, size_t *offset,
                       char *output, size_t capacity)
{
    if (*offset >= size || !capacity) return false;
    unsigned count = data[(*offset)++];
    if (count > (size - *offset) / 2) return false;
    size_t written = 0;
    for (unsigned i = 0; i < count; ++i) {
        uint16_t ch = get16(data + *offset + i * 2);
        if (!ch) break;
        if (written + 1 < capacity) output[written++] = ch < 128 ? (char)ch : '?';
    }
    output[written] = 0;
    *offset += count * 2;
    return true;
}

bool ptp_parse_device_info(const uint8_t *data, size_t size,
                           char model[24], char firmware[24])
{
    size_t offset = 8;
    char scratch[64];
    if (size < offset || !ptp_string(data, size, &offset, scratch, sizeof(scratch)) ||
        offset + 2 > size) return false;
    offset += 2;
    for (int array = 0; array < 5; ++array) {
        if (offset + 4 > size) return false;
        uint32_t count = get32(data + offset);
        offset += 4;
        if (count > (size - offset) / 2) return false;
        offset += count * 2;
    }
    if (!ptp_string(data, size, &offset, scratch, sizeof(scratch)) ||
        !ptp_string(data, size, &offset, model, 24) ||
        !ptp_string(data, size, &offset, firmware, 24)) return false;
    return true;
}

