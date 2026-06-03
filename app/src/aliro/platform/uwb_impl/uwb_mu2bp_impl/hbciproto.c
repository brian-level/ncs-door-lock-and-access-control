#include "hbciproto.h"
#include "hbcidefs.h"
#include "ucidefs.h"
#include "nrfSpi.h"
#include "assertmacros.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hbci);

// define this non-0 to debug packet trx
#define DUMP_PACKETS 0

// firmware image
#if 1 // newer Aliro image
#include "H1_IOT.SR150_SMARTHOME_MAINLINE_PROD_FW_EE.40.B2_0a13ad6308fafdc0.h"

#define heliosEncryptedMainlineFwImage  H1_IOT_SR150_SMARTHOME_MAINLINE_PROD_FW_EE_40_B2_0a13ad6308fafdc0_bin
#define heliosEncryptedMainlineFwImageLen H1_IOT_SR150_SMARTHOME_MAINLINE_PROD_FW_EE_40_B2_0a13ad6308fafdc0_bin_len
#else
#include "H1_IOT.SR150_MAINLINE_PROD_FW_46.41.06_0052bbfed983a1f1.h"

#define heliosEncryptedMainlineFwImage  H1_IOT_SR150_MAINLINE_PROD_FW_46_41_06_0052bbfed983a1f1_bin
#define heliosEncryptedMainlineFwImageLen H1_IOT_SR150_MAINLINE_PROD_FW_46_41_06_0052bbfed983a1f1_bin_len
#endif

typedef struct
{
    uint8_t *data;
    uint16_t len;
    uint8_t seg;
    uint8_t crc;
}
hbci_packet_t;

static uint8_t mIObuf[MAX_HBCI_LEN];
static uint8_t mRxHeader[HBCI_HDR_LEN];

static int hbci_check(hbci_packet_t *packet, uint8_t cla, uint8_t ins, uint8_t seg)
{
    bool ok = (packet != NULL)
              && (packet->len > 0)
              && (packet->data[0] == cla)
              && (packet->data[1] == ins)
              && ((packet->data[2] >> 4) == seg);

    if (!ok)
    {
        LOG_ERR("Unexpected CLA/INS/SEG : %02x %02x %02x", packet->data[0], packet->data[1], packet->data[2] >> 4);
    }

    return ok ? 0 : -1;
}

static void hbci_prepare(hbci_packet_t *packet, uint8_t cla, uint8_t ins, uint8_t seg)
{
    packet->data = mIObuf;
    HBCI_HDR(packet->data, cla, ins, seg);
    packet->seg = seg;
    packet->len = HBCI_HDR_LEN;
    packet->crc = 0;
}

static int hbci_add(hbci_packet_t *packet, const uint8_t *payload, uint16_t size)
{
    if (packet->len + size > MAX_HBCI_LEN)
    {
        return -1;
    }

    memcpy(&packet->data[packet->len], payload, size);
    packet->len += size;

    return 0;
}

static int hbci_done(hbci_packet_t *packet)
{
    if (packet->seg == FINAL_PACKET)
    {
        if (packet->len == HBCI_HDR_LEN)
        {
            packet->data[2] =
                packet->data[3] = 0;
        }
        else
        {
            packet->data[2] = (packet->len + 1 - HBCI_HDR_LEN) & 0xFF;
            packet->data[3] |= ((packet->len + 1 - HBCI_HDR_LEN) >> 8) & 0x0F;
        }
    }
    else
    {
        packet->data[2] = 0;
        packet->data[3] &= 0xF0;
    }

    if (packet->len > HBCI_HDR_LEN)
    {
        //Add CRC. Be aware that CRC is not included in the payload size in the packet header
        /// BDD = looks more like a checksum vs crc, but whatever, nobody checks it
        packet->crc = 0;

        for (int i = 0; i < packet->len; i++)
        {
            packet->crc += packet->data[i];
        }

        packet->crc               = (packet->crc ^ 0xFF) + 1;
        packet->data[packet->len] = packet->crc;
        packet->len++;
    }

    return 0;
}

static int hbci_wait_ready(void)
{
    int ret = -1;
    bool readable;
    int timeout = 0;

    do
    {
        ret = NRFSPIpoll(&readable);

        if (ret)
        {
            break;
        }

        if (readable)
        {
            ret = 0;
            break;
        }

        if (timeout < 100)
        {
            k_sleep(K_USEC(250));
        }
        else
        {
            k_sleep(K_MSEC(1));
        }
    }
    while (timeout++ < 500);

    if (timeout >= 500)
    {
        LOG_ERR("HBCI timeout");
        ret = -ETIMEDOUT;
    }
    else if (ret)
    {
        LOG_ERR("HBCI error");
    }

    return ret;
}

static int hbci_transceive(hbci_packet_t *snd, int sndBegin, int sndLen, hbci_packet_t *rcv)
{
    int ret;
    uint32_t paylen;

    ret = NRFSPIwrite(&snd->data[sndBegin], sndLen);
    require_noerr(ret, exit);

#if DUMP_PACKETS
    LOG_HEXDUMP_INF(snd->data + sndBegin, sndLen > 4 ? 4 : sndLen, "TX->");
#endif
    ret = hbci_wait_ready();
    require_noerr(ret, exit);

    ret = NRFSPIread(mRxHeader, HBCI_HDR_LEN);
    require_noerr(ret, exit);

    rcv->data = mRxHeader;

    paylen = ((uint32_t)mRxHeader[HBCI_HDR_LEN_MSB] << 8) | ((uint32_t)mRxHeader[HBCI_HDR_LEN_LSB]);
    require((paylen + HBCI_HDR_LEN + 1) <= MAX_HBCI_LEN, exit);

    if (paylen > 0)
    {
        // note this shares mIObuf with the send-data which should have already
        // been all sent if we are reading more than a header response
        rcv->data = mIObuf;
        memcpy(rcv->data, mRxHeader, HBCI_HDR_LEN);

        ret = NRFSPIread(rcv->data + HBCI_HDR_LEN, paylen + 1);
        require_noerr(ret, exit);
    }

    rcv->len = paylen + HBCI_HDR_LEN;
#if DUMP_PACKETS
    LOG_HEXDUMP_INF(rcv->data, rcv->len, "<-RX");
#endif

exit:

    if (ret)
    {
        rcv->len  = 0;
        rcv->data = 0;
    }

    return ret;
}

static int hbci_transceive_hdr(hbci_packet_t *snd, hbci_packet_t *rcv)
{
    return hbci_transceive(snd, 0, HBCI_HDR_LEN, rcv);
}

static int hbci_transceive_payload(hbci_packet_t *snd, hbci_packet_t *rcv)
{
    return hbci_transceive(snd, HBCI_HDR_LEN, snd->len - HBCI_HDR_LEN, rcv);
}

static int _HbciEncryptedFwDownload(void)
{
    hbci_packet_t snd;
    hbci_packet_t rcv;
    int fwSize;
    int total;
    int ret = -1;
    uint8_t mtype;
    uint8_t gid;
    uint8_t oid;

    // Probe the device with the first query to see if its already running
    // the f/w we load and is in UCI mode
    //
    // HBCI QUERY
    hbci_prepare(&snd, GENERAL_QRY_CLA, QRY_STATUS_INS, FINAL_PACKET);
    hbci_done(&snd);
    hbci_transceive_hdr(&snd, &rcv);

    LOG_HEXDUMP_DBG(rcv.data, 4, "Probe");

    if (rcv.data[0] != GENERAL_ANS_CLA || rcv.data[1] != ANS_HBCI_READY_INS)
    {
        // not hbci reply see if its uci
        //
        hbci_prepare(&snd, GENERAL_QRY_CLA, QRY_STATUS_INS, FINAL_PACKET);
        hbci_done(&snd);

        NRFSPIstartSync();
        hbci_transceive_hdr(&snd, &rcv);
        NRFSPIstopSync();

        mtype   = rcv.data[0] >> UCI_MT_SHIFT;
        gid     = rcv.data[0] & UCI_GID_MASK;
        oid     = rcv.data[1] & UCI_OID_MASK;

        if (mtype == UCI_MT_DATA || mtype == UCI_MT_NTF)
        {
            if (gid == UCI_GID_CORE && oid == UCI_MSG_CORE_GENERIC_ERROR_NTF)
            {
                // if the response is "0x60 0x07 ..." that is an UCI error status, so
                // we assume our f/w is already running and return ok
                //
                LOG_INF("UWB f/w apparently running already");
                ret = 0;
                goto exit;
            }
        }
    }

    if (hbci_check(&rcv, GENERAL_ANS_CLA, ANS_HBCI_READY_INS, FINAL_PACKET))
    {
        LOG_ERR("Wrong response to [GENERAL_QRY_CLA, QRY_STATUS_INS]");
        goto exit;
    }

    // HIF MODE
    hbci_prepare(&snd, GENERAL_CMD_CLA, CMD_MODE_HIF_INS, FINAL_PACKET);
    hbci_done(&snd);
    hbci_transceive_hdr(&snd, &rcv);

    if (hbci_check(&rcv, GENERAL_ACK_CLA, ACK_VALID_APDU_INS, FINAL_PACKET))
    {
        LOG_ERR("Wrong response to [GENERAL_CMD_CLA, CMD_MODE_HIF_INS]");
        goto exit;
    }

    // HIF MODE STATUS QUERY
    hbci_prepare(&snd, GENERAL_QRY_CLA, QRY_STATUS_INS, FINAL_PACKET);
    hbci_done(&snd);
    hbci_transceive_hdr(&snd, &rcv);

    if (hbci_check(&rcv, GENERAL_ANS_CLA, ANS_MODE_PATCH_HIF_READY_INS, FINAL_PACKET))
    {
        LOG_ERR("Wrong response to [GENERAL_QRY_CLA, QRY_STATUS_INS]");
        goto exit;
    }

    // Download FW
    fwSize = heliosEncryptedMainlineFwImageLen;

    if (fwSize == 0 || fwSize < 0)
    {
        LOG_ERR("Invalid fw image size");
        goto exit;
    }

    total = 0;

    while (total < fwSize)
    {
        uint8_t seg;
        int chunkLen;

        //LOG_INF("FW Image %d/%d", total, fwSize);

        chunkLen = fwSize - total;

        if (chunkLen > FW_CHUNK_LEN)
        {
            seg      = SEG_PACKET;
            chunkLen = FW_CHUNK_LEN;
        }
        else
        {
            seg      = FINAL_PACKET;
        }

        hbci_prepare(&snd, FW_DWNLD_CMD_CLA, FW_DWNLD_DWNLD_IMAGE, seg);

        if (hbci_add(&snd, &heliosEncryptedMainlineFwImage[total], chunkLen))
        {
            LOG_ERR("Error adding payload to packet");
            goto exit;
        }

        hbci_done(&snd);
        hbci_transceive_hdr(&snd, &rcv);

        /*FW download stuck here if logs are disable so adding some delay*/
        k_sleep(K_USEC(10));

        if (hbci_check(&rcv, GENERAL_ACK_CLA, ACK_VALID_APDU_INS, FINAL_PACKET))
        {
            LOG_ERR("Wrong response to [FW_DWNLD_CMD_CLA, FW_DWNLD_DWNLD_IMAGE]");
            goto exit;
        }

        k_sleep(K_USEC(10));     // WAR for B2

        hbci_transceive_payload(&snd, &rcv);

        if (hbci_check(&rcv, GENERAL_ACK_CLA, ACK_VALID_APDU_INS, FINAL_PACKET))
        {
            // Wrong packet header
            LOG_ERR("Wrong response to payload");
            goto exit;
        }

        total += chunkLen;
    }

    // hack, wait for chip to flash this f/w
    k_sleep(K_MSEC(60));

    // HBCI QUERY
    hbci_prepare(&snd, FW_DWNLD_QRY_CLA, FW_DWNLD_QRY_IMAGE_STATUS, FINAL_PACKET);
    hbci_done(&snd);
    hbci_transceive_hdr(&snd, &rcv);

    if (hbci_check(&rcv, FW_DWNLD_ANS_CLA, FW_DWNLD_IMAGE_SUCCESS, FINAL_PACKET))
    {
        LOG_ERR("Wrong response to [FW_DWNLD_QRY_CLA, FW_DWNLD_QRY_IMAGE_STATUS]");
        goto exit;
    }

    LOG_INF("HELIOS FW download completed");
    ret = 0;
exit:
    return ret;
}

int HBCIprotoInit()
{
    int ret = -EINVAL;

    // enable device
    ret = NRFSPIenableChip(true);
    require_noerr(ret, exit);

    // note that the module takes about 9ms to auto-load boot-loader and be online
    // TODO - move this wait to the app layer?
    k_sleep(K_MSEC(10));

    ret = _HbciEncryptedFwDownload();
exit:
    return ret;
}


