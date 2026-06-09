#include "aliro_uwb.h"

#include "uwbproto.h"
#include "uwbdefs.h"
#include "ucidefs.h"
#include "uciextdefs.h"
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

    if (*room < sizeof(uint8_t))
    {
        ret = -EOVERFLOW;
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
        ret = -EOVERFLOW;
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

    if ((uint32_t)*room < sizeof(uint32_t))
    {
        ret = -EOVERFLOW;
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

static int _ALIRO_PUT_ATTRS_UINT8(uint8_t **cursor, int *room, uint8_t attribute, uint8_t *values, const int num_values)
{
    _ALIRO_PUT_UINT8(cursor, room, attribute);
    _ALIRO_PUT_UINT8(cursor, room, sizeof(uint8_t) * num_values);

    for (int i = 0; i < num_values; i++)
    {
        _ALIRO_PUT_UINT8(cursor, room, values[i]);
    }

    return (*room > 0) ? 0 : -1;
}

static int _ALIRO_PUT_ATTR_UINT16(uint8_t **cursor, int *room, uint8_t attribute, uint16_t value)
{
    _ALIRO_PUT_UINT8(cursor, room, attribute);
    _ALIRO_PUT_UINT8(cursor, room, sizeof(uint16_t));

    _ALIRO_PUT_UINT16(cursor, room, value);

    return (*room > 0) ? 0 : -1;
}

static int _ALIRO_PUT_ATTRS_UINT16(uint8_t **cursor, int *room, uint8_t attribute, uint16_t *values, const int num_values)
{
    _ALIRO_PUT_UINT8(cursor, room, attribute);
    _ALIRO_PUT_UINT8(cursor, room, sizeof(uint16_t) * num_values);

    for (int i = 0; i < num_values; i++)
    {
        _ALIRO_PUT_UINT16(cursor, room, values[i]);
    }

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
    val = ((uint16_t) * cursor++) << 8;
    val |= ((uint16_t) * cursor++) & 0xff;
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
    val = ((uint32_t) * cursor++) << 24;
    val |= ((uint32_t) * cursor++) << 16;
    val |= ((uint32_t) * cursor++) << 8;
    val |= ((uint32_t) * cursor++) & 0xff;
    *pcursor = cursor;
    *pval = val;
    *avail = *avail - 4;
    ret = 0;
exit:
    return ret;
}

static int _DumpAliroAttributes(const uint8_t *inbuf, const size_t inLength)
{
    int ret = -EINVAL;
    uint8_t attr;
    uint8_t attrlen;
    size_t avail = inLength;
    uint8_t *cursor = (uint8_t*)inbuf;
    uint32_t u32val;
    uint16_t u16val;
    uint8_t u8val;

    require(inbuf && inLength, exit);

    ret = 0;

    while (avail > 1 && ret == 0)
    {
        attr = *cursor++;
        attrlen  = *cursor++;
        avail -= 2;

        switch (attr)
        {
        case ALIRO_ATTR_UWB_CONFIG_ID:
            ret = _ALIRO_GET_UINT16(&cursor, &u16val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_CONFIG_ID: 0x%04x", u16val);
            break;

        case ALIRO_ATTR_UWB_PULSE_SHAPE_COMBO:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_PULSE_SHAPE_COMBO: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_SESSION_ID:
            ret = _ALIRO_GET_UINT32(&cursor, &u32val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_SESSION_ID: 0x%08x", u32val);
            break;

        case ALIRO_ATTR_UWB_CHANNEL_BITMASK:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_CHANNEL_BITMASK: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_RAN_MULTIPLIER:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_RAN_MULTIPLIER: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_SLOT_BITMASK:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_SLOT_BITMASK: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_SYNC_CODE_INDEX_MASK:
            ret = _ALIRO_GET_UINT32(&cursor, &u32val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_SYNC_CODE_INDEX_MASK: 0x%08x", u32val);
            break;

        case ALIRO_ATTR_UWB_SYNC_CODE_INDEX:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_SYNC_CODE_INDEX: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_HOP_CONFIG_BITMASK:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_HOP_CONFIG_BITMASK: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_CHAPS_PER_SLOT:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_CHAPS_PER_SLOT: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_NUM_RESPONDER_NODES:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_NUM_RESPONDER_NODES: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_SLOTS_PER_ROUND:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_SLOTS_PER_ROUND: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_STS_INDEX_0:
            ret = _ALIRO_GET_UINT32(&cursor, &u32val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_STS_INDEX_0: 0x%08x", u32val);
            break;

        case ALIRO_ATTR_UWB_TIME_0:
            ret = _ALIRO_GET_UINT32(&cursor, &u32val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_UWB_TIME_0: 0x%08x", u32val);
            break;

        case ALIRO_ATTR_UWB_HOP_KEY_MODE:
            ret = _ALIRO_GET_UINT32(&cursor, &u32val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_HOP_KEY_MODE: 0x%08x", u32val);
            break;

        case ALIRO_ATTR_UWB_MAC_MODE:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            require_noerr(ret, exit);
            LOG_INF("ALIRO UWB_MAC_MODE: 0x%02x", u8val);
            break;

        case ALIRO_ATTR_UWB_VENDOR_SPECIFIC:
            LOG_HEXDUMP_INF(cursor, attrlen, "UWB_VENDOR_SPECIFIC");
            avail -= attrlen;
            cursor += attrlen;
            ret = 0;
            break;

        case ALIRO_ATTR_UWB_STATUS:
            ret = _ALIRO_GET_UINT8(&cursor, &u8val, &avail);
            LOG_INF("ALIRO UWB_STATUS: 0x%02x", u8val);
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

static int _SetParamsFromAttributes(uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength)
{
    int ret = -EINVAL;
    uint8_t attr;
    uint8_t attrlen;
    size_t avail = inLength;
    uint8_t *cursor = (uint8_t*)inbuf;

    require(params && inbuf && inLength, exit);

    ret = 0;

    if (1)
    {
        _DumpAliroAttributes(cursor, avail);
    }

    while (avail > 1 && ret == 0)
    {
        attr = *cursor++;
        attrlen  = *cursor++;
        avail -= 2;

        switch (attr)
        {
        case ALIRO_ATTR_UWB_CONFIG_ID:
            ret = _ALIRO_GET_UINT16(&cursor, &params->configIdentifier, &avail);
            break;

        case ALIRO_ATTR_UWB_PULSE_SHAPE_COMBO:
            ret = _ALIRO_GET_UINT8(&cursor, &params->pulseShapeCombo, &avail);
            break;

        case ALIRO_ATTR_UWB_SESSION_ID:
            avail -= attrlen;
            cursor += attrlen;
            ret = 0;
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

// [b7 b6 b5] : bitmask of hopping modes the device offers to use in the ranging session
//      100 - No Hopping
//      010 - Continuous Hopping
//      001 - Adaptive Hopping

// [b4 b3 b2 b1 b0] : bit mask of hopping sequences the device offers to use in the ranging session
//      b4=1 is always set because of the default hopping sequence. Support for it is mandatory.
//      b3=1 is set when the optional AES based hopping sequence is supported.
// Rest of the bits are reserved for future

static uint8_t AliroUWBhoppingBitmaskToSR150(uint8_t bits)
{
    switch (bits)
    {
    case ALIRO_HOPPING_CONFIG_NO_HOPPING:
        return UWB_CCC_HopMask_Disable;

    case ALIRO_HOPPING_CONFIG_CONTINUOUS_HOPPING_MODULO:
        return UWB_CCC_HopMask_Cont_def;

    case ALIRO_HOPPING_CONFIG_CONTINUOUS_HOPPING_AES:
        return UWB_CCC_HopMask_Cont_AES;

    case ALIRO_HOPPING_CONFIG_ADAPTIVE_HOPPING_MODULO:
        return UWB_CCC_HopMask_Adapt_def;

    case ALIRO_HOPPING_CONFIG_ADAPTIVE_HOPPING_AES:
        return UWB_CCC_HopMask_Adapt_AES;

    default:
        return 0xFF;
    }
}

static uint8_t AliroUWBhoppingModeToSR150(uint8_t bits)
{
    switch (bits)
    {
    case ALIRO_HOPPING_CONFIG_NO_HOPPING:
        return UWB_CCC_HopMode_Disable;

    case ALIRO_HOPPING_CONFIG_CONTINUOUS_HOPPING_MODULO:
        return UWB_CCC_HopMode_Cont_def;

    case ALIRO_HOPPING_CONFIG_CONTINUOUS_HOPPING_AES:
        return UWB_CCC_HopMode_Cont_AES;

    case ALIRO_HOPPING_CONFIG_ADAPTIVE_HOPPING_MODULO:
        return UWB_CCC_HopMode_Adapt_def;

    case ALIRO_HOPPING_CONFIG_ADAPTIVE_HOPPING_AES:
        return UWB_CCC_HopMode_Adapt_AES;

    default:
        return 0xFF;
    }
}

void AliroUWBConfigPrint(const char *blurb, const uwb_session_params_t *params)
{
    if (params == NULL)
    {
        LOG_ERR("uwb_session_params_t == NULL");
        return;
    }

    LOG_INF("---- %s ---- parameters", blurb ? blurb : "");
    LOG_INF("params_id                          : 0x%04x", params->configIdentifier);
    LOG_INF("pulse_shape_combo                  : 0x%02x", params->pulseShapeCombo);
    //LOG_INF("session_id                         : 0x%08x", params->session_id);
    LOG_INF("channel                            : %d", params->channel);
    LOG_INF("ran_multiplier                     : %d", params->ranMultiplier);
    LOG_INF("sync_code_index                    : %d", params->syncCodeIndex);
    LOG_INF("chaps_per_slot                     : %d", params->chapsPerSlot);
    LOG_INF("number_responders_nodes            : %d", params->respondersNodes);
    LOG_INF("hopping_params_bitmask             : 0x%02x", params->hoppingBitmask);
    LOG_INF("selected_number_slots_per_round    : %d", params->slotsPerRound);
    LOG_INF("csa_mac_mode                       : 0x%02x", params->macMode);
    LOG_INF("sts_index0                         : 0x%08x", params->stsIndex0);
    LOG_INF("hop_mode_key                       : 0x%08x / %u", params->hopModeKey, params->hopModeKey);
    LOG_INF("uwb_time0 (hex)                    : 0x%08x", params->uwbTime0);
    LOG_INF("");
    LOG_INF("Calculated values:");
    LOG_INF("SLOT_DURATION                     : %d", params->chapsPerSlot * 1200 / 3);
    LOG_INF("RANGING_DURATION                  : %d", params->ranMultiplier * 96);
    LOG_INF("HOPPING_MODE                      : %d", AliroUWBhoppingBitmaskToSR150(params->hoppingBitmask));
}

// The responder-device initiates ranging capability exchange by sending Ranging Session Setup M1
// Message ID (see section 11.7.2.2) to the initiator. The Ranging Session Setup M1 Message ID
// includes following Attribute IDs (see Table 11-13):
// 1. UWB Configuration Identifiers,
// 2. Pulse Shape Combination,
// 3. Channel Bitmask: list of available UWB RF channels.
// 4. UWB Session Identifier: Identifier of the current ranging session.

int AliroUWBbuildM1(uint32_t sessionIdentifier, uwb_config_params_t *params, uint8_t *outbuf, const size_t outbufSize, size_t *bytesMade)
{
    uint8_t *cursor = outbuf;
    uint8_t *lenptr;
    int room = outbufSize;
    int lenroom = 4;
    int ret = -EINVAL;

    require(params && outbuf && outbufSize && bytesMade, exit);

    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PROTO_TYPE_UWB);
    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PT_UWB_SSM1);
    lenptr = cursor;
    _ALIRO_PUT_UINT16(&cursor, &room, 0);
    _ALIRO_PUT_ATTRS_UINT16(&cursor, &room, ALIRO_ATTR_UWB_CONFIG_ID, params->config_identifiers, params->num_config_identifiers);
    _ALIRO_PUT_ATTRS_UINT8(&cursor, &room, ALIRO_ATTR_UWB_PULSE_SHAPE_COMBO, params->pulse_shape_combos, params->num_pulse_shape_combos);
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_CHANNEL_BITMASK, params->channelBitmask);
    _ALIRO_PUT_ATTR_UINT32(&cursor, &room, ALIRO_ATTR_UWB_SESSION_ID, sessionIdentifier);
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

int AliroUWBparseM2(uwb_config_params_t *config, uwb_session_params_t *params, const uint8_t *inbuf, const size_t inLength)
{
    int ret = -EINVAL;
    int i;

    require(params && inbuf && inLength, exit);

    ret = _SetParamsFromAttributes(params, inbuf, inLength);
    require_noerr(ret, exit);

    // validate/merge M2 params
    //
    // config identifier
    //
    ret = -EINVAL;

    for (i = 0; i < config->num_config_identifiers; i++)
    {
        if (config->config_identifiers[i] == params->configIdentifier)
        {
            break;
        }
    }

    require(i < config->num_config_identifiers, exit);

    // pulse shape combo
    //
    ret = -EINVAL;

    for (i = 0; i < config->num_pulse_shape_combos; i++)
    {
        if (config->pulse_shape_combos[i] == params->pulseShapeCombo)
        {
            break;
        }
    }

    require(i < config->num_pulse_shape_combos, exit);

    // channel bitmask
    //
    ret = -EINVAL;
    require((params->channelBitmask & config->channelBitmask) == config->channelBitmask, exit);

    // RAN multiplier must be >= our lowest value we put in M1
    //
    ret = -EINVAL;
    require(params->ranMultiplier >= config->ranMultiplier, exit);

    // SLOT bitmask

    // Hopping bitmask.  We select the mode that matches first from upper to lower bits
    //

    // select matching mode (bits 7,6,5)
    for (i = 7; i > 4; i--)
    {
        if ((params->hoppingBitmask & (1 << i)) && (config->hoppingBitmask & (1 << i)))
        {
            break;
        }
    }

    if (i <= 4)
    {
        ret = -EINVAL;
        LOG_ERR("No overlap in hopping mode: %02x, config=%02x", params->hoppingBitmask, config->hoppingBitmask);
    }
    else
    {
        params->hoppingBitmask &= ~0xE0;
        params->hoppingBitmask |= (1 << i);
    }

    // select matching sequence (bits 4,3)
    for (i = 4; i > 2; i--)
    {
        if ((params->hoppingBitmask & (1 << i)) && (config->hoppingBitmask & (1 << i)))
        {
            break;
        }
    }

    if (i <= 2)
    {
        ret = -EINVAL;
        LOG_ERR("No overlap in hopping sequence: %02x, config=%02x", params->hoppingBitmask, config->hoppingBitmask);
    }
    else
    {
        params->hoppingBitmask &= ~0x1F;
        params->hoppingBitmask |= (1 << i);
    }

    ret = 0;
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

int AliroUWBparseM4(uwb_config_params_t *config, uwb_session_params_t *params, const uint8_t *inbuf, size_t inLength)
{
    int ret = -EINVAL;

    require(params && inbuf && inLength, exit);

    ret = _SetParamsFromAttributes(params, inbuf, inLength);
    require_noerr(ret, exit);

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


int AliroUWBbuildAppConfiguration(
    uwb_session_params_t *params,
    uint8_t *inBuffer,
    size_t inBufferSize,
    size_t *outBufferCount)
{
    int ret = -EINVAL;
    uint8_t *cursor = inBuffer;
    uint8_t *lenpos;
    uint8_t *parmcountpos;
    int room = inBufferSize;
    int lenroom;
    int parmcount = 0;
    int datalen;

    require(inBuffer && (inBufferSize > 16) && outBufferCount, exit);

    /* build an app config to use
     */
    // UCI header
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_MTS_CMD | UCI_GID_SESSION_MANAGE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_MSG_SESSION_SET_APP_CONFIG);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0x00);
    lenpos = cursor;
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    require_noerr(ret, exit);

    // 4 byte session handle goes here
    ret  = _UWB_PUT_UINT32(&cursor, &room, 0x00);

    // number of parameters
    parmcountpos = cursor;
    ret |= _UWB_PUT_UINT8(&cursor, &room, parmcount);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_UWB_CONFIG_ID);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    ret |= _UWB_PUT_UINT16(&cursor, &room, params->configIdentifier);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RANGING_PROTOCOL_VER);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    ret |= _UWB_PUT_UINT16(&cursor, &room, 0x0100);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_STS_CONFIG);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0x01);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_PULSESHAPE_COMBO);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, params->pulseShapeCombo);
    parmcount++;
    require_noerr(ret, exit);
#if 1
    uint8_t hopping_mode = AliroUWBhoppingModeToSR150(params->hoppingBitmask);
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_HOPPING_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, hopping_mode);
    parmcount++;
    require_noerr(ret, exit);
#endif
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RANGING_DURATION);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_RANGING_DURATION);
    ret |= _UWB_PUT_UINT32(&cursor, &room, params->ranMultiplier * 96);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_SLOT_DURATION);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_SLOT_DURATION);
    ret |= _UWB_PUT_UINT16(&cursor, &room, params->chapsPerSlot * 1200 / 3);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_SLOTS_PER_RR);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_SLOTS_PER_RR);
    ret |= _UWB_PUT_UINT8(&cursor, &room, params->slotsPerRound);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_STS_INDEX);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_STS_INDEX);
    ret |= _UWB_PUT_UINT32(&cursor, &room, params->stsIndex0);
    parmcount++;
    require_noerr(ret, exit);
#if 0
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_HOP_MODE_KEY);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 4);
    ret |= _UWB_PUT_UINT32(&cursor, &room, params->hopModeKey);
    parmcount++;
    require_noerr(ret, exit);
#endif

#if 0
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_ALIRO_MAC_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, params->macMode);
    parmcount++;
    require_noerr(ret, exit);
#endif

#if 0
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_STATIC_STS_IV);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_STATIC_STS_IV);
    ret |= _UWB_PUT_DATA(&cursor, &room, (uint8_t*)&params->stsIndex0, UCI_PARAM_LEN_STATIC_STS_IV);
    parmcount++;
    require_noerr(ret, exit);
#endif
#if 0
    uint8_t vendor_id[] = { 8, 7 };
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_VENDOR_ID);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_VENDOR_ID);
    ret |= _UWB_PUT_DATA(&cursor, &room, vendor_id, UCI_PARAM_LEN_VENDOR_ID);
    parmcount++;
    require_noerr(ret, exit);
#endif
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_PREAMBLE_CODE_INDEX);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_PREAMBLE_CODE_INDEX);
    ret |= _UWB_PUT_UINT8(&cursor, &room, params->syncCodeIndex);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_CHANNEL_NUMBER);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_CHANNEL_NUMBER);
    ret |= _UWB_PUT_UINT8(&cursor, &room, params->channel);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_NO_OF_CONTROLEES);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_NO_OF_CONTROLEES);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_DEVICE_ROLE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_DEVICE_ROLE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, params->device_role);
    parmcount++;
    require_noerr(ret, exit);
#if 1
    if (params->device_role != UWB_DeviceRole_Responder)
    {
        ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_DST_MAC_ADDRESS);
        ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_DEST_MAC_ADDRESS);
        ret |= _UWB_PUT_DATA(&cursor, &room, params->dst_mac_addr, UCI_PARAM_LEN_DEST_MAC_ADDRESS);
        parmcount++;
        require_noerr(ret, exit);
    }
#endif
#if 1
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_MULTI_NODE_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_MULTI_NODE_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    parmcount++;
    require_noerr(ret, exit);
#endif
#if 0
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_MAC_ADDRESS_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1 /*UCI_PARAM_LEN_MAC_ADDRESS_MODE*/);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_SCHEDULED_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1 /*UCI_PARAM_LEN_SCHEDULED_MODE*/);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    parmcount++;
    require_noerr(ret, exit);
#endif
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_DEVICE_MAC_ADDRESS);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_DEVICE_MAC_ADDRESS);
    ret |= _UWB_PUT_DATA(&cursor, &room, params->our_mac_addr, UCI_PARAM_LEN_DEVICE_MAC_ADDRESS);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_DEVICE_TYPE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_DEVICE_TYPE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, params->device_type);
    parmcount++;
    require_noerr(ret, exit);

#if 1
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RANGING_ROUND_USAGE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_RANGING_METHOD);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    parmcount++;
    require_noerr(ret, exit);
#endif
#if 0
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RANGING_ROUND_CONTROL);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1/*UCI_PARAM_LEN_RANGING_ROUND_CONTROL*/);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 3);
    parmcount++;
    require_noerr(ret, exit);
#endif
#if 1
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_SESSION_INFO_NTF);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    parmcount++;
    require_noerr(ret, exit);
#endif

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_MAC_FCS_TYPE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RFRAME_CONFIG);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 3);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_CCC_CONFIG_QUIRKS);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_FAR_PROXIMITY_CONFIG);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    ret |= _UWB_PUT_UINT16(&cursor, &room, 0x4e20);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_NEAR_PROXIMITY_CONFIG);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    ret |= _UWB_PUT_UINT16(&cursor, &room, 0x0000);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_MAX_NUMBER_OF_MEASUREMENTS);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    ret |= _UWB_PUT_UINT16(&cursor, &room, 0xffff);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_URSK_TTL);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    ret |= _UWB_PUT_UINT16(&cursor, &room, 0x02d0);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_MAX_RR_RETRY);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    ret |= _UWB_PUT_UINT16(&cursor, &room, 0x00);
    parmcount++;
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RESPONDER_SLOT_INDEX);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    parmcount++;
    require_noerr(ret, exit);

    // back annotate length
    datalen = cursor - lenpos - 1;
    lenroom = 4;
    ret  = _UWB_PUT_UINT8(&lenpos, &lenroom, datalen);
    require_noerr(ret, exit);

    // back annotate parameter count
    lenroom = 4;
    ret  = _UWB_PUT_UINT8(&parmcountpos, &lenroom, parmcount);
    require_noerr(ret, exit);

    *outBufferCount = cursor - inBuffer;

exit:
    return ret;
}

int AliroUWBbuildVendorConfiguration(
    uwb_session_params_t *params,
    uint32_t sessionIdentifier,
    uint8_t *sessionKey,
    int sessionKeyLength,
    uint8_t *inBuffer,
    size_t inBufferSize,
    size_t *outBufferCount)
{
    int ret = -EINVAL;
    uint8_t *cursor = inBuffer;
    uint8_t *lenpos;
    uint8_t *parmcountpos;
    int room = inBufferSize;
    int lenroom;
    int parmcount = 0;
    int datalen;

    /** 12 bytes of Random Key with its 1st 4 bytes set as 0xB5 to distinguish URSK from WRAPPED_RDS */
    uint8_t randomKey[RANDOM_KEY_LEN] = {0xB5, 0xB5, 0xB5, 0xB5, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    require(sessionKey, exit);
    require(inBuffer && (inBufferSize > 16) && outBufferCount, exit);

    // UCI header
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_MTS_CMD | UCI_GID_VENDOR);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_MSG_SESSION_VENDOR_SET_APP_CONFIG);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0x00);
    lenpos = cursor;
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    require_noerr(ret, exit);

    // 4 byte session handle goes here
    ret  = _UWB_PUT_UINT32(&cursor, &room, 0x00);

    // number of parameters
    parmcountpos = cursor;
    ret |= _UWB_PUT_UINT8(&cursor, &room, parmcount);
    require_noerr(ret, exit);

#if 1
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_VENDOR_PARAM_ID_CSA_MAC_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    ret |= _UWB_PUT_UINT8(&cursor, &room, params->macMode);
    parmcount++;
    require_noerr(ret, exit);
#endif

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_VENDOR_PARAM_ID_WRAPPED_RDS);
    ret |= _UWB_PUT_UINT8(&cursor, &room, CCC_WRAPPED_RDS_LEN);

    if (sessionKeyLength == CCC_SESSION_KEY_LEN)
    {
        /** Plain URSK passed, form the Wrapped RDS */
        /* SessionID (4 bytes) (note its byte-swapped vs everything else!) */
        ret = _ALIRO_PUT_UINT32(&cursor, &room, sessionIdentifier);
        require_noerr(ret, exit);

        /* Random Key (12 bytes) */
        ret = _UWB_PUT_DATA(&cursor, &room, randomKey, RANDOM_KEY_LEN);
        require_noerr(ret, exit);

        /* URSK */
        ret = _UWB_PUT_DATA(&cursor, &room, sessionKey, sessionKeyLength);
        require_noerr(ret, exit);

        parmcount++;
    }
    else
    {
        LOG_ERR("Bad key size");
    }

    // back annotate length
    datalen = cursor - lenpos - 1;
    lenroom = 4;
    ret  = _UWB_PUT_UINT8(&lenpos, &lenroom, datalen);
    require_noerr(ret, exit);

    // back annotate parameter count
    lenroom = 4;
    ret  = _UWB_PUT_UINT8(&parmcountpos, &lenroom, parmcount);
    require_noerr(ret, exit);

    *outBufferCount = cursor - inBuffer;

exit:
    return ret;
}

