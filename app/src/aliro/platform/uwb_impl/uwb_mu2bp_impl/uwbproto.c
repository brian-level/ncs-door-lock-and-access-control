#include "uwbproto.h"
#include "uwbdefs.h"
#include "uwbcanned.h"
#include "uwbrange.h"
#include "hbciProto.h"
#include "uciproto.h"
#include "ucidefs.h"
#include "uciextdefs.h"
#include "nrfspi.h"
#include "uwbsettings.h"
#include "assertmacros.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uwb);

// how long we expect to be in any sesssion state
// at most before we give up (milliseconde) (for most states)
//
#define UWB_STATE_TRANSITION_TIMEOUT_MS    (80)

// how many consquetive range errors before we shut down
//
#define UWB_MAX_RANGE_ERRORS    (32)

#define UWB_NEXT_STATE(ns)  \
    if (mUWB.dump_proto & 0x2) { LOG_INF("Init-State %d -> %d", mUWB.init_state, ns); }  \
    mUWB.init_state = ns;                        \
    mUWB.state_timer = k_uptime_get() + _uwb_time_for_state(ns)

#define UWB_MAX_COMMAND_SET (16)

#define SYNC_CODE_BITMASK(val) (0x01U << ((val)-1U))

#define IPC_GPIO_NODE DT_PATH(gpio)
static const struct gpio_dt_spec mAnt_Sel = GPIO_DT_SPEC_GET(DT_NODELABEL(uwb_ant_sel), gpios);

static struct
{
    bool initialized;
    int  sliceErrors;
    bool is_responder;
    int  power_offset;
    uint8_t  channel_id;
    uint8_t  flop_rate;
    uint8_t  invert_antenna;
    uint8_t  dump_proto;
    uint8_t  antenna_mode;
    bool do_AoA_Calibration;
    bool do_Calibration;
    bool do_OTP_Read_Power;
    bool do_OTP_Read_XTAL;
    bool do_get_device_info_and_caps;

    bool one_time_init_request;

    uint64_t state_timer;
    bool     stopOnRangeErrors;

    uint32_t next_session_id;
    uwb_session_t sessions[UWB_MAX_SESSIONS];

    // buffer is also used for UCI raw protocol so needs to be
    uint8_t  configData[UWB_MAX_UCI_MESSAGE /*UWB_MAX_CONFIG_DATA*/];
    uint32_t configDataLength;
    bool     configDataIsProfile;

    uint8_t antenna_selector[24];
    uint8_t antenna_selector_length;

    enum
    {
        UWB_IDLE,
        UWB_SESSION,
    }
    state, next_state;

    uwb_init_state_t init_state;
    uwb_init_state_t next_init_state;

    uint8_t uci_session_state;

    int command_set_count;
    int command_set_state;
    uint32_t command_size[UWB_MAX_COMMAND_SET];
    const uint8_t *commands[UWB_MAX_COMMAND_SET];

    session_state_callback_t session_callback;

    uwb_device_info_t device_info;
}
mUWB;

static uint16_t s_config_identifiers[] = { 0x0000, 0x0001 };
static uint8_t  s_pulse_shape_combos[] = { 0x00, 0x11, 0x22 };


static int __uwb_parse_device_info(const uint8_t *inData, const int inLength)
{
    int ret = -EINVAL;
    uint8_t *cursor;
    uint8_t *next_cursor;
    uint8_t type;
    uint8_t length;
    uint8_t discard;
    uint32_t ext_length;
    int index;
    uwb_device_info_t *di = &mUWB.device_info;

    memset(&mUWB.device_info, 0, sizeof(mUWB.device_info));

    require(inData, exit);
    require(inLength >= 8, exit);

    cursor = (uint8_t*)inData;

    // check first byte which is the reponse status
    discard = _UWB_GET_UINT8(&cursor);
    require(discard == 0, exit);

    di->uciGenericMajor                   = _UWB_GET_UINT8(&cursor);
    di->uciGenericMinorMaintenanceVersion = _UWB_GET_UINT8(&cursor);
    di->macMajorVersion                   = _UWB_GET_UINT8(&cursor);
    di->macMinorMaintenanceVersion        = _UWB_GET_UINT8(&cursor);
    di->phyMajorVersion                   = _UWB_GET_UINT8(&cursor);
    di->phyMinorMaintenanceVersion        = _UWB_GET_UINT8(&cursor);
    di->mwMajor                           = UWBIOTVER_VER_MAJOR;
    di->mwMinor                           = UWBIOTVER_VER_MINOR;
    di->mwRc                              = UWBIOTVER_VER_DEV;

    // skip 2 bytes of test version
    discard = _UWB_GET_UINT8(&cursor);
    discard = _UWB_GET_UINT8(&cursor);

    ext_length = _UWB_GET_UINT8(&cursor);

    LOG_DBG("Ext len=%u", ext_length);

    next_cursor = cursor;

    while (ext_length > 0)
    {
        cursor  = next_cursor;
        type    = _UWB_GET_UINT8(&cursor);
        length  = _UWB_GET_UINT8(&cursor);
        next_cursor = cursor + length;
        ext_length -= length + 2;

        LOG_DBG("Device Info tag:%02X len:%u", type, length);

        switch (type)
        {
        case UCI_EXT_PARAM_ID_DEVICE_NAME:
            di->devNameLen = length;

            for (index = 0; length > 0; length--)
            {
                if (index < (sizeof(di->devName) - 1))
                {
                    di->devName[index++] = _UWB_GET_UINT8(&cursor);
                }
            }

            di->devName[index] = 0;
            break;

        case UCI_EXT_PARAM_ID_FW_VERSION:
            require(length == 3, exit);
            di->fwMajor = _UWB_GET_UINT8(&cursor);
            di->fwMinor = _UWB_GET_UINT8(&cursor);
            di->fwRc    = _UWB_GET_UINT8(&cursor);
            break;

        case UCI_EXT_PARAM_ID_VENDOR_UCI_VER:
            require(length == 3, exit);
            di->nxpUciMajor = _UWB_GET_UINT8(&cursor);
            di->nxpUciMinor = _UWB_GET_UINT8(&cursor);
            di->nxpUciPatch = _UWB_GET_UINT8(&cursor);
            break;

        case UCI_EXT_PARAM_ID_UWB_CHIP_ID:
            require(length == UWB_MAX_CHIP_ID_LEN, exit);

            for (index = 0; length > 0; length--)
            {
                if (index < (sizeof(di->nxpChipId) - 1))
                {
                    di->nxpChipId[index++] = _UWB_GET_UINT8(&cursor);
                }
            }

            break;

        case UCI_EXT_PARAM_ID_UWBS_MAX_PPM_VALUE:
            require(length == 1, exit);
            di->maxPpmValue = _UWB_GET_UINT8(&cursor);
            break;

        case UCI_EXT_PARAM_ID_TX_POWER:
            require(length == sizeof(uint16_t), exit);
            di->txPowerValue = _UWB_GET_UINT16(&cursor);
            break;

        case UCI_EXT_PARAM_ID_FIRA_EXT_UCI_GENERIC_VER:
            require(length == 3, exit);
            di->uciGenericMajor                   = _UWB_GET_UINT8(&cursor);
            di->uciGenericMinorMaintenanceVersion = _UWB_GET_UINT8(&cursor);
            di->uciGenericPatch                   = _UWB_GET_UINT8(&cursor);
            break;

        case UCI_EXT_PARAM_ID_FIRA_EXT_TEST_VER:
            require(length == 3, exit);
            di->uciTestMajor = _UWB_GET_UINT8(&cursor);
            di->uciTestMinor = _UWB_GET_UINT8(&cursor);
            di->uciTestPatch = _UWB_GET_UINT8(&cursor);
            break;

        case UCI_EXT_PARAM_ID_FW_BOOT_MODE:
            require(length == sizeof(uint8_t), exit);
            di->fwBootMode = _UWB_GET_UINT8(&cursor);
            break;

        case UCI_EXT_PARAM_ID_UCI_CCC_VERSION:
        case UCI_EXT_PARAM_ID_CCC_VERSION:
            break;

        default:
            LOG_WRN("Ignoring device info tag %02X", type);
            break;
        }
    }

    LOG_INF("UWB Model %s", di->devName);
    ret = 0;
exit:
    return ret;
}

static uint64_t _uwb_time_for_state(int state)
{
    uint64_t expect;

    switch (state)
    {
    case UWB_IS_INIT:
        expect = 1000;  // have to load firmware, etc.
        break;

    case UWB_IS_WAIT_NTF:
        expect = 1000;  // no idea how long chip will get its act together
        break;

    case UWB_IS_READY:
    case UWB_IS_INIT_SESSION:
        expect = 2000;  // ranging data slows things down
        break;

    default:
        expect = UWB_STATE_TRANSITION_TIMEOUT_MS;
        break;
    }

    return expect;
}

static int _uwb_write(
    const uint8_t *inData,
    const int inCount)
{
    int ret = -EINVAL;

    require(inData, exit);
    require(inCount, exit);

    uint8_t cnt = inData[3];

    if ((int)cnt != (inCount - UCI_MSG_HDR_SIZE))
    {
        LOG_ERR("cmd cnt != payload  %u != %u", cnt, inCount - UCI_MSG_HDR_SIZE);
    }

    ret = UCIprotoWriteRaw(inData, inCount);
exit:
    return ret;
}

static uint8_t *_uwb_add_session_handle(const uwb_session_t *session, uint8_t *command)
{
    uint32_t id;

    // todo - worry about endianess?
    if (session)
    {
        id = session->session_handle;
    }
    else
    {
        LOG_ERR("No current session for add handle");
        id = 0xDEADFACE;
    }

    memcpy(command + UWB_SESSION_ID_OFFSET_IN_CMD, &id, sizeof(uint32_t));
    return command;
}

static uwb_session_t *_uwb_alloc_session(const uint32_t session_id, const void *ble_conn_ctx)
{
    int i;

    for (i = 0; i < UWB_MAX_SESSIONS; i++)
    {
        if (mUWB.sessions[ i ].session_state == UWB_SS_INACTIVE)
        {
            mUWB.sessions[ i ].session_state = UWB_SS_CREATED;
            mUWB.sessions[ i ].session_id = session_id;
            mUWB.sessions[ i ].session_handle = 0;
            mUWB.sessions[ i ].ble_conn_ctx = ble_conn_ctx;
            mUWB.sessions[ i ].uci_session_state = 0;
            mUWB.sessions[ i ].current_antenna_sel = ANTSEL_INVALID;
            mUWB.sessions[ i ].requested_antenna_sel = ANTSEL_FRONT;
            mUWB.sessions[ i ].range_errors = 0;
            mUWB.sessions[ i ].flop_counter = 0;

            LOG_DBG("Session %08X Allocated for conn %08X",
                    (uint32_t)(uintptr_t)&mUWB.sessions[ i ],
                    (uint32_t)(uintptr_t)ble_conn_ctx);
            break;
        }
    }

    return (i < UWB_MAX_SESSIONS) ? &mUWB.sessions[ i ] : NULL;
}

static void _uwb_list_sessions(void)
{
    int i;

    for (i = 0; i < UWB_MAX_SESSIONS; i++)
    {
        if (mUWB.sessions[ i ].session_state != UWB_SS_INACTIVE)
        {
            LOG_INF("%d conn %08X  id %08X", i,
                    (uint32_t)(uintptr_t)mUWB.sessions[ i ].ble_conn_ctx,  mUWB.sessions[ i ].session_handle);
        }
    }
}

static uwb_session_t *_uwb_find_session_by_handle(const uint32_t session_handle)
{
    int i;

    for (i = 0; i < UWB_MAX_SESSIONS; i++)
    {
        if (mUWB.sessions[ i ].session_state >= UWB_SS_CREATED)
        {
            if (mUWB.sessions[ i ].session_handle == session_handle)
            {
                break;
            }
        }
    }

    if (i >= UWB_MAX_SESSIONS)
    {
        LOG_ERR("Didnt find session %08X", session_handle);
    }

    return (i < UWB_MAX_SESSIONS) ? &mUWB.sessions[ i ] : NULL;
}

static uwb_session_t *_uwb_find_session_by_connection(const void *ble_conn_ctx)
{
    int i;

    for (i = 0; i < UWB_MAX_SESSIONS; i++)
    {
        if (mUWB.sessions[ i ].session_state >= UWB_SS_CREATED)
        {
            if (mUWB.sessions[ i ].ble_conn_ctx == ble_conn_ctx)
            {
                break;
            }
        }
    }

    if (i >= UWB_MAX_SESSIONS)
    {
        LOG_ERR("Didnt find session by ctx %08X", (uint32_t)(uintptr_t)ble_conn_ctx);
        _uwb_list_sessions();
    }

    return (i < UWB_MAX_SESSIONS) ? &mUWB.sessions[ i ] : NULL;
}

static uwb_session_t *_uwb_find_session_by_state(const uwb_session_state_t inState)
{
    int i;

    for (i = 0; i < UWB_MAX_SESSIONS; i++)
    {
        if (mUWB.sessions[ i ].session_state == inState)
        {
            break;
        }
    }

    return (i < UWB_MAX_SESSIONS) ? &mUWB.sessions[ i ] : NULL;
}

static int _uwb_free_session(uint32_t const session_handle)
{
    uwb_session_t *session;

    session = _uwb_find_session_by_handle(session_handle);

    if (session)
    {
        LOG_DBG("Session %08X  ctx %08X, handle %08X deallocated",
                (uint32_t)(uintptr_t)session,
                (uint32_t)(uintptr_t)session->ble_conn_ctx,
                session_handle);
        session->session_handle = 0;
        session->session_state = UWB_SS_INACTIVE;
        session->uci_session_state = 0;
    }

    return session ? 0 : -1;
}

static int _uwb_session_count(void)
{
    int count = 0;
    int i;

    for (i = 0; i < UWB_MAX_SESSIONS; i++)
    {
        if (mUWB.sessions[ i ].session_state >= UWB_SS_CREATED)
        {
            count++;
        }
    }

    return count;
}

static int _uwb_set_session_state(uwb_session_t *session, const uint8_t sess_state, const uint8_t sess_reason)
{
    int ret = -1;

    require(session, exit);

    session->uci_session_state = sess_state;

    switch (sess_state)
    {
    case UWB_SESSION_INITIALIZED:
        // A session we created locally is ready to start
        session->session_state = UWB_SS_INITIALIZED;
        LOG_INF("UWBS new session handle %08X", session->session_handle);
        break;

    case UWB_SESSION_DEINITIALIZED:
        session->session_state = UWB_SS_STOPPED;;
        LOG_INF("Session %08X de-initialized", session->session_handle);
        _uwb_free_session(session->session_handle);
        break;

    case UWB_SESSION_ACTIVE:
        session->session_state = UWB_SS_STARTED;
        LOG_INF("Session %08X Active!", session->session_handle);
        break;

    case UWB_SESSION_IDLE:
        if (session->session_state == UWB_SS_CREATED || session->session_state == UWB_SS_INITIALIZED)
        {
            // An idle session is created and configured
            // and is waiting for init-ranging
            session->session_state = UWB_SS_INITIALIZED;
            LOG_INF("Session %08X idle, starting", session->session_handle);
        }
        else if (session->session_state == UWB_SS_STOPPING)
        {
            session->session_state = UWB_SS_STOPPED;
            LOG_INF("Session %08X idle, stopped", session->session_handle);
        }
        else
        {
            // An active session is idled, what to do?
            session->session_state = UWB_SS_IDLE;
            LOG_INF("Session %08X idle", session->session_handle);
        }

        break;

    case UWB_SESSION_ERROR:
        session->session_state = UWB_SS_STOPPING;;
        LOG_DBG("Session %08X error", session->session_handle);
        break;

    default:
        LOG_WRN("unhandled sess state %02X", sess_state);
        break;
    }

    ret = 0;

exit:
    return ret;
}

static int _uwb_build_antenna_selector(const uint8_t inSelection)
{
    int ret = -1;
    uint8_t *cursor = mUWB.antenna_selector;
    int room = sizeof(mUWB.antenna_selector);
    int length;
    uint8_t tx_idx = 1;

    // advance 4 bytes for UCI header
    ret = _UWB_PUT_UINT32(&cursor, &room, 0);
    require_noerr(ret, exit);

    // advance 4 bytes for session id
    ret = _UWB_PUT_UINT32(&cursor, &room, 0);
    require_noerr(ret, exit);

    // number of parameters
    ret  = _UWB_PUT_UINT8(&cursor, &room, 2);
    require_noerr(ret, exit);

    // Antenna config Tx
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_VENDOR_PARAM_ID_ANTENNAE_CONFIGURATION_TX);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 3);   // byte count
    ret |= _UWB_PUT_UINT8(&cursor, &room, 1);   // 1 tx antenna follows

    if (inSelection == ANTSEL_BACK)
    {
        if (mUWB.antenna_mode == ANTMODE_THREE_FRONT)
        {
            //LOG_WRN( "No f/b switching in 3-front mode" );
        }
        else if (mUWB.antenna_mode == ANTMODE_TWO_FRONT_ONE_BACK)
        {
            tx_idx = 2;
        }
        else if (mUWB.antenna_mode == ANTMODE_ONE_FRONT_ONE_BACK)
        {
            tx_idx = 2;
        }
        else
        {
            LOG_ERR("Bad antenna mode");
        }
    }

    ret |= _UWB_PUT_UINT8(&cursor, &room, tx_idx);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
    require_noerr(ret, exit);

    // Antenna config Rx
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_VENDOR_PARAM_ID_ANTENNAE_CONFIGURATION_RX);

    if (mUWB.antenna_mode == ANTMODE_THREE_FRONT)
    {
        ret |= _UWB_PUT_UINT8(&cursor, &room, 4);   // byte count
        // configure mode 1 (dual Rx)
        ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
        // use H and V antenna pairs for azimuth and elevation
        ret |= _UWB_PUT_UINT8(&cursor, &room, 2);   // 2 rx antenna follows (H,V)
        ret |= _UWB_PUT_UINT8(&cursor, &room, 1);   // pair ID 1 (H)
        ret |= _UWB_PUT_UINT8(&cursor, &room, 2);   // pair ID 2 (V)
    }
    else if (mUWB.antenna_mode == ANTMODE_TWO_FRONT_ONE_BACK)
    {
        ret |= _UWB_PUT_UINT8(&cursor, &room, 3);   // byte count
        // configure mode 1 (dual Rx)
        ret |= _UWB_PUT_UINT8(&cursor, &room, 1);
        // use H and V antenna pairs for azimuth and elevation
        ret |= _UWB_PUT_UINT8(&cursor, &room, 1);   // 1 rx antenna pair follows (H)

        if (inSelection == ANTSEL_FRONT)
        {
            ret |= _UWB_PUT_UINT8(&cursor, &room, 1);   // pair ID 1 (H outside)
        }
        else
        {
            ret |= _UWB_PUT_UINT8(&cursor, &room, 2);   // pair ID 2 (H inside)
        }
    }
    else /* one front one back, just one the single rx antenna */
    {
        ret |= _UWB_PUT_UINT8(&cursor, &room, 3);   // byte count
        // configure mode 0 (single Rx)
        ret |= _UWB_PUT_UINT8(&cursor, &room, 0);
        // use H+(inside or outside)
        ret |= _UWB_PUT_UINT8(&cursor, &room, 1);   // 1 rx antenna follows

        if (inSelection == ANTSEL_FRONT)
        {
            ret |= _UWB_PUT_UINT8(&cursor, &room, 1);   // antenna ID 1 (outside)
        }
        else
        {
            ret |= _UWB_PUT_UINT8(&cursor, &room, 2);   // antenna ID 2 (inside)
        }
    }

    require_noerr(ret, exit);

    length = cursor - mUWB.antenna_selector;

    // reset cursor to beginning, add UCI header
    cursor = mUWB.antenna_selector;
    room = sizeof(mUWB.antenna_selector);
    ret  = _UWB_PUT_UINT8(&cursor, &room, UCI_MTS_CMD | UCI_GID_VENDOR);
    ret |= _UWB_PUT_UINT8(&cursor, &room, UCI_MSG_SESSION_VENDOR_SET_APP_CONFIG);
    ret |= _UWB_PUT_UINT8(&cursor, &room, 0x00);
    ret |= _UWB_PUT_UINT8(&cursor, &room, length - UCI_MSG_HDR_SIZE);
    require_noerr(ret, exit);

    mUWB.antenna_selector_length = length;
    ret = 0;
exit:
    return ret;
}

static int _uwb_initialize(
    bool    haveMessage,
    uint8_t type,
    uint8_t gid,
    uint8_t oid,
    uint8_t *payload,
    int payloadLength)
{
    int ret = 0;
    uint8_t status;
    uwb_session_t *session;
    uint32_t session_handle;
    uint8_t ant_bit;

    LOG_DBG("Init UWBS state %d [%d of %d]", mUWB.init_state, mUWB.command_set_state, mUWB.command_set_count);

    if (haveMessage && (type == UCI_MT_NTF))
    {
        if (gid == UCI_GID_CORE && oid == UCI_MSG_CORE_DEVICE_STATUS_NTF)
        {
            status = 0;

            if (payloadLength > 0)
            {
                status = payload[0];
            }

            if (status)
            {
                if (mUWB.next_init_state == UWB_IS_RESET)
                {
                    LOG_DBG("UWBS Ready after devid set, reset");
                    UWB_NEXT_STATE(mUWB.next_init_state);
                }
                else if (mUWB.next_init_state == UWB_IS_SET_CONFIG)
                {
                    LOG_DBG("UWBS Ready after reset, set config");
                    UWB_NEXT_STATE(mUWB.next_init_state);
                }
                else
                {
                    LOG_DBG("UWBS Ready, no action needed");
                }
            }
            else
            {
                LOG_DBG("UWBS Not Ready");
            }
        }
        else if (gid == UCI_GID_SESSION_MANAGE && oid == UCI_MSG_SESSION_STATUS_NTF)
        {
            if (payloadLength >= 6)
            {
                uint8_t  sess_state;
                uint8_t  sess_reason;

                memcpy(&session_handle, payload, 4);

                sess_state  = payload[4];
                sess_reason = payload[5];

                LOG_DBG("Session %08X state %02X %02X", session_handle, sess_state, sess_reason);

                session = _uwb_find_session_by_handle(session_handle);
                _uwb_set_session_state(session, sess_state, sess_reason);

                if (mUWB.session_callback)
                {
                    mUWB.session_callback(session, sess_state, sess_reason);
                }

                UWB_NEXT_STATE(UWB_IS_READY);
            }
            else
            {
                LOG_WRN("bad payload for sess ntf");
            }
        }
        else if (gid == UCI_GID_RANGE_MANAGE && oid == 0x00)
        {
            uint8_t *cursor = payload + 4;

            // session id is uint32 4 bytes into range payload
            session_handle = _UWB_GET_UINT32(&cursor);
            session = _uwb_find_session_by_handle(session_handle);

            if (session)
            {
                int rret = UWBrangeData(session->current_antenna_sel, payload, payloadLength);

                if (rret)
                {
                    if (mUWB.stopOnRangeErrors)
                    {
                        session->range_errors++;

                        // if consequetive range errors get big assume
                        // the session is hopeless and stop it
                        //
                        if (session->range_errors > UWB_MAX_RANGE_ERRORS)
                        {
                            if (session && session->session_state < UWB_SS_STOPPING)
                            {
                                LOG_ERR("Too many consequetive range errors, stopping %08X", session_handle);
                                session->session_state = UWB_SS_STOPPING;
                            }
                        }
                    }
                    else
                    {
                        LOG_WRN("range error");
                    }
                }
                else
                {
                    session->range_errors = 0;
                }

                if (mUWB.flop_rate)
                {
                    // note we flop antenna regardless of errors
                    session->flop_counter++;

                    if (session->flop_counter >= mUWB.flop_rate)
                    {
                        session->flop_counter = 0;
                        session->requested_antenna_sel = (session->current_antenna_sel == ANTSEL_FRONT) ? ANTSEL_BACK : ANTSEL_FRONT;
                    }
                }
            }
        }
        else if (gid == UCI_GID_PROPRIETARY_SE)
        {
            switch (oid)
            {
            case EXT_UCI_MSG_READ_CALIB_DATA_CMD:

                // use the calib data to update the commands we use to
                // setup the h/w.  this is really hacky, maybe be
                // smarter about this?
                //
                // as you can see, this is a horrific use of payload length as
                // a command descriminator.. why did nxp not just invent sub cmds?
                //
                if (payloadLength == 0x05)
                {
                    /*UWB_EXT_READ_CALIB_DATA_XTAL_CAP_NTF*/
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5[8]  = payload[2];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5[10] = payload[3];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5[12] = payload[4];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9[8]  = payload[2];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9[10] = payload[3];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9[12] = payload[4];
                    mUWB.do_OTP_Read_XTAL = false;
                }
                else if (payloadLength == 0x06)
                {
                    /*UWB_EXT_READ_CALIB_DATA_TX_POWER_NTF*/
                    uint8_t offset;
                    offset = (uint8_t)((int)payload[2] + (int)(mUWB.power_offset + ((2.1 - 0.6 + 0.5) * 4)));               /* murata evk */
#if 1 // TODO - getting out of range errors with these?
                    UWB_SET_CALIBRATION_TX_POWER_CH5[11] = 0;
                    UWB_SET_CALIBRATION_TX_POWER_CH9[11] = 0;
                    UWB_SET_CALIBRATION_TX_POWER_CH5[9] = 0;
                    UWB_SET_CALIBRATION_TX_POWER_CH9[9] = 0;
#else
                    UWB_SET_CALIBRATION_TX_POWER_CH5[11] = offset;
                    UWB_SET_CALIBRATION_TX_POWER_CH9[11] = offset;
                    UWB_SET_CALIBRATION_TX_POWER_CH5[9] = payload[3];
                    UWB_SET_CALIBRATION_TX_POWER_CH9[9] = payload[3];
#endif
                    mUWB.do_OTP_Read_Power = false;
                }
                else
                {
                    LOG_WRN("Unhandled read-calib-data ntf");
                }

                if (mUWB.init_state == UWB_IS_WAIT_NTF)
                {
                    UWB_NEXT_STATE(mUWB.next_init_state);
                }

                break;

            default:
                break;
            }
        }

        haveMessage = false;
    }

    if (mUWB.init_state != UWB_IS_WAIT_RSP && mUWB.command_set_count)
    {
        if (mUWB.command_set_state < mUWB.command_set_count)
        {
            // send next command to uwbs and wait for reply
            //
            ret = _uwb_write(mUWB.commands[mUWB.command_set_state], mUWB.command_size[mUWB.command_set_state]);
            mUWB.next_init_state = mUWB.init_state;
            mUWB.init_state = UWB_IS_WAIT_RSP;
        }
    }
    else
    {
        session = NULL;

        if (mUWB.init_state == UWB_IS_READY)
        {
            int i;

            // have intiialized the uwb chip and nothing to do, so
            // check for any sessions wanting to stop, init, or start
            //
            for (i = 0; i < UWB_MAX_SESSIONS && !session; i++)
            {
                session = &mUWB.sessions[ i ];

                switch (session->session_state)
                {
                case UWB_SS_STOPPED:
                    LOG_INF("Deinit session %08X", session->session_handle);
                    UWB_NEXT_STATE(UWB_IS_DEINIT_SESSION);
                    break;

                case UWB_SS_STOPPING:
                    LOG_INF("Stopping session %08X", session->session_handle);
                    UWB_NEXT_STATE(UWB_IS_STOP_SESSION);
                    break;

                case UWB_SS_STARTING:
                    LOG_INF("Staring session %08X", session->session_handle);
                    UWB_NEXT_STATE(UWB_IS_START_SESSION);
                    break;

                case UWB_SS_INITIALIZED:
                    LOG_INF("Configure session %08X", session->session_handle);
                    UWB_NEXT_STATE(UWB_IS_APP_CONFIG_SESSION);
                    break;

                case UWB_SS_CREATED:
                    ;
                    LOG_INF("Initializing session %08X", session->session_handle);
                    UWB_NEXT_STATE(UWB_IS_INIT_SESSION);
                    break;

                case UWB_SS_STARTED:
                    if (session->requested_antenna_sel != session->current_antenna_sel)
                    {
                        LOG_DBG("Session %08X requesting Antenna %d", session->session_handle, session->requested_antenna_sel);
                        UWB_NEXT_STATE(UWB_IS_ANTENNA_SELECT_SESSION);
                    }
                    else
                    {
                        session = NULL;
                    }

                    break;

                default:
                    session = NULL;
                    break;
                }
            }
        }

        switch (mUWB.init_state)
        {
        case UWB_IS_INIT:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_INIT_BOARD_VARIANT_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_INIT_BOARD_VARIANT;
            mUWB.command_set_state = 0;
            break;

        case UWB_IS_RESET:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_RESET_DEVICE_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_RESET_DEVICE;
            mUWB.command_set_state = 0;
            break;

        case UWB_IS_SET_CONFIG:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_CORE_SET_CONFIG_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_CORE_SET_CONFIG;
            mUWB.command_size[mUWB.command_set_count] = UWB_VENDOR_COMMAND_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_VENDOR_COMMAND;

            if (mUWB.do_get_device_info_and_caps)
            {
                mUWB.command_size[mUWB.command_set_count] = UWB_CORE_GET_DEVICE_INFO_CMD_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_CORE_GET_DEVICE_INFO_CMD;
                mUWB.command_size[mUWB.command_set_count] = UWB_CORE_GET_CAPS_INFO_CMD_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_CORE_GET_CAPS_INFO_CMD;
            }

            if (mUWB.antenna_mode == ANTMODE_THREE_FRONT)
            {
                mUWB.command_size[mUWB.command_set_count] = UWB_CORE_SET_ANTENNAS_DEFINE_2BP_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_CORE_SET_ANTENNAS_DEFINE_2BP;
            }
            else if (mUWB.antenna_mode == ANTMODE_TWO_FRONT_ONE_BACK)
            {
                mUWB.command_size[mUWB.command_set_count] = UWB_CORE_SET_ANTENNAS_DEFINE_2JE_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_CORE_SET_ANTENNAS_DEFINE_2JE;
            }
            else if (mUWB.antenna_mode == ANTMODE_ONE_FRONT_ONE_BACK)
            {
                mUWB.command_size[mUWB.command_set_count] = UWB_CORE_SET_ANTENNAS_DEFINE_1F1B_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_CORE_SET_ANTENNAS_DEFINE_1F1B;
            }
            else
            {
                LOG_ERR("Bad antenna mode %02X", mUWB.antenna_mode);
            }

            mUWB.command_set_state = 0;
            break;

        case UWB_IS_READ_OTP_XTAL:
            mUWB.command_set_count = 0;

            if (mUWB.do_OTP_Read_XTAL)
            {
                // read calibration OTP at least once
                mUWB.command_size[mUWB.command_set_count] = UWB_EXT_READ_CALIB_DATA_XTAL_CAP_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_EXT_READ_CALIB_DATA_XTAL_CAP;
            }
            else
            {
                UWB_NEXT_STATE(UWB_IS_READ_OTP_TXPOWER);
            }

            mUWB.command_set_state = 0;
            break;

        case UWB_IS_READ_OTP_TXPOWER:
            mUWB.command_set_count = 0;

            if (mUWB.do_OTP_Read_Power)
            {
                mUWB.command_size[mUWB.command_set_count] = UWB_EXT_READ_CALIB_DATA_TX_POWER_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_EXT_READ_CALIB_DATA_TX_POWER;
            }
            else
            {
                UWB_NEXT_STATE(UWB_IS_CALIBRATE);
            }

            mUWB.command_set_state = 0;
            break;

        case UWB_IS_CALIBRATE:
            mUWB.command_set_count = 0;

            if (mUWB.channel_id == 0x05)
            {
                if (mUWB.do_AoA_Calibration)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH5;
                    /*
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH5;
                    */
                }

                if (mUWB.do_Calibration)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_TX_POWER_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_TX_POWER_CH5;
                }
            }
            else /* channel 0x09 */
            {
                if (mUWB.do_AoA_Calibration)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9;
                    /*
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH9;
                    */
                }

                if (mUWB.do_Calibration)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9;
                    // TODO - this breaks latest aliro f/w
                    //mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_TX_POWER_CH9_SIZE;
                    //mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_TX_POWER_CH9;
                }
            }

            mUWB.command_set_state = 0;
            break;

        case UWB_IS_READY:
            break;

        case UWB_IS_INIT_SESSION:
            mUWB.command_set_count = 0;

            if (mUWB.configDataLength != 0 && mUWB.configDataIsProfile)
            {
                // an iOS session, pass profile from mobile direct to uwb chip f/w
                // this will initialize a session and go right to idle state ntf
                // (as opposed to init-ranging which will just get initialized)
                //
                mUWB.command_size[mUWB.command_set_count] = mUWB.configDataLength;
                mUWB.commands[mUWB.command_set_count++] = mUWB.configData;
            }
            else
            {
                // a local or non-iOS session, should be one new session in active list
                // generate a random session id to give to uwb f/w
                // when the session init ntf comes back well remember the handle
                // it gives us then
                //
                session = _uwb_find_session_by_state(UWB_SS_CREATED);

                if (session)
                {
                    // use id as handle for the add-session-handle call below
                    session->session_handle = session->session_id;
                }
                else
                {
                    LOG_ERR("No new session for init-session");
                }

                mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_INIT_RANGING_SIZE;
                mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_SESSION_INIT_RANGING);
            }

            mUWB.command_set_state = 0;
            break;

        case UWB_IS_APP_CONFIG_SESSION:
            mUWB.command_set_count = 0;

            if (mUWB.configDataLength != 0)
            {
                // if mobile provideds config dats, no need to set all config
                //
                if (!mUWB.configDataIsProfile)
                {
                    // didnt set config in set-profile, so set it here
                    //
                    mUWB.command_size[mUWB.command_set_count] = mUWB.configDataLength;
                    mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, mUWB.configData);
                }

                // set canned/common config
                mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_XAPP_CONFIG_SIZE;
                mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_SESSION_SET_XAPP_CONFIG);
            }
            else
            {
                // setup app config
                //
                mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_APP_CONFIG_SIZE;
                mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_SESSION_SET_APP_CONFIG);
                mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_APP_CONFIG_NXP_SIZE;
                mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_SESSION_SET_APP_CONFIG_NXP);

                // canned / cli session, use pre-fab app config
                //
                if (mUWB.is_responder)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_RESPONDER_CONFIG_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_SESSION_SET_RESPONDER_CONFIG);
                }
                else
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_INITIATOR_CONFIG_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_SESSION_SET_INITIATOR_CONFIG);
                }
            }

            // Setup initial antenna selection
            session->requested_antenna_sel = ANTSEL_FRONT;
            session->current_antenna_sel = ANTSEL_INVALID;

            ant_bit = (session->requested_antenna_sel == ANTSEL_BACK) ? 1 : 0;
            gpio_pin_configure_dt(&mAnt_Sel, ((ant_bit ^ mUWB.invert_antenna) & 1) ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);

            _uwb_build_antenna_selector(session->requested_antenna_sel);

            if (mUWB.antenna_selector_length > 0)
            {
                mUWB.command_size[mUWB.command_set_count] = mUWB.antenna_selector_length;
                mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, mUWB.antenna_selector);
            }

            mUWB.command_set_state = 0;
            break;

        case UWB_IS_START_SESSION:
            mUWB.command_set_count = 0;
#if 0 // dont send debug setups for internal logging
            mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_DEBUG_CONFIG_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_SESSION_SET_DEBUG_CONFIG);
#endif
            mUWB.command_size[mUWB.command_set_count] = UWB_RANGE_START_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_RANGE_START);
            mUWB.command_set_state = 0;
            break;

        case UWB_IS_ANTENNA_SELECT_SESSION:
            if (session)
            {
                ant_bit = (session->requested_antenna_sel == ANTSEL_BACK) ? 1 : 0;
                gpio_pin_configure_dt(&mAnt_Sel, ((ant_bit ^ mUWB.invert_antenna) & 1) ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);

                _uwb_build_antenna_selector(session->requested_antenna_sel);

                if (mUWB.antenna_selector_length > 0)
                {
                    mUWB.command_size[mUWB.command_set_count] = mUWB.antenna_selector_length;
                    mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, mUWB.antenna_selector);
                }

                session->current_antenna_sel = session->requested_antenna_sel;

#if 0

                // when we change antennas, any filtering of range data is probably out of date
                // enough to warrant a reset if flop rate is > 3 or so.
                //
                if (mUWB.flop_rate > 3)
                {
                    UWBrangeResetFilter(session->session_handle, session->current_antenna_sel);
                }

#endif
            }

            UWB_NEXT_STATE(UWB_IS_READY);
            break;

        case UWB_IS_STOP_SESSION:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_RANGE_STOP_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_RANGE_STOP);
            mUWB.command_set_state = 0;
            break;

        case UWB_IS_DEINIT_SESSION:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_DEINIT_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_handle(session, UWB_SESSION_DEINIT);
            mUWB.command_set_state = 0;

            // for some reason chip doesn't send a notification for deinit
            // so announce it ourselves by-hand while we know what session it is
            //
            if (mUWB.session_callback)
            {
                mUWB.session_callback(session, UWB_SESSION_DEINITIALIZED, 0);
            }

            _uwb_set_session_state(session, UWB_SESSION_DEINITIALIZED, 0);
            break;

        case UWB_IS_WAIT_RSP:
            if (!haveMessage)
            {
                break;
            }

            if (type != UCI_MT_RSP)
            {
                LOG_WRN("Unexpected UCI %02X %02X %02X", type, gid, oid);
                break;
            }

            status = 0;

            if (payload && payloadLength > 0)
            {
                status = payload[0];
            }

            if (status == 0)
            {
                // got an OK response, parse response of things we care about
                //
                if (gid == UCI_GID_CORE)
                {
                    if (oid == UCI_MSG_CORE_DEVICE_INFO)
                    {
                        __uwb_parse_device_info(payload, payloadLength);
                        mUWB.do_get_device_info_and_caps = false;
                    }
                }

                // got an OK response, go back to state we were in
                //
                UWB_NEXT_STATE(mUWB.next_init_state);

                if (mUWB.command_set_state < mUWB.command_set_count)
                {
                    // We sent a command set ok, count that
                    //
                    mUWB.command_set_state++;
                }

                if (mUWB.command_set_state >= mUWB.command_set_count)
                {
                    mUWB.command_set_count = 0;
                    mUWB.command_set_state = 0;

                    // Finished a command set, advance state
                    //
                    switch (mUWB.init_state)
                    {
                    case UWB_IS_INIT:
                        // wait for ready ntf before doing a reset
                        UWB_NEXT_STATE(UWB_IS_WAIT_NTF);
                        mUWB.next_init_state = UWB_IS_RESET;
                        break;

                    case UWB_IS_RESET:
                        // wait for ready ntf before set config
                        UWB_NEXT_STATE(UWB_IS_WAIT_NTF);
                        mUWB.next_init_state = UWB_IS_SET_CONFIG;
                        break;

                    case UWB_IS_SET_CONFIG:
                        UWB_NEXT_STATE(UWB_IS_READ_OTP_XTAL);
                        break;

                    case UWB_IS_READ_OTP_XTAL:
                        // wait for otp notification before moving on?
#if  1
                        UWB_NEXT_STATE(UWB_IS_WAIT_NTF);
                        mUWB.next_init_state = UWB_IS_READ_OTP_TXPOWER;
#else
                        UWB_NEXT_STATE(UWB_IS_READ_OTP_TXPOWER);
#endif
                        break;

                    case UWB_IS_READ_OTP_TXPOWER:
                        // wait for otp notification before moving on?
#if  1
                        UWB_NEXT_STATE(UWB_IS_WAIT_NTF);
                        mUWB.next_init_state = UWB_IS_CALIBRATE;
#else
                        UWB_NEXT_STATE(UWB_IS_CALIBRATE);
#endif
                        break;

                    case UWB_IS_CALIBRATE:
                        if (mUWB.one_time_init_request)
                        {
                            // this path is used at boot to determine if h/w is
                            // really present
                            //
                            LOG_DBG("Did 1-time init");
                            mUWB.one_time_init_request = false;
                            UWB_NEXT_STATE(UWB_IS_INIT);
                            mUWB.state = UWB_IDLE;
                        }
                        else
                        {
                            // when the response to calibrate comes in, the system is ready
                            // to run sessions, so this is as far as this second-level
                            // state machine goes
                            //
                            UWB_NEXT_STATE(UWB_IS_READY);
                        }

                        break;

                    case UWB_IS_READY:
                        break;

                    case UWB_IS_INIT_SESSION:

                        // The response to an init-ranging or set-profile command
                        // contains the session handle from the device (this is
                        // an nxp extension even for init-ranging) so use the
                        // response to set our session id for the newest allocated
                        // session context.
                        //
                        // it is NOT an error if there is no payload, the ntf
                        // will have the new session handle in that case
                        //
                        if (
                            (gid == UCI_GID_SESSION_MANAGE && oid == UCI_MSG_SESSION_INIT)
                            || (gid == UCI_GID_PROPRIETARY_SE && oid == EXT_UCI_MSG_SET_PROFILE)
                        )
                        {
                            if (payloadLength >= 5)
                            {
                                uwb_session_t *session;
                                uint32_t session_handle;

                                memcpy(&session_handle, payload + 1, 4);

                                LOG_INF("Response to %s sets session id to %08X",
                                        (gid == UCI_GID_SESSION_MANAGE) ? "INIT-RANGING" : "SET_PROFILE", session_handle);

                                session = _uwb_find_session_by_state(UWB_SS_CREATED);

                                if (!session)
                                {
                                    LOG_ERR("No new session waiting to init for %08X", session_handle);
                                    UWB_NEXT_STATE(UWB_IS_READY);
                                    break;
                                }

                                LOG_DBG("Session %08X gets id %08X", (uint32_t)(uintptr_t)session, session_handle);
                                session->session_handle = session_handle;
                            }
                        }

                        // after an init-session, need to wait for an initialized
                        // notification before we can advance to config app and start
                        //
                        UWB_NEXT_STATE(UWB_IS_WAIT_NTF);
                        mUWB.next_init_state = UWB_IS_READY;
                        break;

                    case UWB_IS_APP_CONFIG_SESSION:

                        // app config response will be for the first session initialized,
                        // so advance that sessions state to starting
                        //
                        session = _uwb_find_session_by_state(UWB_SS_INITIALIZED);

                        if (session)
                        {
                            LOG_INF("Session %08X configured, starting", session->session_handle);
                            session->session_state = UWB_SS_STARTING;
                        }
                        else
                        {
                            LOG_ERR("No initialized session to advance on app-config");
                        }

                        UWB_NEXT_STATE(UWB_IS_READY);
                        break;

                    case UWB_IS_START_SESSION:
                        // after a start-session, need to wait for active
                        // notification to ensure we started it ok which will
                        // advance session state
                        //
                        UWB_NEXT_STATE(UWB_IS_WAIT_NTF);
                        mUWB.next_init_state = UWB_IS_READY;
                        break;

                    case UWB_IS_ANTENNA_SELECT_SESSION:
                        UWB_NEXT_STATE(UWB_IS_READY);
                        break;

                    case UWB_IS_STOP_SESSION:
                        // wait for session status to go idle or less to de-init
                        UWB_NEXT_STATE(UWB_IS_WAIT_NTF);
                        mUWB.next_init_state = UWB_IS_READY;
                        break;

                    case UWB_IS_DEINIT_SESSION:
                        if (_uwb_session_count() == 0)
                        {
                            LOG_INF("No more sessions, deinit UCI");
                            mUWB.init_state = UWB_IS_INIT;
                            mUWB.state = UWB_IDLE;
                            UCIprotoDeInit();
                        }
                        else
                        {
                            UWB_NEXT_STATE(UWB_IS_READY);
                        }

                        break;

                    case UWB_IS_WAIT_RSP:
                    case UWB_IS_WAIT_NTF:
                        LOG_WRN("Shouldnt be here");
                        break;
                    }
                }
            }
            else
            {
                // UCI error, just announce it and let response timeout kill session
                //
                switch (status)
                {
                case UCI_STATUS_REJECTED:
                    LOG_ERR("UCI rejected (state %d)", mUWB.next_init_state);
                    break;

                case UCI_STATUS_FAILED:
                    LOG_ERR("UCI failed (state %d)", mUWB.next_init_state);
                    break;

                case UCI_STATUS_SYNTAX_ERROR:
                    LOG_ERR("UCI syntax (state %d)", mUWB.next_init_state);
                    break;

                case UCI_STATUS_INVALID_PARAM:
                    LOG_ERR("UCI invalid param (state %d)", mUWB.next_init_state);
                    break;

                case UCI_STATUS_SESSSION_NOT_EXIST:
                    LOG_ERR("No existing session for command (state %d)", mUWB.next_init_state);
                    break;

                case UCI_STATUS_SESSSION_ACTIVE:
                    LOG_ERR("Session already active (state %d)", mUWB.next_init_state);
                    break;

                case UCI_STATUS_MAX_SESSSIONS_EXCEEDED:
                    LOG_ERR("To many existing sessions (state %d)", mUWB.next_init_state);
                    break;

                case UCI_STATUS_SESSION_NOT_CONFIGURED:
                    LOG_ERR("Session not configured (state %d)", mUWB.next_init_state);
                    break;

                default:
                    LOG_ERR("Status %02X in response (state %d)", mUWB.next_init_state, status);
                    break;
                }
            }

            break;

        case UWB_IS_WAIT_NTF:
            break;

        default:
            // shouldn't get here
            UWB_NEXT_STATE(UWB_IS_READY);
            mUWB.state = UWB_SESSION;
            break;
        }
    }

    return ret;
}

void _uwb_reset(void)
{
    int i;

    // callback for any active sessions with connections
    //
    if (mUWB.session_callback)
    {
        for (i = 0; i < UWB_MAX_SESSIONS; i++)
        {
            if (mUWB.sessions[i].session_state != UWB_SS_INACTIVE)
            {
                mUWB.session_callback(&mUWB.sessions[i], UWB_SESSION_DEINITIALIZED, 0);
            }
        }
    }

    UCIprotoDeInit();
    mUWB.state = UWB_IDLE;
    mUWB.init_state = UWB_IS_INIT;
    mUWB.uci_session_state = UWB_SESSION_DEINITIALIZED;
    mUWB.command_set_count = 0;
    mUWB.command_set_state = 0;
    mUWB.configDataLength = 0;
}

int UWBgetDeviceInfo(uwb_device_info_t **outDevInfo)
{
    int ret = -EINVAL;

    require(outDevInfo, exit);
    *outDevInfo = &mUWB.device_info;
    ret = -ENODEV;
    require(!mUWB.do_get_device_info_and_caps, exit);
    ret = 0;
exit:
    return ret;
}

uwb_session_t *UWBsessionFromConnection(const void *inConnectionHandle)
{
    return _uwb_find_session_by_connection(inConnectionHandle);
}

int UWBstart(
    const uint8_t inType,
    const uint32_t inSessionId,
    const bool inConfigDataIsProfile,
    const void *inConnectionHandle,
    const uint8_t *inConfigData,
    const int inConfigDataLength)
{
    uwb_session_t *session;
    int ret = -EINVAL;

    session = _uwb_alloc_session(inSessionId, inConnectionHandle);
    require(session, exit);

    if (inType == UWB_DeviceType_Controller)
    {
        mUWB.is_responder = false;
    }
    else
    {
        mUWB.is_responder = true;
    }

    if (inConfigData && inConfigDataLength)
    {
        require(inConfigDataLength < sizeof(mUWB.configData), exit);
        memcpy(mUWB.configData, inConfigData, inConfigDataLength);
        mUWB.configDataLength = inConfigDataLength;
        mUWB.configDataIsProfile = inConfigDataIsProfile;
        LOG_INF("Starting NI Session with %s",
                inConfigDataIsProfile ? "Profile" : "App Data");
    }
    else
    {
        mUWB.configDataLength = 0;
        mUWB.configDataIsProfile = false;
        LOG_INF("Starting UWB Session");
    }

    TimeSignalApplicationEvent();
    ret = 0;
exit:
    return ret;
}

int UWBstop(const void *inConnectionHandle)
{
    int ret = -EINVAL;

    if (mUWB.state != UWB_IDLE)
    {
        uwb_session_t *session;

        if (inConnectionHandle)
        {
            session = _uwb_find_session_by_connection(inConnectionHandle);
            if (session)
            {
                session->session_state = UWB_SS_STOPPING;
                LOG_INF("Stopping session %08X", session->session_handle);
                ret = 0;
            }
        }
        else
        {
            for (int i = 0; i < UWB_MAX_SESSIONS; i++)
            {
                if (mUWB.sessions[i].session_state != UWB_SS_INACTIVE)
                {
                    mUWB.sessions[i].session_state = UWB_SS_STOPPING;
                    LOG_INF("Stopping session %08X", mUWB.sessions[i].session_handle);
                }
            }

            ret = 0;
        }
    }
    else
    {
        LOG_WRN("Not in a session, not stopping");
    }

    TimeSignalApplicationEvent();
    return ret;
}

bool UWBReady(void)
{
    return mUWB.state == UWB_IDLE;
}

int UWBslice(uint32_t *delay)
{
    int ret = 0;
    bool gotMessage;
    uint8_t type;
    uint8_t gid;
    uint8_t oid;
    uint8_t *payload;
    int     payloadLength;
    uwb_session_t *session;

    if (mUWB.sliceErrors > 100)
    {
        // we give up, assume there is no hardware
        return 0;
    }

    gotMessage = false;

    if (mUWB.state != UWB_IDLE)
    {
        ret = UCIprotoSlice(&gotMessage, &type, &gid, &oid, &payload, &payloadLength, delay);

        if (ret)
        {
            LOG_ERR("UCI Error resets UWB");
            _uwb_reset();
            mUWB.sliceErrors++;
        }
        else
        {
            mUWB.sliceErrors = 0;
        }
    }
    else
    {
        ret = 0;
    }

    switch (mUWB.state)
    {
    case UWB_IDLE:
        session = _uwb_find_session_by_state(UWB_SS_CREATED);

        if (session || (mUWB.one_time_init_request && (k_uptime_get() > 500)))
        {
            // Bring up the UCI interface
            // (setup SPI, load f/w and init UCI)
            //
            ret = UCIprotoInit(mUWB.dump_proto);

            mUWB.state = UWB_SESSION;
            UWB_NEXT_STATE(UWB_IS_INIT);
            mUWB.command_set_count = 0;
            mUWB.command_set_state = 0;

            *delay = 20; // let chip boot
        }

        break;

    case UWB_SESSION:
        if (UCIready())
        {
            ret = _uwb_initialize(gotMessage, type, gid, oid, payload, payloadLength);

            if (mUWB.init_state == UWB_IS_WAIT_RSP || mUWB.init_state == UWB_IS_WAIT_NTF)
            {
                // SPI interrupt will shorten delay in wait-app-event in main loop
                // so ok to delay a bunch while waiting for uci response
                //
                *delay = 100;
            }
            else if (mUWB.init_state == UWB_IS_READY)
            {
                // just waiting for range data, no need to loop fast, but poll at
                // 1/2 the range interval in case missed irq?
                //
                *delay = 40;
            }
            else
            {
                // go right to next command send, no delay
                *delay = 0;
            }

            // check state transition timer. if it expires, reset states
            //
            if (mUWB.init_state != UWB_IS_READY)
            {
                volatile uint64_t now = k_uptime_get();

                if (now  > mUWB.state_timer)
                {
                    LOG_ERR("Did not transition from state %d to %d, resetting states",
                            mUWB.init_state, mUWB.next_init_state);
                    _uwb_reset();
                }
            }
        }

        break;
    }

    return ret;
}

void UWBgetConfigParameters(uwb_config_params_t *config)
{
    require(config, exit);

    config->config_identifiers = s_config_identifiers;
    config->num_config_identifiers = sizeof(s_config_identifiers)/sizeof(s_config_identifiers[0]);
    config->pulse_shape_combos = s_pulse_shape_combos;
    config->num_pulse_shape_combos = sizeof(s_pulse_shape_combos) / sizeof(s_pulse_shape_combos[0]);

    config->channel = UWB_CHANNEL_NUMBER;
    if (UWB_CHANNEL_NUMBER == 5)
    {
        config->channelBitmask |= (1 << 0);
    }
    else if (UWB_CHANNEL_NUMBER == 9)
    {
        config->channelBitmask |= (1 << 1);
    }
    else
    {
        LOG_ERR("Not supporting non 5/9 channel");
    }

    config->configIdentifier     = 1;
    config->pulseShapeCombo      = 0;
    config->syncCodeIndexBitmask = SYNC_CODE_BITMASK(12) | SYNC_CODE_BITMASK(11) |
                                   SYNC_CODE_BITMASK(10) | SYNC_CODE_BITMASK(9);
    config->ranMultiplier        = 1;
    config->hoppingBitmask       = 0x50;
    config->chapsPerSlot         = 4;
    config->slotsPerRound        = 6;
    config->respondersNodes      = 1;
    config->macMode              = 0x40 | 0x01;

exit:
    return;
}

void UWBinitSessionParameters(uwb_config_params_t *config, uwb_session_params_t *params)
{
    require(params, exit);

    memset(params, 0, sizeof(uwb_session_params_t));

    params->configIdentifier     = config->configIdentifier;
    params->channel              = config->channel;
    params->channelBitmask       = config->channelBitmask;
    params->pulseShapeCombo      = config->pulseShapeCombo;
    params->syncCodeIndexBitmask = config->syncCodeIndexBitmask;
    params->ranMultiplier        = config->ranMultiplier;
    params->hoppingBitmask       = config->hoppingBitmask;
    params->chapsPerSlot         = config->chapsPerSlot;
    params->slotsPerRound        = config->slotsPerRound;
    params->respondersNodes      = config->respondersNodes;
    params->macMode              = config->macMode;

    params->stsIndex0       = 0;
    params->uwbTime0        = 0;
    params->syncCodeIndex   = 0;
exit:
    return;
}

int UWBinit(session_state_callback_t inSessionStateCallback,
            const uint8_t inAntennaMode,
            const int inFlopRate,
            const uint8_t inDumpProto,
            const bool inHaveDisplay,
            const int16_t inRSSIoffset[2])
{
    int ret = 0;

    memset(&mUWB, 0, sizeof(mUWB));

    mUWB.session_callback = inSessionStateCallback;

    mUWB.power_offset = 0;
    mUWB.do_OTP_Read_XTAL = true;
    mUWB.do_OTP_Read_Power = true;

    mUWB.antenna_mode = inAntennaMode;
    mUWB.flop_rate = inFlopRate;
    mUWB.invert_antenna = 0x00;
    mUWB.dump_proto = inDumpProto;

    /* note this has to exactly match "other radio" of
     * the ranging session to work, so beware
     */
    mUWB.channel_id = UWB_CHANNEL_NUMBER;
    mUWB.is_responder = true;

    mUWB.do_AoA_Calibration = true;
    mUWB.do_Calibration = true;
    mUWB.do_get_device_info_and_caps = true;

    mUWB.stopOnRangeErrors = 0;

    mUWB.next_session_id = 0xFEEDFACD;

    _uwb_reset();

    mUWB.initialized = true;

    // do an initial session to get OTP values and device info/caps
    // but stop at calibration
    //
    mUWB.one_time_init_request = true;

    ret = UWBrangeInit(inHaveDisplay, inRSSIoffset);
    return ret;
}

