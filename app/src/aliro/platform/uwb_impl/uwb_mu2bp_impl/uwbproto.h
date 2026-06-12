
#pragma once

#include "UciDefs.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/// max concurrent sessions we'll handle
///
#define UWB_MAX_SESSIONS    (8)

#define UWB_MAX_CONFIG_DATA (128)
#define UWB_MAX_UCI_MESSAGE (264)

#define UWB_CHANNEL_NUMBER              (9)

#define UWB_IOS_SPEC_VERSION_MAJOR      (1)
#define UWB_IOS_SPEC_VERSION_MINOR      (1)

#define UWB_ANDROID_SPEC_VERSION_MAJOR  (1)
#define UWB_ANDROID_SPEC_VERSION_MINOR  (0)

#define NXP_MANUFACTURER_ID    { 0x32, 0x11, 0x10, 0x00 }

#define UWB_CONFIG_ID_1_MASK (uint32_t)(1 << 1)
#define UWB_CONFIG_ID_2_MASK (uint32_t)(1 << 2)
#define UWB_CONFIG_ID_3_MASK (uint32_t)(1 << 3)

#define UWB_CONFIG_ID_1 (uint8_t)(1)

#define UWB_SUPPORTED_PROFILE_IDS (uint32_t)(UWB_CONFIG_ID_1_MASK | UWB_CONFIG_ID_2_MASK | UWB_CONFIG_ID_3_MASK)

#define UWB_DEVICE_CONTROLLER (uint8_t)(1 << 0)
#define UWB_DEVICE_CONTROLEE (uint8_t)(1 << 1)

#define UWB_SUPPORTED_DEVICE_RANGING_ROLES (uint8_t)(UWB_DEVICE_CONTROLLER | UWB_DEVICE_CONTROLEE)

#define UWB_MAX_CHIP_ID_LEN     (16)
#define UWB_MAX_PPM_VALUE_LEN   (1)
#define UWB_MAX_TX_POWER_LEN    (2)

#define UWBIOTVER_STR_PROD_NAME          "UWBIOT"
#define UWBIOTVER_STR_VER_STRING_NUM     "v04.06.00"
#define UWBIOTVER_STR_PROD_NAME_VER_FULL "UWBIOT_v04.06.00"
#define UWBIOTVER_VER_MAJOR             (4)
#define UWBIOTVER_VER_MINOR             (6)
#define UWBIOTVER_VER_DEV               (0)

// Level proprietary

//  Antenna configuration: where are the fixed antennae
//
#define ANTMODE_INVALID                 (0x00)
#define ANTMODE_THREE_FRONT             (0x07)  /* 2BP one H and one V rx antenns */
#define ANTMODE_ONE_FRONT_ONE_BACK      (0x11)  /* one front and one back both for distance */
#define ANTMODE_TWO_FRONT_ONE_BACK      (0x13)  /* 2JE one H and one in back for distance */

// Antennas selected: in the configuration applied, which antennae are activc
//  Note: ANSEL_FRONT implies *all* front antenna are selected
//
#define ANTSEL_FRONT                    (0x00)
#define ANTSEL_BACK                     (0x01)
#define ANTSEL_INVALID                  (0x80)

typedef enum
{
    UWB_IS_INIT,
    UWB_IS_RESET,
    UWB_IS_SET_CONFIG,
    UWB_IS_READ_OTP_XTAL,
    UWB_IS_READ_OTP_TXPOWER,
    UWB_IS_CALIBRATE,
    UWB_IS_READY,
    UWB_IS_INIT_SESSION,
    UWB_IS_APP_CONFIG_SESSION,
    UWB_IS_START_SESSION,
    UWB_IS_ANTENNA_SELECT_SESSION,
    UWB_IS_SUSPEND_SESSION,
    UWB_IS_RESUME_SESSION,
    UWB_IS_STOP_SESSION,
    UWB_IS_DEINIT_SESSION,
    UWB_IS_WAIT_RSP,
    UWB_IS_WAIT_NTF,
}
uwb_init_state_t;

typedef enum
{
    UWB_SS_INACTIVE,        ///< Free
    UWB_SS_CREATED,         ///< Allocated, ready for init-ranging
    UWB_SS_INITIALIZED,     ///< init-ranging or set-profile responded/notified
    UWB_SS_IDLE,            ///< idle
    UWB_SS_STARTING,        ///< app-config complete
    UWB_SS_STARTED,         ///< active
    UWB_SS_SUSPENDING,      ///< suspend requested
    UWB_SS_SUSPENDED,       ///< suspended
    UWB_SS_STOPPING,        ///< stop requested
    UWB_SS_STOPPED,         ///< stopped
}
uwb_session_state_t;

typedef struct
{
    uint8_t uciGenericMajor;
    uint8_t uciGenericMinorMaintenanceVersion;
    uint8_t uciGenericPatch;
    uint8_t macMajorVersion;
    uint8_t macMinorMaintenanceVersion;
    uint8_t phyMajorVersion;
    uint8_t phyMinorMaintenanceVersion;
    uint8_t devNameLen;
    uint8_t devName[48];
    uint8_t fwMajor;
    uint8_t fwMinor;
    uint8_t fwRc;
    uint8_t nxpUciMajor;
    uint8_t nxpUciMinor;
    uint8_t nxpUciPatch;
    uint8_t nxpChipId[UWB_MAX_CHIP_ID_LEN];
    uint8_t maxPpmValue;
    int16_t txPowerValue;
    uint8_t mwMajor;
    uint8_t mwMinor;
    uint8_t mwRc;
    uint8_t uciTestMajor;
    uint8_t uciTestMinor;
    uint8_t uciTestPatch;
    uint8_t fwBootMode;
}
uwb_device_info_t;

typedef struct
{
    uint16_t    *config_identifiers;
    int         num_config_identifiers;
    uint8_t     *pulse_shape_combos;
    int         num_pulse_shape_combos;
    uint16_t    configIdentifier;
    uint8_t     pulseShapeCombo;
    uint8_t     channelBitmask;
    uint32_t    syncCodeIndexBitmask;
    uint8_t     syncCodeIndex;
    uint8_t     ranMultiplier;
    uint8_t     hoppingBitmask;
    uint32_t    hopModeKey;
    uint8_t     chapsPerSlot;
    uint8_t     slotsPerRound;
    uint8_t     slotBitmask;
    uint8_t     respondersNodes;
    uint8_t     macMode;
    uint8_t     channel;
}
uwb_config_params_t;

typedef struct
{
    uint16_t    configIdentifier;
    uint8_t     pulseShapeCombo;
    uint8_t     channelBitmask;
    uint32_t    syncCodeIndexBitmask;
    uint8_t     syncCodeIndex;
    uint8_t     ranMultiplier;
    uint8_t     hoppingBitmask;
    uint32_t    hopModeKey;
    uint8_t     chapsPerSlot;
    uint8_t     slotsPerRound;
    uint8_t     slotBitmask;
    uint8_t     respondersNodes;
    uint8_t     macMode;
    uint32_t    stsIndex0;
    uint32_t    uwbTime0;

    uint8_t     device_role;
    uint8_t     device_type;
    uint8_t     profile_id;
    uint8_t     channel;
    uint8_t     our_mac_addr[2];
    uint8_t     dst_mac_addr[2];
    uint16_t    our_uwb_ver[2];
}
uwb_session_params_t;

typedef struct
{
    uwb_session_state_t session_state;

    uint8_t     uci_session_state;
    const void  *ble_conn_ctx;
    uint32_t    session_id;
    uint32_t    session_handle;
    uint32_t    sts_index;
    uint16_t    range_errors;
    uint8_t     flop_counter;
    uint8_t     current_antenna_sel;
    uint8_t     requested_antenna_sel;
}
uwb_session_t;

typedef int (*session_state_callback_t)(uwb_session_t *session, uint8_t state, uint8_t reason);

static inline uint8_t _UWB_GET_UINT8(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint8_t val = *cursor++;
    *pcursor = cursor;
    return val;
}

static inline uint16_t _UWB_GET_UINT16(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint16_t val = (uint16_t) * cursor++;

    val |= ((uint16_t) * cursor++) << 8;
    *pcursor = cursor;
    return val;
}

static inline uint32_t _UWB_GET_UINT32(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint32_t val = (uint32_t) * cursor++;

    val |= ((uint32_t) * cursor++) << 8;
    val |= ((uint32_t) * cursor++) << 16;
    val |= ((uint32_t) * cursor++) << 24;
    *pcursor = cursor;
    return val;
}

static inline void _UWB_GET_DATA(uint8_t **pcursor, uint8_t *val, const int count)
{
    uint8_t *cursor = *pcursor;
    int i;

    for (i = 0; i < count; i++)
    {
        *val++ = *cursor++;
    }

    *pcursor = cursor;
}

static inline int _UWB_PUT_UINT32(uint8_t **pcursor, int *room, const uint32_t data)
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

    *cursor++ = data & 0xFF;
    *cursor++ = data >> 8;
    *cursor++ = data >> 16;
    *cursor++ = data >> 24;
    *pcursor = cursor;
    *room = *room - sizeof(uint32_t);
    ret = 0;
exit:
    return ret;
}

static inline int _UWB_PUT_UINT16(uint8_t **pcursor, int *room, const uint16_t data)
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

    *cursor++ = data & 0xFF;
    *cursor++ = data >> 8;
    *pcursor = cursor;
    *room = *room - sizeof(uint16_t);
    ret = 0;
exit:
    return ret;
}

static inline int _UWB_PUT_UINT8(uint8_t **pcursor, int *room, const uint8_t data)
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

static inline int _UWB_PUT_DATA(uint8_t **pcursor, int *room, const uint8_t *data, const int len)
{
    uint8_t *cursor = *pcursor;
    int ret = -EINVAL;
    int i;

    if (!(pcursor && *pcursor && room))
    {
        goto exit;
    }

    if (*room < len)
    {
        goto exit;
    }

    for (i = 0; i < len; i++)
    {
        *cursor++ = data[i];
    }

    *pcursor = cursor;
    *room = *room - len;
    ret = 0;
exit:
    return ret;
}

const char *UWBexplainStatus(const uint8_t status);

void UWBgetConfigParameters(uwb_config_params_t *params);
void UWBinitSessionParameters(uwb_config_params_t *config, uwb_session_params_t *inoutParams);

int UWBgetDeviceInfo(uwb_device_info_t **outDevInfo);

int UWBstart(
    const uint8_t inType,
    const uint32_t inSessionId,
    const bool inConfigDataIsProfile,
    const void *inConnectionHandle,
    const uint8_t *inAppConfigData,
    const int inAppConfigDataLength,
    const uint8_t *inVendorConfigData,
    const int inVendorConfigDataLength);

int UWBstopSession(uwb_session_t *inSession, bool inDestroy);
int UWBstopConnection(const void *inConnectionHandle);

bool UWBready(void);

uwb_session_t *UWBsessionFromConnection(const void *inConnectionHandle);

int UWBslice(uint32_t *delay);

int UWBinit(session_state_callback_t inSessionStateCallback,
            const uint8_t inAntennaMode,
            const int inFlopRate,
            const uint8_t inDumpProto,
            const bool inHaveDisplay,
            const int16_t inRSSIoffset[2]);

#ifdef __cplusplus
}
#endif

