#include "sony_ext.h"
#include "ptp_session.h"
#include "sony_control_encoder.h"
#include "sony_exposure_encoder.h"
typedef struct { int fd; uint32_t transaction; } legacy_t;
static bool write(void *context, uint16_t opcode, uint16_t property, const uint8_t *data, size_t size, bool *accepted)
{ legacy_t *old = context; return ptp_send_data(old->fd, old->transaction, opcode, property, data, size, accepted); }
bool sony_set_scalar(int fd, uint32_t transaction, uint16_t property, uint16_t type, uint32_t value, bool *accepted)
{ legacy_t old = {fd, transaction}; sony_control_writer_t writer = {&old, write};
   return sony_encode_set_scalar(&writer, property, type, value, accepted); }
bool sony_set_exposure_mode(int fd, uint32_t transaction, uint32_t value, bool *accepted)
{ legacy_t old = {fd, transaction}; sony_control_writer_t writer = {&old, write};
   return sony_encode_set_exposure_mode(&writer, value, accepted); }
bool sony_manual_focus_step(int fd, uint32_t transaction, int direction, bool *accepted)
{ legacy_t old = {fd, transaction}; sony_control_writer_t writer = {&old, write};
   return sony_encode_manual_focus_step(&writer, direction, accepted); }
bool sony_shutter_button(int fd, uint32_t transaction, bool full, bool pressed, bool *accepted)
{ legacy_t old = {fd, transaction}; sony_control_writer_t writer = {&old, write};
   return sony_encode_shutter_button(&writer, full, pressed, accepted); }
bool sony_movie_record(int fd, uint32_t transaction, bool recording, bool *accepted)
{ legacy_t old = {fd, transaction}; sony_control_writer_t writer = {&old, write};
   return sony_encode_movie_record(&writer, recording, accepted); }
bool sony_zoom(int fd, uint32_t transaction, int direction, bool *accepted)
{ legacy_t old = {fd, transaction}; sony_control_writer_t writer = {&old, write};
   return sony_encode_zoom(&writer, direction, accepted); }
bool sony_setting_step(int fd, uint32_t transaction, uint16_t property, int direction, bool *accepted)
{ legacy_t old = {fd, transaction}; sony_control_writer_t writer = {&old, write};
   return sony_encode_setting_step(&writer, property, direction, accepted); }
