#include "aliro_uwb.h"

#include "uwbproto.h"
#include "uwbdefs.h"
#include "aliro_proto.h"
#include "assertmacros.h"

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#include <stdarg.h>

LOG_MODULE_REGISTER(aliroUWB);

static inline int _ALIRO_PUT_UINT8(uint8_t **pcursor, int *room, const uint8_t data)
{
    uint8_t *cursor = *pcursor;
    int ret = -EINVAL;

    if (!(pcursor && *pcursor && room))
    {
        goto exit;
    }

    if (*room < 1)
    {
        goto exit;
    }

    *cursor++ = data;
    *pcursor = cursor;
    *room = *room - 1;
    ret = 0;
exit:
    return ret;
}

static inline int _ALIRO_PUT_UINT16(uint8_t **pcursor, int *room, const uint16_t data)
{
    uint8_t *cursor = *pcursor;
    int ret = -EINVAL;

    if (!(pcursor && *pcursor && room))
    {
        goto exit;
    }

    if ((uint32_t)*room < sizeof(uint16_t))
    {
        goto exit;
    }

    *cursor++ = data >> 8;
    *cursor++ = data & 0xFF;
    *pcursor = cursor;
    *room = *room - sizeof(uint16_t);
    ret = 0;
exit:
    return ret;
}

static inline int _ALIRO_PUT_UINT32(uint8_t **pcursor, int *room, const uint32_t data)
{
    uint8_t *cursor = *pcursor;
    int ret = -EINVAL;

    if (!(pcursor && *pcursor && room))
    {
        goto exit;
    }

    if ((uint32_t)*room < sizeof(uint16_t))
    {
        goto exit;
    }

    *cursor++ = data >> 24;
    *cursor++ = data >> 16;
    *cursor++ = data >> 8;
    *cursor++ = data & 0xFF;
    *pcursor = cursor;
    *room = *room - sizeof(uint32_t);
    ret = 0;
exit:
    return ret;
}

static int _ALIRO_PUT_ATTR_UINT8(uint8_t **cursor, int *room, uint8_t attribute, uint8_t value)
{
    _ALIRO_PUT_UINT8(cursor, room, attribute);
    _ALIRO_PUT_UINT8(cursor, room, sizeof(uint8_t));
    _ALIRO_PUT_UINT8(cursor, room, value);

    return (*room > 0) ? 0 : -1;
}

static int _ALIRO_PUT_ATTR_UINT16(uint8_t **cursor, int *room, uint8_t attribute, uint16_t value)
{
    _ALIRO_PUT_UINT8(cursor, room, attribute);
    _ALIRO_PUT_UINT8(cursor, room, sizeof(uint16_t));
    _ALIRO_PUT_UINT16(cursor, room, value);

    return (*room > 0) ? 0 : -1;
}

static int _ALIRO_PUT_ATTR_UINT32(uint8_t **cursor, int *room, uint8_t attribute, uint32_t value)
{
    _ALIRO_PUT_UINT8(cursor, room, attribute);
    _ALIRO_PUT_UINT8(cursor, room, sizeof(uint32_t));
    _ALIRO_PUT_UINT32(cursor, room, value);

    return (*room > 0) ? 0 : -1;
}

static inline uint8_t _ALIRO_GET_UINT8(uint8_t **pcursor, uint8_t *pval, size_t *avail)
{
    uint8_t *cursor = *pcursor;
    uint8_t val;
    int ret = -EINVAL;

    require(pcursor && pval && avail, exit);
    ret = -EOVERFLOW;
    require(*avail > 0, exit);
    val = *cursor++;
    *pcursor = cursor;
    *pval = val;
    *avail = *avail - 1;
    ret = 0;
exit:
    return ret;
}

static inline int _ALIRO_GET_UINT16(uint8_t **pcursor, uint16_t *pval, size_t *avail)
{
    uint8_t *cursor = *pcursor;
    uint16_t val;
    int ret = -EINVAL;

    require(pcursor && pval && avail, exit);
    ret = -EOVERFLOW;
    require(*avail > 1, exit);
    val =  ((uint16_t)*cursor++) << 8;
    val |= ((uint16_t)*cursor++) & 0xff;
    *pcursor = cursor;
    *pval = val;
    *avail = *avail - 2;
    ret = 0;
exit:
    return ret;
}

static inline int _ALIRO_GET_UINT32(uint8_t **pcursor, uint32_t *pval, size_t *avail)
{
    uint8_t *cursor = *pcursor;
    uint32_t val;
    int ret = -EINVAL;

    require(pcursor && pval && avail, exit);
    ret = -EOVERFLOW;
    require(*avail > 3, exit);
    val =  ((uint32_t)*cursor++) << 24;
    val |= ((uint32_t)*cursor++) << 16;
    val |= ((uint32_t)*cursor++) << 8;
    val |= ((uint32_t)*cursor++) & 0xff;
    *pcursor = cursor;
    *pval = val;
    *avail = *avail - 4;
    ret = 0;
exit:
    return ret;
}

static int _SetParamsFromAttributes(uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength)
{
    int ret = -EINVAL;
    uint8_t attr;
    uint8_t attrlen;
    size_t avail = inLength;
    uint8_t *cursor = (uint8_t*)inbuf;

    require(params && inbuf && inLength, exit);

    ret = 0;

    while (avail > 1 && ret == 0)
    {
        attr = *cursor++;
        attrlen  = *cursor++;
        avail -= 2;

        LOG_INF("attr %d len %d", attr, attrlen);

        switch (attr)
        {
        case ALIRO_ATTR_UWB_CONFIG_ID:
            ret = _ALIRO_GET_UINT16(&cursor, &params->configIdentifier, &avail);
            break;
        case ALIRO_ATTR_UWB_PULSE_SHAPE_COMBO:
            ret = _ALIRO_GET_UINT8(&cursor, &params->pulseShapeCombo, &avail);
            break;
        case ALIRO_ATTR_UWB_SESSION_ID:
            break;
        case ALIRO_ATTR_UWB_CHANNEL_BITMASK:
            ret = _ALIRO_GET_UINT8(&cursor, &params->channelBitmask, &avail);
            break;
        case ALIRO_ATTR_UWB_RAN_MULTIPLIER:
            ret = _ALIRO_GET_UINT8(&cursor, &params->ranMultiplier, &avail);
            break;
        case ALIRO_ATTR_UWB_SLOT_BITMASK:
            ret = _ALIRO_GET_UINT8(&cursor, &params->slotBitmask, &avail);
            break;
        case ALIRO_ATTR_UWB_SYNC_CODE_INDEX_MASK:
            ret = _ALIRO_GET_UINT32(&cursor, &params->syncCodeIndexBitmask, &avail);
            break;
        case ALIRO_ATTR_UWB_SYNC_CODE_INDEX:
            ret = _ALIRO_GET_UINT8(&cursor, &params->syncCodeIndex, &avail);
            break;
        case ALIRO_ATTR_UWB_HOP_CONFIG_BITMASK:
            ret = _ALIRO_GET_UINT8(&cursor, &params->hoppingBitmask, &avail);
            break;
        case ALIRO_ATTR_UWB_CHAPS_PER_SLOT:
            ret = _ALIRO_GET_UINT8(&cursor, &params->chapsPerSlot, &avail);
            break;
        case ALIRO_ATTR_UWB_NUM_RESPONDER_NODES:
            ret = _ALIRO_GET_UINT8(&cursor, &params->respondersNodes, &avail);
            break;
        case ALIRO_ATTR_UWB_SLOTS_PER_ROUND:
            ret = _ALIRO_GET_UINT8(&cursor, &params->slotsPerRound, &avail);
            break;
        case ALIRO_ATTR_UWB_STS_INDEX_0:
            ret = _ALIRO_GET_UINT32(&cursor, &params->stsIndex0, &avail);
            break;
        case ALIRO_ATTR_UWB_TIME_0:
            ret = _ALIRO_GET_UINT32(&cursor, &params->uwbTime0, &avail);
            break;
        case ALIRO_ATTR_UWB_HOP_KEY_MODE:
            ret = _ALIRO_GET_UINT32(&cursor, &params->hopModeKey, &avail);
            break;
        case ALIRO_ATTR_UWB_MAC_MODE:
            ret = _ALIRO_GET_UINT8(&cursor, &params->macMode, &avail);
            break;
        case ALIRO_ATTR_UWB_VENDOR_SPECIFIC:
            avail -= attrlen;
            cursor += attrlen;
            ret = 0;
            break;
        case ALIRO_ATTR_UWB_STATUS:
            avail -= attrlen;
            cursor += attrlen;
            ret = 0;
            break;
        default:
            LOG_ERR("Bad attr %02x", attr);
            ret = -EINVAL;
            break;
        }
    }
exit:
    return ret;
}


// The responder-device initiates ranging capability exchange by sending Ranging Session Setup M1
// Message ID (see section 11.7.2.2) to the initiator. The Ranging Session Setup M1 Message ID
// includes following Attribute IDs (see Table 11-13):
// 1. UWB Configuration Identifier,
// 2. Pulse Shape Combination,
// 3. Channel Bitmask: list of available UWB RF channels.
// 4. UWB Session Identifier: Identifier of the current ranging session.

int AliroUWBbuildM1(uint32_t sessionIdentifier, uwb_session_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade)
{
    uint8_t *cursor = outbuf;
    uint8_t *lenptr;
    int room = outbufSize;
    int lenroom = 4;
    int ret = -EINVAL;

    require(params && outbuf && outbufSize && bytesMade, exit);

    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PROTO_TYPE_UWB);                                 // 01
    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PT_UWB_SSM1);                                    // 00
    lenptr = cursor;
    _ALIRO_PUT_UINT16(&cursor, &room, 0);                                                   // nn nn
    _ALIRO_PUT_ATTR_UINT16(&cursor, &room, ALIRO_ATTR_UWB_CONFIG_ID, params->configIdentifier);                    // 00 02 00 01
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_PULSE_SHAPE_COMBO, params->pulseShapeCombo);             // 01 01 00
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_CHANNEL_BITMASK, params->channelBitmask);        // 03 01 02
    _ALIRO_PUT_ATTR_UINT32(&cursor, &room, ALIRO_ATTR_UWB_SESSION_ID, sessionIdentifier);   // 02 04 nn nn nn nn
    _ALIRO_PUT_UINT16(&lenptr, &lenroom, outbufSize - room - 4);

    *bytesMade = outbufSize - room;

    ret = (room > 0) ? 0 : -EOVERFLOW;

exit:
    return ret;
}

// 11.7.2.3 Ranging Session Setup M2 Message ID
// The Ranging Session Setup M2 Message ID is sent by the User Device to the Reader in response to
// Ranging Session Setup M1 Message ID during ranging session setup exchange.
// The following Attribute IDs are carried in the Payload field:
// 1. UWB Configuration Identifier
// 2. Pulse Shape Combination
// 3. Channel Bitmask
// 4. SYNC Code Index Bitmask
// 5. RAN Multiplier
// 6. Slot Bitmask
// 7. Hopping Configuration Bitmask
// 8. Vendor Specific attribute is optionally present.

int AliroUWBparseM2(uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength)
{
    int ret = -EINVAL;

    require(params && inbuf && inLength, exit);

    ret = _SetParamsFromAttributes(params, inbuf, inLength);
exit:
    return ret;
}

// 11.7.2.4 Ranging Session Setup M3 Message ID
// This Ranging Session Setup M3 Message ID is sent by the Reader to the User Device in response to
// Ranging Session Setup M2 Message ID during ranging session setup exchange.
// The following Attribute IDs are carried in the Payload field:
// 1. RAN Multiplier
// 2. Number Chaps per Slot
// 3. Number Responders Nodes
// 4. Number Slots per Round
// 5. SYNC Code Index Bitmask
// 6. Hopping Configuration Bitmask
// 7. MAC Mode
// 8. Vendor Specific attribute is optionally present.

int AliroUWBbuildM3(uwb_session_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade)
{
    uint8_t *cursor = outbuf;
    uint8_t *lenptr;
    int room = outbufSize;
    int lenroom = 4;

    int ret = -EINVAL;

    require(params && outbuf && outbufSize && bytesMade, exit);

    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PROTO_TYPE_UWB);                                 // 01
    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PT_UWB_SSM3);                                    // 02
    lenptr = cursor;
    _ALIRO_PUT_UINT16(&cursor, &room, 0);                                                   // nn nn
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_RAN_MULTIPLIER, params->ranMultiplier);
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_CHAPS_PER_SLOT, params->chapsPerSlot);
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_NUM_RESPONDER_NODES, params->respondersNodes);
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_SLOTS_PER_ROUND, params->slotsPerRound);
    _ALIRO_PUT_ATTR_UINT32(&cursor, &room, ALIRO_ATTR_UWB_SYNC_CODE_INDEX_MASK, params->syncCodeIndexBitmask);
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_HOP_CONFIG_BITMASK, params->hoppingBitmask);
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_MAC_MODE, params->macMode);
    _ALIRO_PUT_UINT16(&lenptr, &lenroom, outbufSize - room - 4);

    *bytesMade = outbufSize - room;

    ret = (room > 0) ? 0 : -EOVERFLOW;

exit:
    return ret;
}

// 11.7.2.5 Ranging Session Setup M4 Message ID
// This Ranging Session Setup M4 Message ID is sent by the User Device to the Reader in response to
// a Ranging Session Setup M3 Message ID during ranging session setup exchange.
// The following Attribute IDs are carried in the Payload field:
// 1. STS Index0
// 2. UWB Time0
// 3. HOP Mode Key
// 4. SYNC Code Index
// 5. Vendor Specific attribute is optionally present.
// 11.7

int AliroUWBparseM4(uwb_session_params_t *params, const uint8_t *inbuf, size_t inLength)
{
    int ret = -EINVAL;

    require(params && inbuf && inLength, exit);

    ret = _SetParamsFromAttributes(params, inbuf, inLength);
exit:
    return ret;
}

int AliroUWBbuildState(uint8_t source, uint8_t value, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade)
{
    int ret = -EINVAL;
    uint8_t *cursor = outbuf;
    uint8_t *lenptr;
    int room = outbufSize;
    int lenroom;
    uint16_t state_value = ((uint16_t)source << 8) | value;

    require(outbuf && outbufSize && bytesMade, exit);

    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PROTO_TYPE_NOTIFICATION);
    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PT_NOTIFICATION_RDR_STATUS_CHANGE);
    lenptr = cursor;
    _ALIRO_PUT_UINT16(&cursor, &room, 0);
    _ALIRO_PUT_ATTR_UINT16(&cursor, &room, ALIRO_ATTR_NTF_RDR_STATUS_CHANGE_STATE, state_value);
    lenroom = 4;
    _ALIRO_PUT_UINT16(&lenptr, &lenroom, outbufSize - room - 4);

    *bytesMade = outbufSize - room;

    ret = (room > 0) ? 0 : -EOVERFLOW;

exit:
    return ret;
}

