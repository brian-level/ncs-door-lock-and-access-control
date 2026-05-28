
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include "uwbproto.h"

int AliroUWBbuildM1(uint32_t sessionIdentifier, uwb_session_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade);
int AliroUWBparseM2(uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength);
int AliroUWBbuildM3(uwb_session_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade);
int AliroUWBparseM4(uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength);

#ifdef __cplusplus
}
#endif


