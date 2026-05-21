#include "uciproto.h"
#include "ucidefs.h"
#include "uciextdefs.h"
#include "hbciproto.h"
#include "nrfspi.h"
#include "assertmacros.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uciproto);

// Define this non-0 to decode protocol in dump (else raw bytes)
#define DUMP_DECODE_PROTO (0)

// wait this long after reset to start commands, even when
// uwbs stays its ok,  it doesn't really respond to the first command
// (this doesn't seem to help, but keep in in case needed)
//
#define UCI_RESET_DELAY_MS  (10)

// give uwbs this many millisecs to respond before re-trying
//
#define UCI_RESP_TIMEOUT_MS (100)

// after this many timeouts, reset the connnection
//
#define UCI_MAX_TIMEOUTS    (4)

static struct
{
    uint8_t dump_proto;

    enum
    {
        UCI_IDLE,
        UCI_BOOT,
        UCI_INIT,
        UCI_READY,
        UCI_TX,
        UCI_RX
    }
    state, nextstate;

    bool    spi_inited;

    uint64_t cmd_start;
    uint32_t timeout_count;

    int     max_packet;
    uint8_t txbuf[UCI_MSG_HDR_SIZE + UCI_MAX_PAYLOAD_SIZE];
    int     txcnt;
    uint8_t rxbuf[UCI_MAX_PAYLOAD_SIZE];
    int     rxcnt;
}
mUCI;

#if DUMP_DECODE_PROTO
static void _uci_dump(
    const char *inBlurb,
    uint8_t inType,
    uint8_t inGID,
    uint8_t inOID,
    const uint8_t *inData,
    const int inCount)
{
    const char *statestr;
    const char *typestr;
    const char *gidstr;
    const char *oidstr = "????";
    const char *devstatstr = "";

    switch (mUCI.state)
    {
    case UCI_BOOT:
        statestr = "BOOT";
        break;

    case UCI_INIT:
        statestr = "INIT";
        break;

    case UCI_READY:
        statestr = "READY";
        break;

    case UCI_TX:
        statestr = "TX";
        break;

    case UCI_RX:
        statestr = "RX";
        break;

    default:
        statestr = "????";
        break;
    }

    switch (inType)
    {
    case UCI_MT_DATA:
        typestr = "dat";
        break; /* proprietary user of 0 by NxP */

    case UCI_MT_CMD:
        typestr = "cmd";
        break;

    case UCI_MT_RSP:
        typestr = "rsp";
        break;

    case UCI_MT_NTF:
        typestr = "ntf";
        break;

    default:
        typestr = "???";
        break;
    }

    switch (inGID)
    {
    case UCI_GID_CORE:
        gidstr =  "core";

        switch (inOID)
        {
        case UCI_MSG_CORE_DEVICE_RESET:
            oidstr = "reset";
            break;

        case UCI_MSG_CORE_DEVICE_STATUS_NTF:
            if (inCount > 0)
            {
                switch (inData[0])
                {
                case 0:
                    devstatstr = "init (properietary)";
                    break;

                case 1:
                    devstatstr = "ready";
                    break;

                case 2:
                    devstatstr = "active";
                    break;

                case 0xFE:
                    devstatstr = "h/w hang";
                    break;

                case 0xFF:
                    devstatstr = "error";
                    break;

                default:
                    devstatstr = "rsrvd";
                    break;
                }
            }

            oidstr = "status ntf";
            break;

        case UCI_MSG_CORE_DEVICE_INFO:
            oidstr = "info";
            break;

        case UCI_MSG_CORE_GET_CAPS_INFO:
            oidstr = "caps";
            break;

        case UCI_MSG_CORE_SET_CONFIG:
            oidstr = "set cfg";
            break;

        case UCI_MSG_CORE_GET_CONFIG:
            oidstr = "get cfg";
            break;

        case UCI_MSG_CORE_DEVICE_SUSPEND:
            oidstr = "suspend";
            break;

        case UCI_MSG_CORE_GENERIC_ERROR_NTF:
            oidstr = "err nrf";
            break;

        case UCI_MSG_CORE_QUERY_UWBS_TIMESTAMP:
            oidstr = "time";
            break;

        default:
            break;
        }

        break;

    case UCI_GID_SESSION_MANAGE:
        gidstr =  "session cfg";
        break;

    case UCI_GID_RANGE_MANAGE:
        gidstr =  "range mngmt";
        break;

    case UCI_GID_DATA_CONTROL:
        gidstr =  "data ctrl";
        break;

    case UCI_GID_TEST:
        gidstr =  "test";
        break;

    case UCI_GID_PROPRIETARY:
        gidstr =  "proprietary";
        break;

    case UCI_GID_VENDOR:
        gidstr =  "vendor";
        break;

    case UCI_GID_PROPRIETARY_SE:
        gidstr =  "prop se";
        break;

    case UCI_GID_INTERNAL_GROUP:
        gidstr =  "internal";
        break;

    case UCI_GID_INTERNAL:
        gidstr =  "int mw dm";
        break;

    default:
        gidstr = "????";
        break;
    }

    LOG_INF("UCI %s %s %02X %02X %02X with %d bytes:  %s %s %s %s",
            statestr,
            inBlurb, inType, inGID, inOID, inCount,
            typestr, gidstr, oidstr, devstatstr);

    if (inCount)
    {
        LOG_HEXDUMP_INF(inData, inCount, "Data:");
    }
}

#else

static char dump_buf[256 * 4];

static void _uci_dump_raw(
    const uint8_t *inData,
    const int inCount,
    char *outBuf,
    const int outSize)
{
    int index;
    int totlen;
    int len;

    require(inData && outBuf, exit);

    for (index = totlen = 0; index < inCount && totlen < outSize; index++)
    {
        len = snprintf(outBuf + totlen, outSize - totlen, "%02X ", inData[index]);

        if (len > 0)
        {
            totlen += len;
        }
        else
        {
            break;
        }
    }

exit:
    return;
}

#endif

static void uci_decode(
    uint8_t inType,
    uint8_t inGID,
    uint8_t inOID,
    uint8_t *inData,
    const int inCount)
{
    switch (inType)
    {
    case UCI_MT_DATA:
        break;

    case UCI_MT_NTF:
        switch (inGID)
        {
        case UCI_GID_CORE:
            switch (inOID)
            {
            case UCI_MSG_CORE_DEVICE_RESET:
                break;

            case UCI_MSG_CORE_DEVICE_STATUS_NTF:
                if (inCount > 0)
                {
                    if (inData[0] == 0)
                    {
                        // NXP proprietary use of device status 0 as
                        // "init, ready to get a proprietary init sequence"
                        //
                        LOG_INF("UWB Device Status Init, Booting");
                        mUCI.state = UCI_READY;
                    }
                    else if (inData[0] == 1)
                    {
                        LOG_INF("UWB Device Ready");
                        mUCI.state = UCI_READY;
                    }
                    else if (inData[0] == 2)
                    {
                        LOG_INF("UWB Device Active");
                        mUCI.state = UCI_READY;
                    }
                    else if (inData[0] == 0xFE || inData[0] == 0xFF)
                    {
                        LOG_INF("UWB Device Error/Hang, resetting");
                        // TODO - toggle power?
                        mUCI.state = UCI_BOOT;
                    }
                }

                break;

            case UCI_MSG_CORE_GENERIC_ERROR_NTF:
                if (inCount > 0 && inData[0] == 0xA)
                {
                    // this happens a lot normally
                    LOG_DBG("Resend request in state %d", mUCI.state);
                    // repeat last command
                    mUCI.state = UCI_TX;
                }

                break;

            default:
                break;
            }

            break;

        default:
            break;
        }

        break;

    case UCI_MT_CMD:
        break;

    case UCI_MT_RSP:
        mUCI.state = mUCI.nextstate;
        mUCI.timeout_count = 0;
        mUCI.txcnt = 0;
        break;

    default:
        break;
    }
}

static int _UCItxCommand(void)
{
    int ret = -EINVAL;
    int chunk;
    int total;
    int remain;
    uint8_t *header;
    uint8_t *payload;

    // make sure reply is countable
    mUCI.rxcnt = 0;

    header = mUCI.txbuf;
    payload = mUCI.txbuf + UCI_MSG_HDR_SIZE;
    total = 0;
    remain = mUCI.txcnt - UCI_MSG_HDR_SIZE;

    require(remain >= 0, exit);

    // set response timeout time stamp
    mUCI.cmd_start = k_uptime_get();

    // wait for reply (with timeout) in rx state and
    // go back to idle when uwbs responds
    //
    mUCI.state = UCI_RX;
    mUCI.nextstate = UCI_READY;

    if (mUCI.dump_proto)
    {
#if DUMP_DECODE_PROTO
        uint8_t mt;
        uint8_t gid;
        uint8_t oid;

        mt  = (header[0] & UCI_MT_MASK) >> UCI_MT_SHIFT;
        gid = (header[0] & UCI_GID_MASK) >> UCI_GID_SHIFT;
        oid = (header[1] & UCI_OID_MASK) >> UCI_OID_SHIFT;

        _uci_dump("TX->", mt, gid, oid, payload, remain);
#else
        _uci_dump_raw(mUCI.txbuf, mUCI.txcnt, dump_buf, sizeof(dump_buf));
        LOG_PRINTK("NXPUCIX => %s\n", dump_buf);
#endif
    }

    do
    {
        chunk = remain - total;

        header[0] &= ~UCI_PBF_MASK;

        if (chunk > mUCI.max_packet)
        {
            chunk = mUCI.max_packet;
            header[0] |= UCI_PBF_MASK;
        }

        header[3] = chunk;

        // xfer header
        ret = NRFSPIwrite(header, 4);
        require_noerr(ret, exit);

        if (chunk)
        {
            k_sleep(K_USEC(80));

            // xfer chunk
            ret = NRFSPIwrite(payload, chunk);

            total += chunk;
            payload += chunk;
        }
    }
    while (!ret && (total < remain));

exit:
    return ret;
}

int UCIprotoRead(
    uint8_t *outType,
    uint8_t *outGID,
    uint8_t *outOID,
    uint8_t *outData,
    int inSize,
    int *outCount)
{
    int ret = -1;
    uint8_t header[4];
    int payload_length;
    uint8_t frag;
    uint8_t type;
    uint8_t gid;
    uint8_t oid;

    require(outType && outGID && outOID && outCount, exit);
    require(outData, exit);
    require(inSize > 0, exit);

    // set sync line active
    ret = NRFSPIstartSync();

    // read header
    ret = NRFSPIread(header, sizeof(header));
    require_noerr(ret, exit);

    type = (header[0] & UCI_MT_MASK) >> UCI_MT_SHIFT;
    *outType = type;

    gid = (header[0] & UCI_GID_MASK) >> UCI_GID_SHIFT;
    *outGID = gid;

    oid = (header[1] & UCI_OID_MASK) >> UCI_OID_SHIFT;
    *outOID = oid;

    *outCount = 0;

    frag = header[0] & UCI_PBF_MASK;
    payload_length = header[3];

    if (header[1] & 0x80)
    {
        // extended payload - length
        LOG_ERR("Ext payload");
    }

    require(payload_length <= inSize, exit);

    if (payload_length > 0)
    {
        // read payload
        ret = NRFSPIread(outData, payload_length);
        require_noerr(ret, exit);
    }

    *outCount = payload_length;

    if (mUCI.dump_proto)
    {
#if DUMP_DECODE_PROTO
        _uci_dump("<-RX", type, gid, oid, mUCI.rxbuf, count);
#else
        int dl = snprintf(dump_buf, sizeof(dump_buf), "%02X %02X %02X %02X ",
                          header[0], header[1], header[2], header[3]);

        if (payload_length)
        {
            _uci_dump_raw(outData, payload_length, dump_buf + dl, sizeof(dump_buf) - dl);
        }

        LOG_PRINTK("NXPUCIR <= %s\n", dump_buf);
#endif
    }

exit:
    // set sync line inactive
    ret = NRFSPIstopSync();
    return ret;
}

int UCIprotoWriteRaw(
    const uint8_t *inData,
    const int inCount)
{
    int ret = -EINVAL;

    require(mUCI.state == UCI_READY, exit);
    require(inData, exit);
    require(inCount >= UCI_MSG_HDR_SIZE, exit);
    require(inCount <= sizeof(mUCI.txbuf), exit);

    memcpy(mUCI.txbuf, inData, inCount);
    mUCI.txcnt = inCount;

    ret = _UCItxCommand();
exit:
    return ret;
}

int UCIprotoWrite(
    const uint8_t inType,
    const uint8_t inGID,
    const uint8_t inOID,
    const uint8_t *inData,
    const int inCount)
{
    int ret = -EINVAL;
    uint8_t *header;
    uint8_t *payload;

    if (inCount)
    {
        require(inData, exit);
    }
    else
    {
        require(inData == NULL, exit);
    }

    require((inCount + UCI_MSG_HDR_SIZE) < sizeof(mUCI.txbuf), exit);

    header  = mUCI.txbuf;
    payload = mUCI.txbuf + UCI_MSG_HDR_SIZE;

    header[0] = (inType << UCI_MT_SHIFT) | ((inGID << UCI_GID_SHIFT) & UCI_GID_MASK);
    header[1] = (inOID << UCI_OID_SHIFT) & UCI_OID_MASK;
    header[2] = 0; // RFU
    header[3] = inCount & 0xff; // will be filled out in txCommand

    if (inCount)
    {
        memcpy(payload, inData, inCount);
    }

    mUCI.txcnt = UCI_MSG_HDR_SIZE + inCount;

    ret = _UCItxCommand();
exit:
    return ret;
}

int UCIprotoSlice(
    bool *outHaveMessage,
    uint8_t *outType,
    uint8_t *outGID,
    uint8_t *outOID,
    uint8_t **outPayload,
    int *outPayloadLength,
    uint32_t *delay)
{
    int ret = -EINVAL;
    bool readable;
    uint64_t now;

    require(outHaveMessage && outType && outGID && outOID, exit);
    require(outPayload && outPayloadLength && delay, exit);

    *outHaveMessage = false;
    *outType = 0xFF;
    *outGID = 0;
    *outOID = 0;
    *outPayload = NULL;
    *outPayloadLength = 0;

    if (mUCI.state != UCI_BOOT && mUCI.state != UCI_TX)
    {
        ret = NRFSPIpoll(&readable);
        require_noerr(ret, exit);

        if (readable)
        {
            int count;
            uint8_t type;
            uint8_t gid;
            uint8_t oid;

            ret = UCIprotoRead(&type, &gid, &oid, mUCI.rxbuf, sizeof(mUCI.rxbuf), &count);

            if (!ret)
            {
                mUCI.timeout_count = 0;
                mUCI.rxcnt = count;

                // advance our state depending upon response/notification
                //
                uci_decode(type, gid, oid, mUCI.rxbuf, count);

                *outHaveMessage = true;
                *outType = type;
                *outGID = gid;
                *outOID = oid;
                *outPayload = mUCI.rxbuf;
                *outPayloadLength = mUCI.rxcnt;

                // dont ever look at this reply again
                mUCI.rxcnt = 0;
                ret = 0;
            }
        }
    }

    switch (mUCI.state)
    {
    case UCI_IDLE:
        LOG_WRN("why call slice in idle state?");
        break;

    case UCI_BOOT:
        // (re)setup the SPI interface
        ret = NRFSPIinit();
        require_noerr(ret, exit);

        // allow later use of spi.  once its been inited once
        // its usable for the rest of up-time
        //
        mUCI.spi_inited = true;

        // load f/w
        ret = HBCIprotoInit();
        require_noerr(ret, exit);

        // when the f/w load is complete, device will
        // post status ready which moves us to from init state
        //
        mUCI.state = UCI_INIT;
        mUCI.timeout_count = 0;
        break;

    case UCI_INIT:
        // getting status in uci proto gets us to ready
        ret = 0;
        break;

    case UCI_READY:
        ret = 0;
        break;

    case UCI_TX: /* retransmit */
        if (mUCI.txcnt > 0)
        {
            ret = _UCItxCommand();
        }
        else
        {
            LOG_ERR("No command to re-transmit?");
            mUCI.state = UCI_READY;
        }

        *delay = 10;
        break;

    case UCI_RX:
        now = k_uptime_get();

        if ((now - mUCI.cmd_start) > UCI_RESP_TIMEOUT_MS)
        {
            mUCI.timeout_count++;

            if (mUCI.timeout_count > UCI_MAX_TIMEOUTS)
            {
                LOG_WRN("Too many timeouts, resetting");
                mUCI.timeout_count = 0;
                mUCI.state = UCI_BOOT;
            }
            else if (mUCI.txcnt)
            {
                // this happens normally, so not error yet
                LOG_DBG("Resp timeout, retry command");
                mUCI.state = UCI_TX;
            }
            else
            {
                LOG_ERR("Why Rx if no Tx?");
                mUCI.state = UCI_BOOT;
            }
        }

        break;

    default:
        break;
    }

exit:
    return ret;
}

bool UCIready(void)
{
    return mUCI.state == UCI_READY;
}

int UCIprotoDeInit(void)
{
    if (mUCI.spi_inited)
    {
        NRFSPIstopSync();
        NRFSPIenableChip(false);
    }

    mUCI.state = UCI_IDLE;
    return 0;
}

int UCIprotoInit(const uint8_t inDumpProto)
{
    int ret = 0;

    // NOTE: this can/should be callable
    // per-session, not just once

    memset(&mUCI, 0, sizeof(mUCI));

    mUCI.dump_proto = inDumpProto & 0x1;
    mUCI.state = UCI_BOOT;
    mUCI.nextstate = UCI_INIT;
    mUCI.timeout_count = 0;
    mUCI.max_packet = UCI_MAX_PAYLOAD_SIZE;
    mUCI.rxcnt = 0;
    mUCI.txcnt = 0;
    return ret;
}

