
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include "uwbproto.h"

typedef enum
{
    SS_INACTIVE,    // nothing happening
    SS_STARTING,    // we asked session to start
    SS_INIT,        // uwb told us it inited a session
    SS_IDLE,        // uwb told us session is created and ready
    SS_ACTIVE,      // uwb told us session is running
    SS_OVER         // for any reason, session is stopping
}
e_session_state_t;

int AliroUWBbuildM1(uint32_t sessionIdentifier, uwb_session_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade);
int AliroUWBparseM2(uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength);
int AliroUWBbuildM3(uwb_session_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade);
int AliroUWBparseM4(uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength);
int AliroUWBbuildState(uint8_t source, uint8_t value, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade);

#ifdef __cplusplus
}
#endif


