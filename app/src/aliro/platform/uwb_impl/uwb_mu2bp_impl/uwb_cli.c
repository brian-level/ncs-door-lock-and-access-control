#include "uwb_cli.h"
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
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(uwbcli);

typedef struct
{
    uint16_t    rx_delay;
    uint16_t    pdoa_calib[11];
    uint16_t    pdoa_offset[2];
    uint16_t    aoa_threshold[2];
    int16_t     rssi_offset[2];
}
uwb_cal_data_t;

static struct
{
    int         setting_test;
    uint8_t     rate_req;
    uint8_t     flop_rate;
    uint8_t     invert;
    uint8_t     dump_proto;
    uint8_t     ant_mode;
    uint8_t     antenna_mode;
    bool        haveDisplay;

    session_state_callback_t sessionStateCallback;

    uint8_t     our_mac_addr[2];
    uint16_t    our_uwb_ver[2];
    uint8_t     our_clock_drift[2];
    uint8_t     our_model_id[4];

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


#if 0
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
#endif

int NIrestartUWB(void)
{
    int ret;

    LOG_INF("Initializing UWB: antenna_mode:%02X  floprate:%u",
            mNI.antenna_mode,
            mNI.flop_rate);
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
    ret = UWBinit(mNI.sessionStateCallback,
                  mNI.antenna_mode,
                  mNI.flop_rate,
                  mNI.dump_proto,
                  mNI.haveDisplay,
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

int TimeWaitApplicationEvent(uint32_t inDelay)
{
    int result = 0;

//    result = k_sem_take(&mNI.event, K_MSEC(inDelay));

    // result != 0 if timed-out, which is OK
    return result;
}

void TimeSignalApplicationEvent(void)
{
//    k_sem_give(&mNI.event);
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
    else if (!strcmp(inKey, "proto"))
    {
        inReadCallback(inCallbackArg, &mNI.dump_proto, inLen);
    }
    else if (!strcmp(inKey, "antmode"))
    {
        inReadCallback(inCallbackArg, &mNI.antenna_mode, inLen);
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

//static struct ble_ni_connection *cli_conn;

static int _CmdStart(const struct shell *shell, size_t argc, char **argv)
{
    int ret = -1;
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

    /*
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
    */

    return ret;
}

static int _CmdStop(const struct shell *shell, size_t argc, char **argv)
{
    int ret = UWBstop(NULL);

    return ret;
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
//    int i;

    if (argc > 1)
    {
        sel = (uint8_t) strtoul(*++argv, NULL, 0);
    }

    if (sel)
    {
        sel = ANTSEL_BACK;
    }

    shell_print(shell, "Setting antenna to %s for all sessions", (sel == ANTSEL_BACK) ? "indoor" : "outdoor");
#if 0
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
#endif
    NIrestartUWB();
    return 0;
}

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
                               SHELL_CMD_ARG(rate, NULL,
                                       " Set session rate [0=auto,1=min,2=max] (2)\n",
                                       _CmdRate, 1, 1),
                               SHELL_CMD_ARG(flop, NULL,
                                       " Set front/back ANT switch rate [0=none (front only), N=meaurements per antenna] (0)\n",
                                       _CmdFlop, 1, 1),
                               SHELL_CMD_ARG(invert, NULL,
                                       " Set front/back ANT switch GPIO invert [1=yes, 0=no] (0)\n",
                                       _CmdInvert, 1, 1),
                               SHELL_CMD_ARG(proto, NULL,
                                       " Show UIC protocol. Bitmask [0=no,1=show UIC,2=show states] (0)\n",
                                       _CmdProto, 1, 1),
                               SHELL_CMD_ARG(antmode, NULL,
                                       " Set Antenna configuration [0x7=3front,0x11=1front1back,0x13=2front1back] (0x7)\n",
                                       _CmdAntMode, 1, 1),
                               SHELL_CMD_ARG(antsel, NULL,
                                       " Set Antenna selection for all sessions [0=front,1=back]\n",
                                       _CmdAntSel, 1, 1),
                               SHELL_CMD(calib, &ni_cal_cmds,
                                       " Calibration Commands\n",
                                       NULL),
                               SHELL_SUBCMD_SET_END
                              );

SHELL_CMD_REGISTER(ni, &sub_ni, "Nearby Interaction", NULL);

#endif

int UWBcliInit(session_state_callback_t sessionStateCallback, const bool inHaveDisplay)
{
    int ret = 0;

    memset(&mNI, 0, sizeof(mNI));

//    ret = k_sem_init(&mNI.event, 1, 1);
//    require_noerr(ret, exit);

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

    mNI.sessionStateCallback = sessionStateCallback;
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
            settings_save_one("ni/proto", &mNI.dump_proto, sizeof(mNI.dump_proto));
            settings_save_one("ni/antmode", &mNI.antenna_mode, sizeof(mNI.antenna_mode));

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

