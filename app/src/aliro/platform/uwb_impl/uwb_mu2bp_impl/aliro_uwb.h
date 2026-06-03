
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include "uwbproto.h"

    /* WrappedRDS parameter lengths */
#define RANDOM_KEY_LEN      12
#define CCC_SESSION_KEY_LEN 32
#define CCC_WRAPPED_RDS_LEN (SESSION_ID_LEN + RANDOM_KEY_LEN + CCC_SESSION_KEY_LEN)

#define ALIRO_CHAP_PER_SLOT_3_MASK  ((uint8_t)0x1u << 0u)
#define ALIRO_CHAP_PER_SLOT_4_MASK  ((uint8_t)0x1u << 1u)
#define ALIRO_CHAP_PER_SLOT_6_MASK  ((uint8_t)0x1u << 2u)
#define ALIRO_CHAP_PER_SLOT_8_MASK  ((uint8_t)0x1u << 3u)
#define ALIRO_CHAP_PER_SLOT_9_MASK  ((uint8_t)0x1u << 4u)
#define ALIRO_CHAP_PER_SLOT_12_MASK ((uint8_t)0x1u << 5u)
#define ALIRO_CHAP_PER_SLOT_24_MASK ((uint8_t)0x1u << 6u)
#define ALIRO_CHAP_PER_SLOT_ALLOWED_BITS_MASK                                                          \
    ((uint8_t)(ALIRO_CHAP_PER_SLOT_3_MASK | ALIRO_CHAP_PER_SLOT_4_MASK | ALIRO_CHAP_PER_SLOT_6_MASK |  \
               ALIRO_CHAP_PER_SLOT_8_MASK | ALIRO_CHAP_PER_SLOT_9_MASK | ALIRO_CHAP_PER_SLOT_12_MASK | \
               ALIRO_CHAP_PER_SLOT_24_MASK))

#define ALIRO_HOPPING_MODE_NO_HOPPING_MASK ((uint8_t)0x1u << 2u)
#define ALIRO_HOPPING_MODE_CONTINUOUS_MASK ((uint8_t)0x1u << 1u)
#define ALIRO_HOPPING_MODE_ADAPTIVE_MASK   ((uint8_t)0x1u << 0u)
#define ALIRO_HOPPING_MODE_BIT_POS         5u
#define ALIRO_HOPPING_MODE_BITFIELD_MASK   ((uint8_t)0x07u << ALIRO_HOPPING_MODE_BIT_POS)

#define ALIRO_HOPPING_SEQ_AES_MASK      ((uint8_t)0x1u << 3u)
#define ALIRO_HOPPING_SEQ_DEFAULT_MASK  ((uint8_t)0x1u << 4u)
#define ALIRO_HOPPING_SEQ_RESERVED_MASK ((uint8_t)0x07u)
#define ALIRO_HOPPING_SEQ_BITFIELD_MASK ((uint8_t)0x1Fu)

#define ALIRO_HOPPING_DEFAULT_CONFIG_BITMASK ((ALIRO_HOPPING_MODE_CONTINUOUS_MASK << ALIRO_HOPPING_MODE_BIT_POS) | ALIRO_HOPPING_SEQ_DEFAULT_MASK)

#define ALIRO_HOPPING_CONFIG_NO_HOPPING (0x80)
#define ALIRO_HOPPING_CONFIG_CONTINUOUS_HOPPING_MODULO (0x50)
#define ALIRO_HOPPING_CONFIG_CONTINUOUS_HOPPING_AES (0x48)
#define ALIRO_HOPPING_CONFIG_ADAPTIVE_HOPPING_MODULO (0x30)
#define ALIRO_HOPPING_CONFIG_ADAPTIVE_HOPPING_AES (0x28)

#define ALIRO_MAC_MODE_RANGING_ROUND_NBR_BIT_POS          6u
#define ALIRO_MAC_MODE_RANGING_ROUND_NBR_BITFIELD_MASK    ((uint8_t)0x3u << ALIRO_MAC_MODE_RANGING_ROUND_NBR_BIT_POS)
#define ALIRO_MAC_MODE_RANGING_ROUND_OFFSET_BITFIELD_MASK ((uint8_t)0x3Fu)

#define ALIRO_STS_INDEX0_MAX ((uint32_t)0x3FFFFFFFu)

#define ALIRO_SYNC_CODE_INDEX_MIN ((uint8_t)1u)
#define ALIRO_SYNC_CODE_INDEX_MAX ((uint8_t)32u)

#define ALIRO_READER_UWB_MAX_PULSESHAPE_COMBO_ENTRIES (9U)
#define ALIRO_READER_UWB_MAX_CONFIG_ID_ENTRIES        (8U)

#ifndef ALIRO_READER_UWB_CONFIG_NUMBER_RESPONDER_NODES
#define ALIRO_READER_UWB_CONFIG_NUMBER_RESPONDER_NODES (1)
#endif /** ALIRO_READER_UWB_CONFIG_NUMBER_RESPONDER_NODES */

#define ALIRO_UWB_SLOT_BITMASK_3  (1 << 0)
#define ALIRO_UWB_SLOT_BITMASK_4  (1 << 1)
#define ALIRO_UWB_SLOT_BITMASK_6  (1 << 2)
#define ALIRO_UWB_SLOT_BITMASK_8  (1 << 3)
#define ALIRO_UWB_SLOT_BITMASK_9  (1 << 4)
#define ALIRO_UWB_SLOT_BITMASK_12 (1 << 5)
#define ALIRO_UWB_SLOT_BITMASK_24 (1 << 6)

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

void AliroUWBConfigPrint(const char *blurb, const uwb_session_params_t *params);

int AliroUWBbuildM1(uint32_t sessionIdentifier, uwb_config_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade);
int AliroUWBparseM2(uwb_config_params_t *config, uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength);
int AliroUWBbuildM3(uwb_session_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade);
int AliroUWBparseM4(uwb_config_params_t *config, uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength);
int AliroUWBbuildState(uint8_t source, uint8_t value, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade);

int AliroUWBbuildAppConfiguration(uwb_session_params_t *params, uint8_t *inBuffer, size_t inBufferSize, size_t *outBufferCount);

#ifdef __cplusplus
}
#endif


