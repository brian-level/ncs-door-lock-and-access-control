
#pragma once

// define this to enable things in nxp headers
//
#define  UWBIOT_UWBD_SR150          (1)

// nearby-interaction app message id
//
// FROM accessory TO controller
//
#define UWBMSG_CONFIG_DATA          (0x01)
#define UWBMSG_DID_START            (0x02)
#define UWBMSG_DID_STOP             (0x03)

// FROM controller TO accessory
//
#define UWBMSG_INITIALIZE_IOS       (0x0A)
#define UWBMSG_INITIALIZE_ANDROID   (0xA5)
#define UWBMSG_CONFIG_AND_START     (0x0B)
#define UWBMSG_STOP                 (0x0C)

// Hack to convert a ble connection to a console
//
#define UWBMSG_HACK_CONSOLE         (0xCC)

// Profile
#define UWB_Profile_1               (0x0B)

#define UWB_PROFILE_iOS_BLOB_SIZE_v1_0  (28)
#define UWB_PROFILE_iOS_BLOB_SIZE_v1_1  (30)

#define UWB_PROFILE_Android_BLOB_SIZE_v1_0   (14)
#define UWB_PROFILE_Android_BLOB_SIZE_v1_1   (14)

// Device Role
//
#define UWB_DeviceRole_Responder      (0)
#define UWB_DeviceRole_Initiator      (1)
#define UWB_DeviceRole_UT_Sync_Anchor (2)
#define UWB_DeviceRole_UT_Anchor      (3)
#define UWB_DeviceRole_UT_Tag         (4)
#define UWB_DeviceRole_Advertiser     (5)
#define UWB_DeviceRole_Observer       (6)
#define UWB_DeviceRole_DlTDoA_Anchor  (7)
#define UWB_DeviceRole_DlTDoA_Tag     (8)

// Session types
//
#define UWBD_RANGING_SESSION                    (0x00)
#define UWBD_RANGING_WITH_INBAND_DATA_TRANSFER   (0x01)
#define UWBD_DATA_TRANSFER                      (0x02)
#define UWBD_RANGING_ONLY_PHASE                 (0x03)
#define UWBD_INBAND_DATA_PHASE                  (0x04)
#define UWBD_RANGING_WITH_DATA_PHASE            (0x05)
#define UWBD_HUS_SESSION                        (0x9F)
#define UWBD_CCC_SESSION                        (0xA0)
#define UWBD_CSA_SESSION                        (0xA2)
#define UWBD_RFTEST                             (0xD0)
#define UWBD_RADAR_TRANSFER                     (0xF0)
#define UWBD_RFU                                (0xFF)

// Notification types
//
#define UWBD_RANGING_DATA                   (0x00)
#define UWBD_DATA_TRANSMIT_NTF              (0x01)
#define UWBD_PER_SEND                       (0x02)
#define UWBD_PER_RCV                        (0x03)
#define UWBD_SR_RX_RCV                      (0x04)
#define UWBD_GENERIC_ERROR_NTF              (0x05)
#define UWBD_DEVICE_RESET                   (0x06)
#define UWBD_RFRAME_DATA                    (0x07)
#define UWBD_DBG_DPD_INFO_NTF               (0x08)
#define UWBD_CIR_PULL_DATA_NTF              (0x09)
#define UWBD_RECOVERY_NTF                   (0x0a)
#define UWBD_SCHEDULER_STATUS_NTF           (0x0b)
#define UWBD_SESSION_DATA                   (0x0c)
#define UWBD_MULTICAST_LIST_NTF             (0x0d)
#define UWBD_OVER_TEMP_REACHED              (0x0e)
#define UWBD_BLINK_DATA_TX_NTF              (0x0f)
#define UWBD_DATA_TRANSFER_PHASE_CONFIG_NTF (0x10)
#define UWBD_ACTION_APP_CLEANUP             (0x11)
#define UWBD_TEST_MODE_LOOP_BACK_NTF        (0x12)
#define UWB_TEST_PHY_LOG_NTF                (0x13)
#define UWB_TEST_EXT_PSDU_LOG_NTF           (0x14)
#define UWBD_TEST_RX_RCV                    (0x15)
#define UWBD_DATA_RECV_NTF                  (0x16)
#define UWBD_CIR_DATA_NTF                   (0x17)
#define UWBD_DATA_LOGGER_NTF                (0x18)
#define UWBD_PSDU_DATA_NTF                  (0x19)
#define UWBD_RANGING_TIMESTAMP_NTF          (0x1a)
#define UWBD_COMMAND_TIMESTAMP_NTF          (0x1b)
#define UWBD_RANGING_CCC_DATA               (0x20)
#define UWBD_INVALID_NTF_EVT                (0xff)

// Multicast mode
//
#define UWB_MultiNodeMode_UniCast    (0)
#define UWB_MultiNodeMode_OnetoMany  (1)
#define UWB_MultiNodeMode_ManytoMany (2)

// Device type
//
#define UWB_DeviceType_Controlee  (0)
#define UWB_DeviceType_Controller (1)
#define UWB_DeviceType_CCC_Controller (0xA0) /* device */
#define UWB_DeviceType_CCC_Controllee (0xA1) /* vehicle */

// Hopping config bitmask
//
#define UWB_CCC_HopMask_Disable     (0x00)  /* No Hopping */
#define UWB_CCC_HopMask_Cont_AES    (0xA0)  /* Continuous Hopping mode with AES-based hopping sequence */
#define UWB_CCC_HopMask_Cont_def    (0xA1)  /* Continuous hopping mode with default hopping sequence */
#define UWB_CCC_HopMask_Adapt_AES   (0xA2)  /* Adaptive hopping mode with AES based hopping sequence. */
#define UWB_CCC_HopMask_Adapt_def   (0xA3)  /* Adaptive Hopping mode with default hopping sequence. */
#define UWB_NXP_HopMask_Adapt       (0xA4)  /* NXP Adaptive Hopping mode. */

// Hopping mode
//
#define UWB_CCC_HopMode_Disable     (0x00)  /* No Hopping */
#define UWB_CCC_HopMode_Cont_AES    (0x05)  /* Continuous Hopping mode with AES-based hopping sequence */
#define UWB_CCC_HopMode_Cont_def    (0x03)  /* Continuous hopping mode with default hopping sequence */
#define UWB_CCC_HopMode_Adapt_AES   (0x04)  /* Adaptive hopping mode with AES based hopping sequence. */
#define UWB_CCC_HopMode_Adapt_def   (0x02)  /* Adaptive Hopping mode with default hopping sequence. */
#define UWB_NXP_HopMode_Adapt       (0x01)  /* ????/ NXP Adaptive Hopping mode. */
