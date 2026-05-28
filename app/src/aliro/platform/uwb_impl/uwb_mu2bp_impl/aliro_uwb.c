#include "aliro_uwb.h"

#include "uwbproto.h"
#include "uwbdefs.h"
#include "aliro_proto.h"

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#include <stdarg.h>

LOG_MODULE_REGISTER(aliroUWB);

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

// The responder-device initiates ranging capability exchange by sending Ranging Session Setup M1
// Message ID (see section 11.7.2.2) to the initiator. The Ranging Session Setup M1 Message ID
// includes following Attribute IDs (see Table 11-13):
// 1. UWB Configuration Identifier,
// 2. Pulse Shape Combination,
// 3. Channel Bitmask: list of available UWB RF channels.
// 4. UWB Session Identifier: Identifier of the current ranging session.

int AliroUWBbuildM1(uint8_t *outbuf, size_t outbufSize, size_t *bytesMade)
{
    uint8_t *cursor = outbuf;
	//uint8_t *lenptr;
    int room = outbufSize;
	//int lenroom = 4;
	uint8_t chanmask = 0;

	if (UWB_CHANNEL_NUMBER == 5)
	{
		chanmask |= (1 << 0);
	}
	else if (UWB_CHANNEL_NUMBER == 9)
	{
		chanmask |= (1 << 1);
	}
	else
	{
		LOG_ERR("Not supporting non 5/9 channel");
		return -1;
	}

    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PROTO_TYPE_UWB);                          	// 01
    _ALIRO_PUT_UINT8(&cursor, &room, ALIRO_PT_UWB_SSM1);                             	// 00
	//lenptr = cursor;
    //_ALIRO_PUT_UINT16(&cursor, &room, 0);                                            	// nn nn
    _ALIRO_PUT_ATTR_UINT16(&cursor, &room, ALIRO_ATTR_UWB_CONFIG_ID, 1);             	// 00 02 00 01
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_PULSE_SHAPE_COMBO, 0);      	// 01 01 00
    _ALIRO_PUT_ATTR_UINT8(&cursor, &room, ALIRO_ATTR_UWB_CHANNEL_BITMASK, chanmask); 	// 03 01 02
    //_ALIRO_PUT_ATTR_UINT32(&cursor, &room, ALIRO_ATTR_UWB_SESSION_ID, 0x01);    		// 02 04 12 34 56 78
    //_ALIRO_PUT_UINT16(&lenptr, &lenroom, outbufSize - room - 4);

    *bytesMade = outbufSize - room;

    return (room > 0) ? 0 : -1;
}
