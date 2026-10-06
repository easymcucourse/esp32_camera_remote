/* Original convenience wrapper retained only for existing wire fixtures. */
#include "sony_exposure_encoder.h"
#include "sony_codes.h"

bool sony_encode_set_exposure_mode(const sony_control_writer_t *writer, uint32_t value, bool *accepted)
{
    return sony_encode_set_scalar(writer, SONY_DPC_EXPOSURE_PROGRAM, 6, value, accepted);
}
