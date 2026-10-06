#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    void *context;
    bool (*write)(void *, uint16_t, uint16_t, const uint8_t *, size_t, bool *);
} sony_control_writer_t;
bool sony_encode_set_scalar(const sony_control_writer_t *writer, uint16_t property, uint16_t type, uint32_t value, bool *accepted);
bool sony_encode_manual_focus_step(const sony_control_writer_t *writer, int direction, bool *accepted);
bool sony_encode_shutter_button(const sony_control_writer_t *writer, bool full, bool pressed, bool *accepted);
bool sony_encode_movie_record(const sony_control_writer_t *writer, bool recording, bool *accepted);
bool sony_encode_zoom(const sony_control_writer_t *writer, int direction, bool *accepted);
bool sony_encode_setting_step(const sony_control_writer_t *writer, uint16_t property, int direction, bool *accepted);
