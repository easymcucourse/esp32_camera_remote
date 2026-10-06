#include "private/sony_control_encoder.h"
#include "sony_codes.h"
#include "ptpip_packet.h"

bool sony_encode_set_scalar(const sony_control_writer_t *writer, uint16_t property,
                     uint16_t type, uint32_t value, bool *accepted)
{
    if (!accepted) return false;
    *accepted = false;
    if (type < 1 || type > 6) return false;
    unsigned width = 1u << ((type - 1) / 2);
    /* Signed values use their width-limited wire bit pattern. */
    if ((width == 1 && value > UINT8_MAX) ||
        (width == 2 && value > UINT16_MAX)) return false;
    uint8_t data[4];
    put32(data, value);
    return writer->write(writer->context, SONY_OC_SET_CONTROL_DEVICE_A,
                            property, data, width, accepted);
}


bool sony_encode_manual_focus_step(const sony_control_writer_t *writer, int direction, bool *accepted)
{
    if (!accepted) return false;
    *accepted = false;
    if (direction != 1 && direction != -1) return false;
    uint16_t value = (uint16_t)(int16_t)direction;
    uint8_t data[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    return writer->write(writer->context, SONY_OC_SET_CONTROL_DEVICE_B,
                            SONY_DPC_MANUAL_FOCUS_ADJUST, data, sizeof(data), accepted);
}

static bool control_button(const sony_control_writer_t *writer, uint16_t property, bool pressed, bool *accepted)
{
    uint8_t data[2] = {pressed ? 2 : 1, 0};
    return writer->write(writer->context, SONY_OC_SET_CONTROL_DEVICE_B,
                            property, data, sizeof(data), accepted);
}
bool sony_encode_shutter_button(const sony_control_writer_t *writer, bool full, bool pressed, bool *accepted)
{
    return control_button(writer, full ? SONY_DPC_SHUTTER_RELEASE :
                           SONY_DPC_SHUTTER_HALF_RELEASE, pressed, accepted);
}
bool sony_encode_movie_record(const sony_control_writer_t *writer, bool recording, bool *accepted)
{
    return control_button(writer, SONY_DPC_MOVIE_RECORD, recording, accepted);
}
bool sony_encode_zoom(const sony_control_writer_t *writer, int direction, bool *accepted)
{
    if (!accepted) return false;
    *accepted = false;
    if (direction < -1 || direction > 1) return false;
    uint8_t data = (uint8_t)(int8_t)direction;
    return writer->write(writer->context, SONY_OC_SET_CONTROL_DEVICE_B,
                            SONY_DPC_ZOOM_OPERATION, &data, sizeof(data), accepted);
}
bool sony_encode_setting_step(const sony_control_writer_t *writer, uint16_t property, int direction, bool *accepted)
{
    if (!accepted) return false;
    *accepted = false;
    if ((property != SONY_DPC_SHUTTER_SPEED && property != SONY_DPC_F_NUMBER) ||
        (direction != -1 && direction != 1)) return false;
    uint8_t data = (uint8_t)(int8_t)direction;
    return writer->write(writer->context, SONY_OC_SET_CONTROL_DEVICE_B,
                            property, &data, sizeof(data), accepted);
}
