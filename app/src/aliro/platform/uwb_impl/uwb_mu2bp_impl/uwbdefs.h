
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

// Multicast mode
//
#define UWB_MultiNodeMode_UniCast    (0)
#define UWB_MultiNodeMode_OnetoMany  (1)
#define UWB_MultiNodeMode_ManytoMany (2)

// Device type
//
#define UWB_DeviceType_Controlee  (0)
#define UWB_DeviceType_Controller (1)

