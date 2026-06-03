
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "uwbproto.h"

int TimeWaitApplicationEvent(uint32_t inDelay);
void TimeSignalApplicationEvent(void);
int UWBsettingsInit(session_state_callback_t sessionStateCallback, const bool inHaveDisplay);

#ifdef __cplusplus
}
#endif
