#include "nearbyinteraction.h"
#include "uwbproto.h"
#include "uwbdefs.h"
#include "uwbcanned.h"
#include "ucidefs.h"
#include "uciextdefs.h"
#include "assertmacros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ni);

struct ble_ni_connection
{
    enum { NI_FREE, NI_BLE_UWB, NI_BLE_CONSOLE } conn_type;
    const void  *conn_ctx;

    uint8_t     client_type; // android or ios
    uint8_t     has_profile_data;
    uint32_t    session_id;
    uint8_t     device_role;
    uint8_t     device_type;
    uint8_t     profile_id;

    enum
    {
        SS_INACTIVE,    // nothing happening
        SS_STARTING,    // we asked session to start
        SS_INIT,        // uwb told us it inited a session
        SS_IDLE,        // uwb told us session in created and ready
        SS_ACTIVE,      // uwb told us session is running
        SS_OVER         // for any reason, session is stopping
    }
    session_state;
};

typedef struct
{
    uint16_t    rx_delay;
    uint16_t    pdoa_calib[11];
    uint16_t    pdoa_offset[2];
    uint16_t    aoa_threshold[2];
    int16_t     rssi_offset[2];
}
uwb_cal_data_t;

// allow for max sessions + 1 console ble connection
//
#define NI_MAX_CONNECTIONS  (UWB_MAX_SESSIONS + 1)

static struct
{
    struct      k_sem event;
    uint64_t    start_timer;
    struct ble_ni_connection connections[NI_MAX_CONNECTIONS];
    uint8_t     msgbuf[NI_MAX_MESSAGE];
    uint32_t    msgcnt;
    int         setting_test;
    uint8_t     rate_req;
    uint8_t     flop_rate;
    uint8_t     invert;
    uint8_t     dump_proto;
    uint8_t     ant_mode;
    bool        ble_console;
    char        ble_name[20];
    uint8_t     antenna_mode;
    int         dist_filter_window;
    int         azim_filter_window;
    int         elev_filter_window;
    int         rssi_filter_window;
    bool        send_usb_csv;
    bool        pnpmode;
    bool        haveDisplay;

    uint8_t     our_mac_addr[2];
    uint16_t    our_uwb_ver[2];
    uint8_t     our_clock_drift[2];
    uint8_t     our_model_id[4];

    uint8_t     shared_data_blob[UWB_MAX_CONFIG_DATA];
    uint8_t     shared_data_blob_length;

    uwb_cal_data_t cal_data;
}
mNI;

#define SHORT_MAC_ADDRESS_MODE (0x00)
#define EXTENDED_MAC_ADDRESS_MODE_WITH_HEADER (0x02)
#define MAC_SHORT_ADD_LEN   (2)
#define MAX_SPEC_VER_LEN    (2)

#define UPDATE_RATE_AUTO    (0)
#define UPDATE_RATE_MIN     (10)
#define UPDATE_RATE_MAX     (20)

#define ACD_CONFIG_LEN      (21)

#define NI_CLIENT_TYPE_IOS      (0)
#define NI_CLIENT_TYPE_ANDROID  (1)

#include <math.h>

const char *FloatPrint(float inVal, char *inBuf, int inBufSize)
{
    // used since adding floating point printf support adds 10k in flash
    //
    int sign = inVal < 0;
    int ival = floor(fabs(inVal));
    int frac = (int)((float)100.0 * (float)fabs(inVal) - (float)100.0 * (float)ival);

    snprintf(inBuf, inBufSize, "%s%d.%02d", sign ? "-" : "", ival, frac);
    return inBuf;
}


/* Example profile

// Documented header

0B              // our profile
01              // our device type
4C 84           // our mac
01              // device role

// Undocumented shared configuration data blob

01 00 01 00     // verson 0001 0001
19              // config data len
55 53           // country code
28 8C 00 00     // session ID
0B              // preamble id
09              // channel
06 00           // slots per round
10 0E           // slot duratiom
B4 00           // range duration
03              // range rount ctrl
CD 26 C4 07 77 D6   // sts_init_iv
1D 72           // dst mac asddr
64 00           // ???? probably clock drift?
*/

static int _NIcreateProfile(struct ble_ni_connection *connection)
{
    uint8_t *cursor = mNI.msgbuf;
    uint8_t *lenpos;
    int room = sizeof(mNI.msgbuf);
    int needed = UCI_MSG_HDR_SIZE + 5 + mNI.shared_data_blob_length;
    int ret = -EINVAL;

    require(room >= needed, exit);

    // create PROP_SET_PROFILE header
    //
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_MTS_CMD | UCI_GID_PROPRIETARY_SE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, EXT_UCI_MSG_SET_PROFILE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    // save cursor to back annotate length
    lenpos = cursor;
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    require_noerr(ret, exit);

    // create the UCI PROP_SET_PROFILE command payload
    //
    ret  = _UWB_PUT_UINT8(&cursor, &room, 0x0b /*connection->profile_id*/);
    ret |= _UWB_PUT_UINT8(&cursor, &room, connection->device_type);
    ret |= _UWB_PUT_DATA(&cursor, &room, mNI.our_mac_addr, sizeof(mNI.our_mac_addr));
    ret |= _UWB_PUT_UINT8(&cursor, &room, connection->device_role);
    ret |= _UWB_PUT_DATA(&cursor, &room, mNI.shared_data_blob, mNI.shared_data_blob_length);
    require_noerr(ret, exit);

    mNI.msgcnt = cursor - mNI.msgbuf;
    require(mNI.msgcnt == needed, exit);

    // back annotate length
    room = 4;
    cursor = lenpos;
    ret = _UWB_PUT_UINT8(&cursor, &room, mNI.msgcnt - 4);

    connection->has_profile_data = true;
    LOG_HEXDUMP_DBG(mNI.msgbuf, mNI.msgcnt, "Profile Info cmd");
exit:
    return ret;

}

/* example ACD

// header (documented part)
0x01,           // tag - ACD data
0x01, 0x00,     // vMaj 1
0x00, 0x00,     // vMin 0
0x14,           // update rate (20 == user interactive)
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // RFU
0x15,           // config data length, 21 bytes
// uwb config data (undocumented part)
0x01, 0x00,     // vMaj 1
0x01, 0x00,     // vMin 1
0x32, 0x11, 0x10, 0x00, // manuf Id
0x46, 0x01, 0x31, 0x00, // model Id
0x00, 0x00,     // middleware vMaj  0
0x06, 0x04,     // midlleware vMin  0406
0x01,           // ranging role (initiator)
0x3f, 0x95,     // mac address
0x64, 0x00,     // clock drift (not present in v1.0, just v1.1)
*/

static int _NIcreateACD(struct ble_ni_connection *connection)
{
    // Nearby-Interaction-Accessory-Protocol-Specification-Release-R2-1.pdf
    uwb_device_info_t *di;
    uint8_t *cursor = mNI.msgbuf;
    int room = sizeof(mNI.msgbuf);
    uint8_t manuf_id[] = NXP_MANUFACTURER_ID;
    int pad;
    int ret;
    int rate;

    ret = UWBgetDeviceInfo(&di);
    require_noerr(ret, exit);

    switch (mNI.rate_req)
    {
    case 0:
        rate = UPDATE_RATE_AUTO;
        break;

    case 1:
        rate = UPDATE_RATE_MIN;
        break;

    case 2:
    default:
        rate = UPDATE_RATE_MAX;
        break;
    }

    ret = _UWB_PUT_UINT8(&cursor, &room, UWBMSG_CONFIG_DATA);    // header cmd for dispatch
    require_noerr(ret, exit);

    if (connection->client_type == NI_CLIENT_TYPE_IOS)
    {
        ret  = _UWB_PUT_UINT16(&cursor, &room, UWB_IOS_SPEC_VERSION_MAJOR);      // MajorVersion
        ret |= _UWB_PUT_UINT16(&cursor, &room, 0/*UWB_IOS_SPEC_VERSION_MINOR*/);      // MinorVersion
        require_noerr(ret, exit);

        ret = _UWB_PUT_UINT8(&cursor, &room, rate);    // PreferredUpdateRate
        require_noerr(ret, exit);

        for (pad = 0; pad < 10; pad++)
        {
            ret |= _UWB_PUT_UINT8(&cursor, &room, 0);            // Reserved
        }

        require_noerr(ret, exit);

        ret  = _UWB_PUT_UINT8(&cursor, &room, ACD_CONFIG_LEN);    // UWBconfigDataLength
        ret |= _UWB_PUT_UINT16(&cursor, &room, mNI.our_uwb_ver[0]);     // MajorVersion
        ret |= _UWB_PUT_UINT16(&cursor, &room, mNI.our_uwb_ver[1]);     // MinorVersion
        ret |= _UWB_PUT_DATA(&cursor, &room, manuf_id, sizeof(manuf_id));
        ret |= _UWB_PUT_DATA(&cursor, &room, mNI.our_model_id, sizeof(mNI.our_model_id));
        require_noerr(ret, exit);

        // mw vMaj
        ret  = _UWB_PUT_UINT16(&cursor, &room, 0);
        // mw vMin
        ret |= _UWB_PUT_UINT8(&cursor, &room, (uint16_t) di->mwMinor);
        ret |= _UWB_PUT_UINT8(&cursor, &room, (uint16_t) di->mwMajor);
        require_noerr(ret, exit);

        ret  = _UWB_PUT_UINT8(&cursor, &room, connection->device_role);     // ranging role
        ret |= _UWB_PUT_DATA(&cursor, &room, mNI.our_mac_addr, sizeof(mNI.our_mac_addr));
        ret |= _UWB_PUT_DATA(&cursor, &room, mNI.our_clock_drift, sizeof(mNI.our_clock_drift));
        require_noerr(ret, exit);
    }
    else if (connection->client_type == NI_CLIENT_TYPE_ANDROID)
    {
        // spec version @0
        ret  = _UWB_PUT_UINT16(&cursor, &room, UWB_ANDROID_SPEC_VERSION_MAJOR);      // MajorVersion
        ret |= _UWB_PUT_UINT16(&cursor, &room, 0/*UWB_ANDROID_SPEC_VERSION_MINOR*/);      // MinorVersion
        require_noerr(ret, exit);

        // chip type (NOT chip id) @4
        ret  = _UWB_PUT_UINT8(&cursor, &room, 0x00);
        ret |= _UWB_PUT_UINT8(&cursor, &room, 0x01 /* chip type SR150 */);
        require_noerr(ret, exit);

        // chip fw version @6
        ret  = _UWB_PUT_UINT8(&cursor, &room, di->fwMajor);     // MajorVersion
        ret |= _UWB_PUT_UINT8(&cursor, &room, di->fwMinor);     // MinorVersion
        require_noerr(ret, exit);

        // mw vMin @8
        ret  = _UWB_PUT_UINT8(&cursor, &room, (uint16_t) di->mwMajor);
        ret |= _UWB_PUT_UINT8(&cursor, &room, (uint16_t) di->mwMinor);
        // mw vMaj @10
        ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
        require_noerr(ret, exit);

        // supported profile ids @11
        ret = _UWB_PUT_UINT8(&cursor, &room, 0);
        ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
        ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
        ret |= _UWB_PUT_UINT8(&cursor, &room, 0xE);   // support profile ids 1,2,3
        require_noerr(ret, exit);

        // supported ranging roles @15
        ret = _UWB_PUT_UINT8(&cursor, &room, 0x3);     // support controller and controllee
        require_noerr(ret, exit);

        // mac maddr @ 16
        ret = _UWB_PUT_DATA(&cursor, &room, mNI.our_mac_addr, sizeof(mNI.our_mac_addr));
        require_noerr(ret, exit);
    }
    else
    {
        LOG_ERR("No such client type %u", connection->client_type);
    }

    mNI.msgcnt = cursor - mNI.msgbuf;

    LOG_HEXDUMP_DBG(mNI.msgbuf, mNI.msgcnt, "ACD");
exit:
    return ret;
}

static int _NIcreateSimpleMessage(uint8_t msgcode, uint8_t *msgbuf, const uint32_t bufsize, int *outCount)
{
    uint8_t *cursor = msgbuf;
    int room = bufsize;
    int ret = -1;

    ret = _UWB_PUT_UINT8(&cursor, &room, msgcode);
    require_noerr(ret, exit);
    require(outCount != NULL, exit);
    *outCount = cursor - msgbuf;
    require(*outCount <= bufsize, exit);
    ret = 0;
exit:
    return ret;
}

static int _NIparseIOSSCD(
    struct ble_ni_connection *connection,
    const uint8_t *inData,
    const int inCount)
{
    int ret = -EINVAL;
    uint8_t *cursor = (uint8_t*) inData;
    int remain;
    uint8_t payloadLength;
    uint16_t vers_maj;
    uint16_t vers_min;

    LOG_HEXDUMP_DBG(inData, inCount, "Shared CD (iOS)");

    // fixed for iOS
    connection->profile_id = 0xB;

    remain = inCount;
    require(remain >= 28, exit);

    /* version maj/min */
    vers_maj = _UWB_GET_UINT16(&cursor);
    vers_min = _UWB_GET_UINT16(&cursor);

    require(vers_maj == 0x0001, exit);
    require((vers_min == 0x0001 || vers_min == 0x0000), exit);

    payloadLength = _UWB_GET_UINT8(&cursor);
    require(payloadLength <= remain, exit);

    if (vers_min == 1)
    {
        require(inCount == UWB_PROFILE_iOS_BLOB_SIZE_v1_1, exit);
    }
    else if (vers_min == 0)
    {
        require(inCount == UWB_PROFILE_iOS_BLOB_SIZE_v1_0, exit);
    }

    memcpy(mNI.shared_data_blob, inData, inCount);
    mNI.shared_data_blob_length = inCount;

    ret = _NIcreateProfile(connection);
    verify_noerr(ret);

exit:
    return ret;
}

static int _NIparseAndroidSCD(
    struct ble_ni_connection *connection,
    const uint8_t *inData,
    const int inCount)
{
    int ret = -EINVAL;
    uint8_t *cursor = (uint8_t*) inData;
    int remain;
    int room;
    uint16_t vers_maj;
    uint16_t vers_min;
    uint32_t session_id;
    uint8_t preamble_id;
    uint8_t channel;
    uint8_t profile_id;
    uint8_t ranging_role;
    uint8_t dest_mac[2];

    LOG_HEXDUMP_DBG(inData, inCount, "Shared CD (Android)");

    remain = inCount;

    require(remain >= UWB_PROFILE_Android_BLOB_SIZE_v1_0, exit);

    /* version maj/min */
    vers_maj = _UWB_GET_UINT16(&cursor);
    vers_min = _UWB_GET_UINT16(&cursor);

    require(vers_maj == 0x0001, exit);
    require((vers_min == 0x0001 || vers_min == 0x0000), exit);

    if (vers_min == 1)
    {
        require((inCount == UWB_PROFILE_Android_BLOB_SIZE_v1_1), exit);
    }
    else if (vers_min == 0)
    {
        require((inCount == UWB_PROFILE_Android_BLOB_SIZE_v1_0), exit);
    }

#if 1 // session id is in BE format in the SCD
    connection->has_profile_data = false;

    session_id = _UWB_GET_UINT8(&cursor);
    session_id <<= 8;
    session_id |= _UWB_GET_UINT8(&cursor);
    session_id <<= 8;
    session_id |= _UWB_GET_UINT8(&cursor);
    session_id <<= 8;
    session_id |= _UWB_GET_UINT8(&cursor);
#else
    session_id   = _UWB_GET_UINT32(&cursor);
#endif
    preamble_id  = _UWB_GET_UINT8(&cursor);
    channel      = _UWB_GET_UINT8(&cursor);
    profile_id   = _UWB_GET_UINT8(&cursor);
    ranging_role = _UWB_GET_UINT8(&cursor);
    _UWB_GET_DATA(&cursor, dest_mac, 2);

    LOG_DBG("Android CD session %08X profile %08X role %08X", session_id, profile_id, ranging_role);

    if (ranging_role & 0x01)
    {
        connection->device_type = UWB_DeviceType_Controller;
        connection->device_role = UWB_DeviceRole_Initiator;
        mNI.our_mac_addr[0] = 0x11;
        mNI.our_mac_addr[1] = 0x11;
    }
    else if (ranging_role & 0x02)
    {
        connection->device_type = UWB_DeviceType_Controlee;
        connection->device_role = UWB_DeviceRole_Responder;
        mNI.our_mac_addr[0] = 0x22;
        mNI.our_mac_addr[1] = 0x22;
    }
    else
    {
        LOG_ERR("Invalid ranging role %02X", ranging_role);
    }

    connection->profile_id = profile_id;
    connection->session_id = session_id;

    cursor = mNI.shared_data_blob;
    room = sizeof(mNI.shared_data_blob);

    /* build an app config to use
     */
    // 4 byte session handle goes here
    ret  = _UWB_PUT_UINT32(&cursor, &room, 0x00);

    // number of parameters
    ret |= _UWB_PUT_UINT8(&cursor, &room, 16);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RANGING_DURATION);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_RANGING_DURATION);
    ret |= _UWB_PUT_UINT32(&cursor, &room, 240);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_SLOT_DURATION);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_SLOT_DURATION);
    ret |= _UWB_PUT_UINT16(&cursor, &room, 2400);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_SLOTS_PER_RR);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_SLOTS_PER_RR);
#if 1 // use 6 for round-robin, Android does that
    ret |= _UWB_PUT_UINT8(&cursor, &room, 6);
#else
    ret |= _UWB_PUT_UINT8(&cursor, &room, 10);
#endif
    require_noerr(ret, exit);

    uint8_t sts_iv[] = { 1, 2, 3, 4, 5, 6 };
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_STATIC_STS_IV);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_STATIC_STS_IV);
    ret |= _UWB_PUT_DATA(&cursor, &room, sts_iv, UCI_PARAM_LEN_STATIC_STS_IV);
    require_noerr(ret, exit);

    uint8_t vendor_id[] = { 8, 7 };
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_VENDOR_ID);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_VENDOR_ID);
    ret |= _UWB_PUT_DATA(&cursor, &room, vendor_id, UCI_PARAM_LEN_VENDOR_ID);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_PREAMBLE_CODE_INDEX);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_PREAMBLE_CODE_INDEX);
    ret |= _UWB_PUT_UINT8(&cursor, &room, preamble_id);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_CHANNEL_NUMBER);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_CHANNEL_NUMBER);
    ret |= _UWB_PUT_UINT8(&cursor, &room, channel);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_NO_OF_CONTROLEES);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_NO_OF_CONTROLEES);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_DST_MAC_ADDRESS);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_DEST_MAC_ADDRESS);
#if 0
    ret |= _UWB_PUT_UINT8(&cursor, &room, dest_mac[1]);
    ret |= _UWB_PUT_UINT8(&cursor, &room, dest_mac[0]);
#else
    ret |= _UWB_PUT_DATA(&cursor, &room, dest_mac, UCI_PARAM_LEN_DEST_MAC_ADDRESS);
#endif
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_DEVICE_ROLE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_DEVICE_ROLE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, connection->device_role);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_MULTI_NODE_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_MULTI_NODE_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_MAC_ADDRESS_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1 /*UCI_PARAM_LEN_MAC_ADDRESS_MODE*/);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_SCHEDULED_MODE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1 /*UCI_PARAM_LEN_SCHEDULED_MODE*/);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_DEVICE_MAC_ADDRESS);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_DEVICE_MAC_ADDRESS);
    ret |= _UWB_PUT_DATA(&cursor, &room, mNI.our_mac_addr, UCI_PARAM_LEN_DEVICE_MAC_ADDRESS);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RANGING_ROUND_USAGE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_RANGING_METHOD);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 2);
    require_noerr(ret, exit);

    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_DEVICE_TYPE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_LEN_DEVICE_TYPE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, connection->device_type);
    require_noerr(ret, exit);

#if 0
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_PARAM_ID_RANGING_ROUND_CONTROL);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1/*UCI_PARAM_LEN_RANGING_ROUND_CONTROL*/);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 3);
    require_noerr(ret, exit);
#endif

    mNI.shared_data_blob_length = cursor - mNI.shared_data_blob;

    // now build UCI from app config

    cursor = mNI.msgbuf;
    room = sizeof(mNI.msgbuf);

    // UCI header
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_MTS_CMD | UCI_GID_SESSION_MANAGE);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_MSG_SESSION_SET_APP_CONFIG);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0x00);
    ret |= _UWB_PUT_UINT8(&cursor, &room, mNI.shared_data_blob_length);
    require_noerr(ret, exit);

    // data
    ret  = _UWB_PUT_DATA(&cursor, &room, mNI.shared_data_blob, mNI.shared_data_blob_length);
    require_noerr(ret, exit);

    mNI.msgcnt = cursor - mNI.msgbuf;

    LOG_HEXDUMP_DBG(mNI.msgbuf, mNI.msgcnt, "Android App config");
exit:
    return ret;
}

static struct ble_ni_connection *_NIallocConnection(const void *conn_ctx)
{
    int i;

    for (i = 0; i < NI_MAX_CONNECTIONS; i++)
    {
        if (mNI.connections[ i ].conn_type == NI_FREE)
        {
            // NOTE - assume all connections are consoles until the
            // first char comes in which should be 0xa/b/c for UWB
            //
            mNI.connections[ i ].conn_type = NI_BLE_CONSOLE;
            mNI.connections[ i ].conn_ctx = conn_ctx;
            mNI.connections[ i ].session_state = SS_INACTIVE;

            /// TODO - generate random mac address?
#if 1 // be the controller (mobile app is controllee for nearby interaction)
            mNI.connections[ i ].device_type = UWB_DeviceType_Controller;
            mNI.connections[ i ].device_role = UWB_DeviceRole_Initiator;
            mNI.our_mac_addr[0] = 0x11;
            mNI.our_mac_addr[1] = 0x11;
#else
            mNI.connections[ i ].device_type = UWB_DeviceType_Controlee;
            mNI.connections[ i ].device_role = UWB_DeviceRole_Responder;
            mNI.our_mac_addr[0] = 0x22;
            mNI.our_mac_addr[1] = 0x22;
#endif
            break;
        }
    }

    return (i < NI_MAX_CONNECTIONS) ? &mNI.connections[ i ] : NULL;
}

static struct ble_ni_connection *_NIfindConnection(const void *conn_ctx)
{
    int i;

    for (i = 0; i < NI_MAX_CONNECTIONS; i++)
    {
        if (mNI.connections[ i ].conn_ctx == conn_ctx)
        {
            break;
        }
    }

    return (i < NI_MAX_CONNECTIONS) ? &mNI.connections[ i ] : NULL;
}

static void _NIfreeConnection(const void *conn_ctx)
{
    struct ble_ni_connection *connection;

    connection = _NIfindConnection(conn_ctx);
    require(connection, exit);

    connection->conn_ctx = NULL;
    connection->conn_type = NI_FREE;
exit:
    return;
}

static const void *_NIgetConsoleConnection(void)
{
    int i;

    for (i = 0; i < NI_MAX_CONNECTIONS; i++)
    {
        if (mNI.connections[ i ].conn_ctx)
        {
            if (mNI.connections[ i ].conn_type == NI_BLE_CONSOLE)
            {
                break;
            }
        }
    }

    return (i < NI_MAX_CONNECTIONS) ? mNI.connections[ i ].conn_ctx : NULL;
}

static int _NItxMessage(const void *conn_ctx, const uint8_t *inData, const uint32_t inCount)
{
    int ret = 0;

    if (conn_ctx)
    {
#if 0
        /// TOOD - send data on BLE connection

        // TODO - what if conn_ctx is an int handle and 0 is valid?
        // maybe use a .is_conn_ctx_set member?
        //
        ret = BLEinternalNotifyUWB((void *) conn_ctx, (void*) inData, inCount);

        if (ret == inCount)
        {
            ret = 0;
        }

#endif
    }
    else
    {
        LOG_INF("No BLE connection to notify");
        ret = 0;
    }

    return ret;
}

static int _SessionStateCallback(uwb_session_t *session, uint8_t state, uint8_t reason)
{
    int ret = 0;
    struct ble_ni_connection *connection;
    uint32_t session_handle;

    require(session, exit);
    connection = _NIfindConnection(session->ble_conn_ctx);

    // note, the session's connection can be taken away
    // if the BLE client closes the connection first
    //
    if (connection)
    {
        connection->session_state = state;
    }

    session_handle = session->session_handle;

    switch (state)
    {
    case UWB_SESSION_INITIALIZED:
        LOG_DBG("Session %08X Initialized", session_handle);

        if (connection)
        {
            connection->session_state = SS_INIT;
        }

        break;

    case UWB_SESSION_DEINITIALIZED:
        LOG_DBG("Session %08X de-Initialized", session_handle);

        if (connection)
        {
            connection->session_state = SS_OVER;
        }

        break;

    case UWB_SESSION_ACTIVE:
        LOG_DBG("ctx %08X   session %08X started",
                (uint32_t)(uintptr_t)connection->conn_ctx, session_handle);

        if (connection)
        {
            connection->session_state = SS_ACTIVE;
        }

        break;

    case UWB_SESSION_IDLE:
        LOG_DBG("Session %08X Idle", session_handle);

        if (connection)
        {
            connection->session_state = SS_IDLE;
        }

        break;

    case UWB_SESSION_ERROR:
        LOG_INF("Session %08X Error %02X", session_handle, reason);

        if (connection)
        {
            connection->session_state = SS_OVER;
        }

        break;

    default:
        break;
    }

    if (connection->session_state == SS_OVER)
    {
        LOG_INF("Session %0X complete", session_handle);

        if (connection)
        {
            if (connection->session_state != SS_INACTIVE && connection->session_state != SS_OVER)
            {
                // Inform mobile session is over
                //
                if (connection && connection->conn_ctx)
                {
                    uint8_t msgbuf[8];
                    int msgcnt;

                    ret = _NIcreateSimpleMessage(UWBMSG_DID_STOP, msgbuf, sizeof(msgbuf), &msgcnt);
                    ret = _NItxMessage(connection->conn_ctx, msgbuf, msgcnt);
                }
            }

            // make sure underlying session is stopped for sure
            //
            UWBstop(connection->conn_ctx);
        }

        connection->session_state = SS_INACTIVE;
    }

exit:
    return ret;
}

int NIrxMessage(void *conn_ctx, const uint8_t *inData, const uint32_t inCount)
{
    int ret = -EINVAL;
    struct ble_ni_connection *connection;

    require(inData, exit);
    require(inCount > 0, exit);

    connection = _NIfindConnection(conn_ctx);
    require(connection, exit);

    LOG_DBG("Got Message %02X %u bytes for NI %08X", inData[0], inCount,
            (uint32_t)(uintptr_t) connection);

    if (connection->conn_type == NI_BLE_CONSOLE)
    {
        if (inData[0] >= UWBMSG_INITIALIZE_IOS && inData[0] <= UWBMSG_STOP)
        {
            LOG_DBG("Connection switching to UWB mode");
            connection->conn_type = NI_BLE_UWB;
        }
    }

    switch (inData[0])
    {
    case UWBMSG_INITIALIZE_IOS:

        // iOS client wants to start a session.  We respond
        // with an AccessoryConfigurationData payload
        //
        if (connection->session_state == SS_INACTIVE || connection->session_state == SS_STARTING)
        {
            connection->client_type = NI_CLIENT_TYPE_IOS;
            ret = _NIcreateACD(connection);

            if (!ret)
            {
                ret = _NItxMessage(conn_ctx, mNI.msgbuf, mNI.msgcnt);
                connection->session_state = SS_STARTING;
            }
            else
            {
                LOG_ERR("Can't make IOS ACS");
            }

            mNI.start_timer = k_uptime_get();
        }
        else
        {
            LOG_WRN("Ignoring Init iOS because already active");
        }

        break;

    case UWBMSG_INITIALIZE_ANDROID:
        if (connection->session_state == SS_INACTIVE)
        {
            connection->client_type = NI_CLIENT_TYPE_ANDROID;
            ret = _NIcreateACD(connection);

            if (!ret)
            {
                ret = _NItxMessage(conn_ctx, mNI.msgbuf, mNI.msgcnt);
                connection->session_state = SS_STARTING;
                mNI.start_timer = k_uptime_get();
            }
        }
        else
        {
            LOG_WRN("Ignoring Init Android because already active");
        }

        break;

    case UWBMSG_CONFIG_AND_START:

        // Mobile client is giving us a ShareableConfigurationData blob
        // which we will pass to the UWB layer and start a session
        //
        if (connection->client_type == NI_CLIENT_TYPE_IOS)
        {
            ret = _NIparseIOSSCD(connection, inData + 1, inCount - 1);
        }
        else
        {
            ret = _NIparseAndroidSCD(connection, inData + 1, inCount - 1);
        }

        // Tell mobile client we've started session if connected
        //
        if (connection->conn_ctx)
        {
            uint8_t msgbuf[8];
            int msgcnt;

            ret = _NIcreateSimpleMessage(UWBMSG_DID_START, msgbuf, sizeof(msgbuf), &msgcnt);
            require_noerr(ret, exit);
            ret = _NItxMessage(connection->conn_ctx, msgbuf, msgcnt);
        }
        else
        {
            LOG_WRN("No connection to inform active");
            ret = 0;
        }

        if (ret)
        {
            LOG_ERR("Cant parse shared data");
            break;
        }

        if (!ret)
        {
            ret = UWBstart(connection->device_type,
                           connection->session_id,
                           connection->has_profile_data,
                           conn_ctx, mNI.msgbuf,
                           mNI.msgcnt);

            if (ret)
            {
                LOG_WRN("Can't start session");

                if (conn_ctx)
                {
                    uint8_t msgbuf[8];
                    int msgcnt;

                    ret = _NIcreateSimpleMessage(UWBMSG_DID_STOP, msgbuf, sizeof(msgbuf), &msgcnt);
                    require_noerr(ret, exit);
                    ret = _NItxMessage(conn_ctx, msgbuf, msgcnt);
                }
            }
        }

        break;

    case UWBMSG_STOP:
        ret = UWBstop(conn_ctx);
        break;

    case UWBMSG_HACK_CONSOLE:
        LOG_DBG("Setting BLE connection as console");
        connection->conn_type = NI_BLE_CONSOLE;
        ret = 0;
        break;

    default:
        ret = 0;
        LOG_WRN("Ignoring cmd 0x%02X", inData[0]);
        break;
    }

exit:
    return ret;
}

void NIsendConsoleData(const char *inData, const int inLength)
{
    const void *conn_ctx;

    if (mNI.ble_console)
    {
        conn_ctx = _NIgetConsoleConnection();

        if (conn_ctx)
        {
            _NItxMessage(conn_ctx, (uint8_t*) inData, inLength);
        }
    }
}

int NIbleConnectHandler(const void * const inConnectionHandle, const uint16_t inMTU, const bool isConnected)
{
    int ret = -1;
    struct ble_ni_connection *connection;

    if (isConnected)
    {
        connection = _NIallocConnection(inConnectionHandle);
        require(connection, exit);
#if 0

        if (connection->session_state != SS_INACTIVE)
        {
            LOG_INF("BLE connect stops ranging session");
            UWBstop();
        }

#endif
        LOG_DBG("New NI %08X for BLE Conn %08X\n",
                (uint32_t)(uintptr_t) connection, (uint32_t)(uintptr_t) inConnectionHandle);
        connection->session_state = SS_INACTIVE;
        ret = 0;
    }
    else
    {
        connection = _NIfindConnection(inConnectionHandle);

        if (connection)
        {
            _NIfreeConnection(inConnectionHandle);
        }

#if 1

        if (connection && connection->session_state != SS_INACTIVE)
        {
            LOG_INF("BLE disconnect stops ranging session");
            UWBstop(inConnectionHandle);
        }

#endif
        ret = 0;
    }

    // note, returning non-0 here will close the ble connection
exit:
    return ret;
}

int NIrestartUWB(void)
{
    int ret;

    LOG_INF("Initializing UWB: antenna_mode:%02X  floprate:%u  filters:%d,%d,%d,%d",
            mNI.antenna_mode,
            mNI.flop_rate,
            mNI.dist_filter_window,
            mNI.azim_filter_window,
            mNI.elev_filter_window,
            mNI.rssi_filter_window);

    LOG_INF("  RX delay: 0x%04X   PDoA off:%02X,%02X  AoA thres:%02X,%02X  RSSI ant1 fudge:%d:%d ",
            mNI.cal_data.rx_delay,
            mNI.cal_data.pdoa_offset[0],
            mNI.cal_data.pdoa_offset[1],
            mNI.cal_data.aoa_threshold[0],
            mNI.cal_data.aoa_threshold[1],
            mNI.cal_data.rssi_offset[0],
            mNI.cal_data.rssi_offset[1]);
#if 1 /* leave in for debug, show the calib table */
    int elev;
    int azim;

    for (elev = 0; elev < 11; elev++)
    {
        for (azim = 0; azim < 11; azim++)
        {
#if 1
            uint8_t lsb = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9[9 + elev * 11 * 2 + azim * 2];
            int8_t  msb = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9[9 + elev * 11 * 2 + azim * 2 + 1];
            int16_t pdoa = ((int16_t)msb << 8) | lsb;
            double pdx = (double)pdoa / 16384.0;
            char fbuf[32];

            LOG_RAW("%02X%02X %s  ", msb, lsb, FloatPrint(pdx, fbuf, sizeof(fbuf)));
#else
            LOG_RAW("%02X,%02X ", UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9[9 + elev * 11 * 2 + azim * 2],
                    UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9[9 + elev * 11 * 2 + azim * 2 + 1]);
#endif
        }

        LOG_RAW("\n");
    }

#endif
    ret = UWBinit(_SessionStateCallback,
                  mNI.antenna_mode,
                  mNI.flop_rate,
                  mNI.dump_proto,
                  mNI.send_usb_csv,
                  mNI.pnpmode,
                  mNI.haveDisplay,
                  mNI.dist_filter_window,
                  mNI.azim_filter_window,
                  mNI.elev_filter_window,
                  mNI.rssi_filter_window,
                  mNI.cal_data.rssi_offset);

    verify_noerr(ret);
    return ret;
}

void NIapplyCalibration(void)
{
    // Set RX delay for 4 antennas
    int rx_delay = mNI.cal_data.rx_delay; /* 14.2 format of 1/4cm */
    uint8_t lsb = rx_delay & 0xFF;
    uint8_t msb = (rx_delay >> 8) & 0xFF;
    int offset = UWB_SET_CALIBRATION_VAR_OFFSET;

    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = 1;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = lsb;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = msb;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = 2;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = lsb;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = msb;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = 3;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = lsb;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = msb;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = 4;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = lsb;
    UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[offset++] = msb;

    // Set PDoA offset
    lsb = mNI.cal_data.pdoa_offset[0] & 0xFF;
    msb = (mNI.cal_data.pdoa_offset[0] >> 8) & 0xFF;
    offset = UWB_SET_CALIBRATION_VAR_OFFSET;

    UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9[offset++] = 1;
    UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9[offset++] = lsb;
    UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9[offset++] = msb;

    lsb = mNI.cal_data.pdoa_offset[1] & 0xFF;
    msb = (mNI.cal_data.pdoa_offset[1] >> 8) & 0xFF;

    UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9[offset++] = 2;
    UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9[offset++] = lsb;
    UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9[offset++] = msb;

    // Set AoA threshold
    lsb = mNI.cal_data.aoa_threshold[0] & 0xFF;
    msb = (mNI.cal_data.aoa_threshold[0] >> 8) & 0xFF;
    offset = UWB_SET_CALIBRATION_VAR_OFFSET;

    UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9[offset++] = 1;
    UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9[offset++] = lsb;
    UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9[offset++] = msb;

    lsb = mNI.cal_data.aoa_threshold[1] & 0xFF;
    msb = (mNI.cal_data.aoa_threshold[1] >> 8) & 0xFF;

    UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9[offset++] = 2;
    UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9[offset++] = lsb;
    UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9[offset++] = msb;

    // Set PDoA calib table
    int elev;
    int azim;
    offset = UWB_SET_CALIBRATION_VAR_OFFSET;

    // Note we currently only handle one row of data since we aren't doing
    // elevation, so just repeat row
    //
    UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9[offset++] = 1; // pair id 1

    bool is_2bp = true; // hack - detect our cal is the 2bp cal and apply whole table
    int off_2bp = 0;

    for (elev = 0; elev < 11; elev++)
    {
        for (azim = 0; azim < 11; azim++)
        {
            lsb = mNI.cal_data.pdoa_calib[azim] & 0xFF;
            msb = (mNI.cal_data.pdoa_calib[azim] >> 8) & 0xFF;
            UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9[offset++] = lsb;
            UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9[offset++] = msb;

            if (elev == 5)
            {
                if (UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_2BP[off_2bp++] != lsb)
                {
                    is_2bp = false;
                }

                if (UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_2BP[off_2bp++] != msb)
                {
                    is_2bp = false;
                }
            }
            else
            {
                off_2bp += 2;
            }
        }
    }

    if (is_2bp)
    {
        LOG_INF("Restoring 2BP calib table");
        memcpy(UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9 + UWB_SET_CALIBRATION_VAR_OFFSET + 1,
               UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_2BP, 11 * 11 * 2);
    }
}

void NIcalFor2BP(void)
{
    mNI.cal_data.rx_delay = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_2BP;
    mNI.cal_data.pdoa_offset[0] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_2BP[0];
    mNI.cal_data.pdoa_offset[1] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_2BP[1];
    mNI.cal_data.aoa_threshold[0] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_2BP[0];
    mNI.cal_data.aoa_threshold[1] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_2BP[1];
    // note: endianess is assumed, amd we pick the elevation == 0 (5th row)
    memcpy(mNI.cal_data.pdoa_calib, UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_2BP + (5 * 11 * 2), sizeof(mNI.cal_data.pdoa_calib));
    NIapplyCalibration();

    // Since 2BP has elevation, fill out the whole cal table, not just one row
    memcpy(UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9 + UWB_SET_CALIBRATION_VAR_OFFSET + 1,
           UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_2BP, 11 * 11 * 2);
}

void NIcalFor2JE(void)
{
    mNI.cal_data.rx_delay = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_2JE;
    mNI.cal_data.pdoa_offset[0] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_2JE[0];
    mNI.cal_data.pdoa_offset[1] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_2JE[1];
    mNI.cal_data.aoa_threshold[0] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_2JE[0];
    mNI.cal_data.aoa_threshold[1] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_2JE[1];
    // note: endianess is assumed
    memcpy(mNI.cal_data.pdoa_calib, UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_2JE, sizeof(mNI.cal_data.pdoa_calib));
    NIapplyCalibration();
}

int NIslice(uint32_t *delay)
{
    return UWBslice(delay);
}

int TimeWaitApplicationEvent(uint32_t inDelay)
{
    int result;

    result = k_sem_take(&mNI.event, K_MSEC(inDelay));

    // result != 0 if timed-out, which is OK
    return result;
}

void TimeSignalApplicationEvent(void)
{
    k_sem_give(&mNI.event);
}

#if CONFIG_SETTINGS

#include <zephyr/settings/settings.h>

void NIstoreCalibration(void)
{
    // RX delay
    settings_save_one("ni/cal_rxdelay", &mNI.cal_data.rx_delay, sizeof(mNI.cal_data.rx_delay));

    // Set PDoA offset
    settings_save_one("ni/cal_pdoa_off1", &mNI.cal_data.pdoa_offset[0], sizeof(mNI.cal_data.pdoa_offset[0]));
    settings_save_one("ni/cal_pdoa_off2", &mNI.cal_data.pdoa_offset[1], sizeof(mNI.cal_data.pdoa_offset[1]));

    // Set AoA threshold
    settings_save_one("ni/cal_aoa_thres1", &mNI.cal_data.aoa_threshold[0], sizeof(mNI.cal_data.aoa_threshold[0]));
    settings_save_one("ni/cal_aoa_thres2", &mNI.cal_data.aoa_threshold[1], sizeof(mNI.cal_data.aoa_threshold[1]));

    // Set PDoA calib table
    settings_save_one("ni/cal_pdoa_calib_0", &mNI.cal_data.pdoa_calib[0], sizeof(mNI.cal_data.pdoa_calib[0]));
    settings_save_one("ni/cal_pdoa_calib_1", &mNI.cal_data.pdoa_calib[1], sizeof(mNI.cal_data.pdoa_calib[1]));
    settings_save_one("ni/cal_pdoa_calib_2", &mNI.cal_data.pdoa_calib[2], sizeof(mNI.cal_data.pdoa_calib[2]));
    settings_save_one("ni/cal_pdoa_calib_3", &mNI.cal_data.pdoa_calib[3], sizeof(mNI.cal_data.pdoa_calib[3]));
    settings_save_one("ni/cal_pdoa_calib_4", &mNI.cal_data.pdoa_calib[4], sizeof(mNI.cal_data.pdoa_calib[4]));
    settings_save_one("ni/cal_pdoa_calib_5", &mNI.cal_data.pdoa_calib[5], sizeof(mNI.cal_data.pdoa_calib[5]));
    settings_save_one("ni/cal_pdoa_calib_6", &mNI.cal_data.pdoa_calib[6], sizeof(mNI.cal_data.pdoa_calib[6]));
    settings_save_one("ni/cal_pdoa_calib_7", &mNI.cal_data.pdoa_calib[7], sizeof(mNI.cal_data.pdoa_calib[7]));
    settings_save_one("ni/cal_pdoa_calib_8", &mNI.cal_data.pdoa_calib[8], sizeof(mNI.cal_data.pdoa_calib[8]));
    settings_save_one("ni/cal_pdoa_calib_9", &mNI.cal_data.pdoa_calib[9], sizeof(mNI.cal_data.pdoa_calib[9]));
    settings_save_one("ni/cal_pdoa_calib_10", &mNI.cal_data.pdoa_calib[10], sizeof(mNI.cal_data.pdoa_calib[10]));

    // Set RSSI fudge/offset
    settings_save_one("ni/cal_rssi_off1", &mNI.cal_data.pdoa_offset[0], sizeof(mNI.cal_data.rssi_offset[0]));
    settings_save_one("ni/cal_rssi_off2", &mNI.cal_data.pdoa_offset[1], sizeof(mNI.cal_data.rssi_offset[1]));
}

static int _NiSettingLoad(const char *inKey, size_t inLen, settings_read_cb inReadCallback, void *inCallbackArg, void *inParam)
{
    if (!strcmp(inKey, "test"))
    {
        inReadCallback(inCallbackArg, &mNI.setting_test, inLen);
        LOG_DBG("ST %d", mNI.setting_test);
    }
    else if (!strcmp(inKey, "rate"))
    {
        inReadCallback(inCallbackArg, &mNI.rate_req, inLen);
    }
    else if (!strcmp(inKey, "flop"))
    {
        inReadCallback(inCallbackArg, &mNI.flop_rate, inLen);
    }
    else if (!strcmp(inKey, "invert"))
    {
        inReadCallback(inCallbackArg, &mNI.invert, inLen);
    }
    else if (!strcmp(inKey, "blecon"))
    {
        inReadCallback(inCallbackArg, &mNI.ble_console, inLen);
        printk("set ble con %d -----------------\n", mNI.ble_console);
    }
    else if (!strcmp(inKey, "blename"))
    {
        memset(mNI.ble_name, 0, sizeof(mNI.ble_name));
        inReadCallback(inCallbackArg, &mNI.ble_name, inLen);
    }
    else if (!strcmp(inKey, "proto"))
    {
        inReadCallback(inCallbackArg, &mNI.dump_proto, inLen);
    }
    else if (!strcmp(inKey, "antmode"))
    {
        inReadCallback(inCallbackArg, &mNI.antenna_mode, inLen);
    }
    else if (!strcmp(inKey, "dfiltw"))
    {
        inReadCallback(inCallbackArg, &mNI.dist_filter_window, inLen);
    }
    else if (!strcmp(inKey, "afiltw"))
    {
        inReadCallback(inCallbackArg, &mNI.azim_filter_window, inLen);
    }
    else if (!strcmp(inKey, "efiltw"))
    {
        inReadCallback(inCallbackArg, &mNI.elev_filter_window, inLen);
    }
    else if (!strcmp(inKey, "rfiltw"))
    {
        inReadCallback(inCallbackArg, &mNI.rssi_filter_window, inLen);
    }
    else if (!strcmp(inKey, "usbcsv"))
    {
        inReadCallback(inCallbackArg, &mNI.send_usb_csv, inLen);
    }
    else if (!strcmp(inKey, "pnpmode"))
    {
        inReadCallback(inCallbackArg, &mNI.pnpmode, inLen);
    }
    else if (!strcmp(inKey, "cal_rxdelay"))
    {
        inReadCallback(inCallbackArg, &mNI.cal_data.rx_delay, inLen);
    }
    else if (!strcmp(inKey, "cal_pdoa_off1"))
    {
        inReadCallback(inCallbackArg, &mNI.cal_data.pdoa_offset[0], inLen);
    }
    else if (!strcmp(inKey, "cal_pdoa_off2"))
    {
        inReadCallback(inCallbackArg, &mNI.cal_data.pdoa_offset[1], inLen);
    }
    else if (!strcmp(inKey, "cal_aoa_thres1"))
    {
        inReadCallback(inCallbackArg, &mNI.cal_data.aoa_threshold[0], inLen);
    }
    else if (!strcmp(inKey, "cal_aoa_thres2"))
    {
        inReadCallback(inCallbackArg, &mNI.cal_data.aoa_threshold[1], inLen);
    }
    else if (!strncmp(inKey, "cal_pdoa_calib_", 15))
    {
        int j = strtoul(inKey + 15, NULL, 0);

        if (j >= 0 && j < 11)
        {
            inReadCallback(inCallbackArg, &mNI.cal_data.pdoa_calib[j], inLen);
        }
    }
    else if (!strcmp(inKey, "cal_rssi_off1"))
    {
        inReadCallback(inCallbackArg, &mNI.cal_data.rssi_offset[0], inLen);
    }
    else if (!strcmp(inKey, "cal_rssi_off2"))
    {
        inReadCallback(inCallbackArg, &mNI.cal_data.rssi_offset[1], inLen);
    }
    else
    {
        LOG_ERR("No such setting %s", inKey);
    }

    return 0;
}

#else

void NIstoreCalibration(void)
{
    return;
}

#endif

#ifdef CONFIG_SHELL

#include <zephyr/shell/shell.h>

static struct ble_ni_connection *cli_conn;

static int _CmdStart(const struct shell *shell, size_t argc, char **argv)
{
    bool initiate = false;
    uint32_t session_id = 0x11223344; /* note, this is the id expected by nxp scripts */

    if (argc > 1)
    {
        switch ((*++argv) [0])
        {
        case 'i':
        case 'I':
            initiate = true;
            break;
        }
    }

    if (argc > 2)
    {
        session_id = strtoul(*++argv, NULL, 0);
    }

    if (!cli_conn)
    {
        cli_conn = _NIallocConnection(NULL);
    }

    if (!cli_conn)
    {
        return -1;
    }

    if (initiate)
    {
        cli_conn->device_type = UWB_DeviceType_Controller;
        cli_conn->device_role = UWB_DeviceRole_Initiator;
        mNI.our_mac_addr[0] = 0x11;
        mNI.our_mac_addr[1] = 0x11;
    }
    else
    {
        cli_conn->device_type = UWB_DeviceType_Controlee;
        cli_conn->device_role = UWB_DeviceRole_Responder;
        mNI.our_mac_addr[0] = 0x22;
        mNI.our_mac_addr[1] = 0x22;
    }

    shell_print(shell, "Starting %s ranging session with id 0x%08X",
                initiate ? "initator" : "responder", session_id);

    int ret = UWBstart(cli_conn->device_type, session_id, false, NULL, NULL, 0);

    return ret;
}

static int _CmdStop(const struct shell *shell, size_t argc, char **argv)
{
    int ret = UWBstop(cli_conn ? cli_conn->conn_ctx : NULL);

    return ret;
}

static int _CmdUnpair(const struct shell *shell, size_t argc, char **argv)
{
    int ret;

    // TODO ?
#if 1
    ret = 0;
#else
    ret =  bt_unpair(BT_ID_DEFAULT, NULL);
#endif
    return ret;
}

static int _CmdPing(const struct shell *shell, size_t argc, char **argv)
{
    NIsendConsoleData("hello\r\n", 7);
    return 0;
}

static int _CmdRate(const struct shell *shell, size_t argc, char **argv)
{
    int rate = 2;

    if (argc > 1)
    {
        rate = (int) strtoul(*++argv, NULL, 0);
    }

    if (rate < 0)
    {
        rate = 0;
    }
    else if (rate > 2)
    {
        rate = 2;
    }

    shell_print(shell, "Setting requested rate to %d", rate);

    mNI.rate_req = rate;
    settings_save_one("ni/rate", &mNI.rate_req, sizeof(mNI.rate_req));
    NIrestartUWB();
    return 0;
}

static int _CmdFlop(const struct shell *shell, size_t argc, char **argv)
{
    int flop = 4;

    if (argc > 1)
    {
        flop = (int) strtoul(*++argv, NULL, 0);
    }

    if (flop < 0)
    {
        flop = 0;
    }

    shell_print(shell, "Setting front/back switch rate to once every %d measurements", flop);

    mNI.flop_rate = flop;
    settings_save_one("ni/flop", &mNI.flop_rate, sizeof(mNI.flop_rate));
    NIrestartUWB();
    return 0;
}

static int _CmdInvert(const struct shell *shell, size_t argc, char **argv)
{
    bool invert = false;

    if (argc > 1)
    {
        invert = 0 != (int) strtoul(*++argv, NULL, 0);
    }

    shell_print(shell, "Setting front/back switch gpio invert to %s", invert ? "true" : "false");

    mNI.invert = invert;
    settings_save_one("ni/invert", &mNI.invert, sizeof(mNI.invert));
    NIrestartUWB();
    return 0;
}

static int _CmdConsole(const struct shell *shell, size_t argc, char **argv)
{
    int onoff = 1;

    if (argc > 1)
    {
        onoff = (int) strtoul(*++argv, NULL, 0);
    }

    shell_print(shell, "Setting BLE console %s", onoff ? "On" : "Off");

    mNI.ble_console = onoff != 0;
    printk("set ble con %d -----------------\n", mNI.ble_console);
    settings_save_one("ni/blecon", &mNI.ble_console, sizeof(mNI.ble_console));

    return 0;
}

static int _CmdName(const struct shell *shell, size_t argc, char **argv)
{
    char *name = "nrfUWB";

    if (argc > 1)
    {
        name = *++argv;
    }

    shell_print(shell, "Setting BLE Adv Name %s", name);

    memset(mNI.ble_name, 0, sizeof(mNI.ble_name));
    strncpy(mNI.ble_name, name, sizeof(mNI.ble_name) - 1);
    settings_save_one("ni/blename", &mNI.ble_name, sizeof(mNI.ble_name));

    return 0;
}

static int _CmdProto(const struct shell *shell, size_t argc, char **argv)
{
    int proto = 0;

    if (argc > 1)
    {
        proto = (int) strtoul(*++argv, NULL, 0);
    }

    proto &= 0x3;

    shell_print(shell, "Setting dump-protocol to %u", proto);

    mNI.dump_proto = proto;
    settings_save_one("ni/proto", &mNI.dump_proto, sizeof(mNI.dump_proto));
    NIrestartUWB();
    return 0;
}

static int _CmdAntMode(const struct shell *shell, size_t argc, char **argv)
{
    int mode = ANTMODE_THREE_FRONT;

    if (argc > 1)
    {
        mode = (int) strtoul(*++argv, NULL, 0);
    }

    if (mode == ANTMODE_THREE_FRONT
            ||  mode == ANTMODE_ONE_FRONT_ONE_BACK
            ||  mode == ANTMODE_TWO_FRONT_ONE_BACK
       )
    {
        mNI.antenna_mode = mode;
        shell_print(shell, "Setting antenna mode to 0x%02X", mNI.antenna_mode);
        settings_save_one("ni/antmode", &mNI.antenna_mode, sizeof(mNI.antenna_mode));
    }
    else
    {
        shell_print(shell, "Invalid mode 0x%02X. Use 0x7 (3 front), 0x11 (1 front 1 back), or 0x13 (2 front 1 back)", mode);
    }

    NIrestartUWB();
    return 0;
}

static int _CmdAntSel(const struct shell *shell, size_t argc, char **argv)
{
    uwb_session_t *session;
    uint8_t sel = ANTSEL_FRONT;
    int i;

    if (argc > 1)
    {
        sel = (uint8_t) strtoul(*++argv, NULL, 0);
    }

    if (sel)
    {
        sel = ANTSEL_BACK;
    }

    shell_print(shell, "Setting antenna to %s for all sessions", (sel == ANTSEL_BACK) ? "indoor" : "outdoor");

    for (i = 0; i < NI_MAX_CONNECTIONS; i++)
    {
        if (mNI.connections[ i ].conn_ctx)
        {
            session = UWBsessionFromConnection(mNI.connections[ i ].conn_ctx);

            if (session)
            {
                session->requested_antenna_sel = sel;
            }
        }
    }

    NIrestartUWB();
    return 0;
}

static int _CmdFilter(const struct shell *shell, size_t argc, char **argv)
{
    int dfilter = 0;
    int afilter = 0;
    int efilter = 0;
    int rfilter = 0;

    if (argc > 1)
    {
        dfilter = (int) strtoul(*++argv, NULL, 0);
    }

    if (argc > 2)
    {
        afilter = (int) strtoul(*++argv, NULL, 0);
    }

    if (argc > 3)
    {
        efilter = (int) strtoul(*++argv, NULL, 0);
    }

    if (argc > 4)
    {
        rfilter = (int) strtoul(*++argv, NULL, 0);
    }

    shell_print(shell, "Setting filter window sizes: distance:%d azimuth:%d elevation:%d rssi:%d",
                dfilter, afilter, efilter, rfilter);

    mNI.dist_filter_window = dfilter;
    mNI.azim_filter_window = afilter;
    mNI.elev_filter_window = efilter;
    mNI.rssi_filter_window = rfilter;

    settings_save_one("ni/dfiltw", &mNI.dist_filter_window, sizeof(mNI.dist_filter_window));
    settings_save_one("ni/afiltw", &mNI.azim_filter_window, sizeof(mNI.azim_filter_window));
    settings_save_one("ni/efiltw", &mNI.elev_filter_window, sizeof(mNI.elev_filter_window));
    settings_save_one("ni/rfiltw", &mNI.elev_filter_window, sizeof(mNI.rssi_filter_window));
    NIrestartUWB();
    return 0;
}

static int _CmdUSBCSV(const struct shell *shell, size_t argc, char **argv)
{
    bool usbcsv = false;

    if (argc > 1)
    {
        usbcsv = strtoul(*++argv, NULL, 0) != 0;
    }

    mNI.send_usb_csv = usbcsv;

    if (usbcsv)
    {
        if (mNI.pnpmode)
        {
            shell_print(shell, "Disabling PnP mode due to USB CSV mode");
            mNI.pnpmode = false;
        }
    }

    shell_print(shell, "Setting USB CSV mode to %d", mNI.send_usb_csv);
    settings_save_one("ni/usbcsv", &mNI.send_usb_csv, sizeof(mNI.send_usb_csv));
    NIrestartUWB();
    return 0;
}

static int _CmdPnPMode(const struct shell *shell, size_t argc, char **argv)
{
    bool pnp = false;

    if (argc > 1)
    {
        pnp = strtoul(*++argv, NULL, 0) != 0;
    }

    mNI.pnpmode = pnp;

    if (pnp)
    {
        if (mNI.send_usb_csv)
        {
            shell_print(shell, "Disabling USB CSV mode due to PnP mode");
            mNI.send_usb_csv = false;
        }
    }

    shell_print(shell, "Setting PnP mode to %d", mNI.pnpmode);
    settings_save_one("ni/pnpmode", &mNI.pnpmode, sizeof(mNI.pnpmode));
    NIrestartUWB();
    return 0;
}

#if CONFIG_NI_BLE_CONSOLE
static int _CmdSim(const struct shell *shell, size_t argc, char **argv)
{
    int sesscount = 1;
    int duration = 20;
    uint64_t end;
    int sess;
    char text[128];
    char fbufA[16];
    char fbufB[16];
    char fbufC[16];

    double distance[UWB_MAX_SESSIONS];
    double azimuth[UWB_MAX_SESSIONS];
    double elevation[UWB_MAX_SESSIONS];
    double rssi;

    if (argc > 1)
    {
        sesscount = (int) strtoul(*++argv, NULL, 0);
    }

    if (sesscount < 1)
    {
        sesscount = 1;
    }
    else if (sesscount > UWB_MAX_SESSIONS)
    {
        sesscount = UWB_MAX_SESSIONS;
    }

    if (argc > 2)
    {
        duration = (int) strtoul(*++argv, NULL, 0);
    }

    if (duration < 5)
    {
        duration = 5;
    }

    end = k_uptime_get() + duration * 1000;

    for (sess = 0; sess < sesscount; sess++)
    {
        distance[sess] = 6.0 - (double)sess;
        azimuth[sess] = 30.0 - 30.0 * sess;
        elevation[sess] = 5.0 - (double)sess;
    }

    shell_print(shell, "Starting sim of %d sessions for %d seconds", sesscount, duration);

    do
    {
        for (sess = 0; sess < sesscount; sess++)
        {
            int textlen;

            rssi = distance[sess];

            if (rssi < 0)
            {
                rssi = -rssi;
            }

            rssi = rssi * -100 / 6.0;

            if (rssi < -100)
            {
                rssi = -100;
            }
            else if (rssi > -40)
            {
                rssi = -40;
            }

            textlen = snprintf(text, sizeof(text), "%d,%d,%s,%s,%s\r\n",
                               sess + 1,
                               (int)rssi,
                               FloatPrint(distance[sess], fbufA, sizeof(fbufA)),
                               FloatPrint(azimuth[sess], fbufB, sizeof(fbufB)),
                               FloatPrint(elevation[sess], fbufC, sizeof(fbufC)));
            distance[sess] -= 0.045;

            NIsendConsoleData(text, textlen);

            k_sleep(K_MSEC(40));
        }
    }
    while (k_uptime_get() < end);

    return 0;
}
#endif

static int _CmdDistCalib(const struct shell *shell, size_t argc, char **argv)
{
    double calib = 941.0;
    char fbuf[32];

    if (argc > 1)
    {
        calib = strtof(*++argv, NULL);
    }

    mNI.cal_data.rx_delay = (int)(calib * 16.0); // store as cm * 4 in 14.2 format
    shell_print(shell, "Setting Rx-Delay to %s cm", FloatPrint(calib, fbuf, sizeof(fbuf)));
    NIstoreCalibration();
    NIrestartUWB();
    return 0;
}

static int _CmdPDoAOffsetCalib(const struct shell *shell, size_t argc, char **argv)
{
    double offset;
    char fbuf[32];

    if (argc > 1)
    {
        offset = strtof(*++argv, NULL);
    }
    else
    {
        offset = -448.0;
    }

    mNI.cal_data.pdoa_offset[0] = (int)(offset);
    shell_print(shell, "Setting PDoA offset antenna pair 1 to %s", FloatPrint(offset, fbuf, sizeof(fbuf)));

    if (argc > 2)
    {
        offset = strtof(*++argv, NULL);
    }
    else
    {
        offset = -665.0;
    }

    mNI.cal_data.pdoa_offset[1] = (int)(offset);
    shell_print(shell, "Setting PDoA offset antenna pair 2 to %s", FloatPrint(offset, fbuf, sizeof(fbuf)));

    NIstoreCalibration();
    NIrestartUWB();
    return 0;
}

static int _CmdRSSIOffsetCalib(const struct shell *shell, size_t argc, char **argv)
{
    int offset;

    if (argc > 1)
    {
        offset = strtol(*++argv, NULL, 0);
    }
    else
    {
        offset = 0;
    }

    mNI.cal_data.rssi_offset[0] = (int16_t)(offset);
    shell_print(shell, "Setting RSSI offset antenna outside to %d", offset);

    if (argc > 2)
    {
        offset = strtol(*++argv, NULL, 0);
    }
    else
    {
        offset = 0;
    }

    mNI.cal_data.rssi_offset[1] = (int16_t)(offset);
    shell_print(shell, "Setting RSSI offset antenna inside to %d", offset);

    NIstoreCalibration();
    NIrestartUWB();
    return 0;
}

static int _CmdThresholdCalib(const struct shell *shell, size_t argc, char **argv)
{
    double threshold;
    char fbuf[32];

    if (argc > 1)
    {
        threshold = strtof(*++argv, NULL);
    }
    else
    {
        threshold = 22654.0;
    }

    mNI.cal_data.aoa_threshold[0] = (int)(threshold);
    shell_print(shell, "Setting AoA threshold antenna pair 1 to %s", FloatPrint(threshold, fbuf, sizeof(fbuf)));

    if (argc > 2)
    {
        threshold = strtof(*++argv, NULL);
    }
    else
    {
        threshold = -42932.0;
    }

    mNI.cal_data.aoa_threshold[1] = (int)(threshold);
    shell_print(shell, "Setting AoA threshold antenna pair 2 to %s", FloatPrint(threshold, fbuf, sizeof(fbuf)));

    NIstoreCalibration();
    NIrestartUWB();
    return 0;
}

static int _CmdPDoACalib(const struct shell *shell, size_t argc, char **argv)
{
    NIstoreCalibration();
    NIrestartUWB();
    return 0;
}

static int _CmdCal2BP(const struct shell *shell, size_t argc, char **argv)
{
    NIcalFor2BP();
    NIstoreCalibration();
    NIrestartUWB();
    // fill table again since restart fills all elevation rows the same
    NIcalFor2BP();
    return 0;
}

static int _CmdCal2JE(const struct shell *shell, size_t argc, char **argv)
{
    NIcalFor2JE();
    NIstoreCalibration();
    NIrestartUWB();
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ni_cal_cmds,
                               SHELL_CMD_ARG(distance, NULL,
                                       " Set Rx antenna delay in cm (941.0)\n",
                                       _CmdDistCalib, 1, 1),
                               SHELL_CMD_ARG(pdoa_offsets, NULL,
                                       " Set PDoA Offsets [ antenna-pair-1 [ antenna-pair-2 ]] (-448.0 -665)\n",
                                       _CmdPDoAOffsetCalib, 1,  2),
                               SHELL_CMD_ARG(threshold, NULL,
                                       " Set AoA Thresholds [ antenna-pair-1 [ antenna-pair-2 ]] (22654.0 -42932.0)\n",
                                       _CmdThresholdCalib, 1,  2),
                               SHELL_CMD_ARG(rssi_offsets, NULL,
                                       " Set RSSI fudge/offsets [ antenna0 (outside) [ antenna1 (inside) ]] (0 0)\n",
                                       _CmdRSSIOffsetCalib, 1,  2),
                               SHELL_CMD_ARG(pdoa, NULL,
                                       " Set PDoA Calib Table [ 11 entries, -60 to +60 azimuth ] (set from 2BP 0 elevation)\n",
                                       _CmdPDoACalib, 1,  11),
                               SHELL_CMD(2BP, NULL,
                                       " Set Calibration paramters for 2BP module\n",
                                       _CmdCal2BP),
                               SHELL_CMD(2JE, NULL,
                                       " Set Calibration paramters for 2JE module\n",
                                       _CmdCal2JE),
                               SHELL_SUBCMD_SET_END
                              );

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ni,
                               SHELL_CMD_ARG(start, NULL,
                                       " Start ranging session [i (initiateor) | r (responder)] [session id] (r 0x11223344)\n",
                                       _CmdStart, 0, 2),
                               SHELL_CMD(stop, NULL,
                                       " Stop ranging session\n",
                                       _CmdStop),
                               SHELL_CMD(unpair, NULL,
                                       " Unpair BLE device\n",
                                       _CmdUnpair),
                               SHELL_CMD(ping, NULL,
                                       " Send text on BLE report characteristic\n",
                                       _CmdPing),
                               SHELL_CMD_ARG(rate, NULL,
                                       " Set session rate [0=auto,1=min,2=max] (2)\n",
                                       _CmdRate, 1, 1),
                               SHELL_CMD_ARG(flop, NULL,
                                       " Set front/back ANT switch rate [0=none (front only), N=meaurements per antenna] (0)\n",
                                       _CmdFlop, 1, 1),
                               SHELL_CMD_ARG(invert, NULL,
                                       " Set front/back ANT switch GPIO invert [1=yes, 0=no] (0)\n",
                                       _CmdInvert, 1, 1),
                               SHELL_CMD_ARG(console, NULL,
                                       " Set console-over-ble on/off [0=off,1=on] (1)\n",
                                       _CmdConsole, 1, 1),
                               SHELL_CMD_ARG(name, NULL,
                                       " Set BLE adv name for device [name] (nrfUWB)\n",
                                       _CmdName, 1, 1),
#if CONFIG_NI_BLE_CONSOLE
                               SHELL_CMD_ARG(sim, NULL,
                                       " Generate simulated range data [number of sessions (1)] [duration secs (20)]\n",
                                       _CmdSim, 0, 2),
#endif
                               SHELL_CMD_ARG(proto, NULL,
                                       " Show UIC protocol. Bitmask [0=no,1=show UIC,2=show states] (0)\n",
                                       _CmdProto, 1, 1),
                               SHELL_CMD_ARG(antmode, NULL,
                                       " Set Antenna configuration [0x7=3front,0x11=1front1back,0x13=2front1back] (0x7)\n",
                                       _CmdAntMode, 1, 1),
                               SHELL_CMD_ARG(antsel, NULL,
                                       " Set Antenna selection for all sessions [0=front,1=back]\n",
                                       _CmdAntSel, 1, 1),
                               SHELL_CMD_ARG(filter, NULL,
                                       " Set range filter window sizes [distance azimuth elevation rssi] 0=no filter (0 0 0 0)\n",
                                       _CmdFilter, 1, 4),
                               SHELL_CMD_ARG(usbcsv, NULL,
                                       " Set send CSV data to USB serial [0=no, 1=yes] (0)\n",
                                       _CmdUSBCSV, 1, 1),
                               SHELL_CMD_ARG(pnpmode, NULL,
                                       " Use USB serial for direct UCI channel [0=no, 1=yes] (0)\n",
                                       _CmdPnPMode, 1, 1),
                               SHELL_CMD(calib, &ni_cal_cmds,
                                       " Calibration Commands\n",
                                       NULL),
                               SHELL_SUBCMD_SET_END
                              );

SHELL_CMD_REGISTER(ni, &sub_ni, "Nearby Interaction", NULL);

#endif

const char *NIgetBLEname(void)
{
    return mNI.ble_name;
}

int NIinit(const bool inHaveDisplay)
{
    int ret = 0;

    memset(&mNI, 0, sizeof(mNI));

    ret = k_sem_init(&mNI.event, 1, 1);
    require_noerr(ret, exit);

    strncpy(mNI.ble_name, "nrfUWB", sizeof(mNI.ble_name) - 1);

    // Default antenna mode sets default calibration

    if (true)
    {
        mNI.antenna_mode = ANTMODE_TWO_FRONT_ONE_BACK; // select 2JE style module
        mNI.flop_rate = 1; // switch antennas each measurement
    }
    else
    {
        mNI.antenna_mode = ANTMODE_THREE_FRONT;  // select 2BP style module
        mNI.flop_rate = 0; // no flopping
    }

    mNI.rate_req = 2; // max
    mNI.ble_console = true; // on

    mNI.dist_filter_window = 0;
    mNI.azim_filter_window = 0;
    mNI.elev_filter_window = 0;
    mNI.rssi_filter_window = 0;

    mNI.send_usb_csv = false;
    mNI.pnpmode = false;
    mNI.haveDisplay = inHaveDisplay;

    mNI.our_uwb_ver[0] = 1;
    mNI.our_uwb_ver[1] = 1;

    mNI.our_model_id[0] = 0x46;
    mNI.our_model_id[1] = 0x01;
    mNI.our_model_id[2] = 0x31;
    mNI.our_model_id[3] = 0x00;

    mNI.our_clock_drift[0] = 0x64;
    mNI.our_clock_drift[1] = 0x00;

#if CONFIG_SETTINGS

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load_subtree_direct("ni", _NiSettingLoad, NULL);

        if (mNI.setting_test == 0)
        {
            mNI.setting_test = 12345;
            settings_save_one("ni/test", &mNI.setting_test, sizeof(mNI.setting_test));
            settings_save_one("ni/rate", &mNI.rate_req, sizeof(mNI.rate_req));
            settings_save_one("ni/flop", &mNI.flop_rate, sizeof(mNI.flop_rate));
            settings_save_one("ni/invert", &mNI.invert, sizeof(mNI.invert));
            settings_save_one("ni/blecon", &mNI.ble_console, sizeof(mNI.ble_console));
            settings_save_one("ni/proto", &mNI.dump_proto, sizeof(mNI.dump_proto));
            settings_save_one("ni/antmode", &mNI.antenna_mode, sizeof(mNI.antenna_mode));
            settings_save_one("ni/dfiltw", &mNI.dist_filter_window, sizeof(mNI.dist_filter_window));
            settings_save_one("ni/afiltw", &mNI.azim_filter_window, sizeof(mNI.azim_filter_window));
            settings_save_one("ni/efiltw", &mNI.elev_filter_window, sizeof(mNI.elev_filter_window));
            settings_save_one("ni/rfiltw", &mNI.rssi_filter_window, sizeof(mNI.rssi_filter_window));
            settings_save_one("ni/usbcsv", &mNI.send_usb_csv, sizeof(mNI.send_usb_csv));
            settings_save_one("ni/pnpmode", &mNI.pnpmode, sizeof(mNI.pnpmode));

            if (mNI.antenna_mode == ANTMODE_TWO_FRONT_ONE_BACK)
            {
                NIcalFor2JE();
            }
            else
            {
                NIcalFor2BP();
            }

            NIstoreCalibration();
        }
    }
    else
#endif
    {
        if (mNI.antenna_mode == ANTMODE_TWO_FRONT_ONE_BACK)
        {
            NIcalFor2JE();
        }
        else
        {
            NIcalFor2BP();
        }
    }

    NIapplyCalibration();

    ret = NIrestartUWB();
    require_noerr(ret, exit);

exit:
    return ret;
}

