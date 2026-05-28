
#pragma once

// Protocol type and message ID in Aliro message  (Table 11-11)
//
#define ALIRO_PROTO_TYPE_AP             (0)
#define ALIRO_PROTO_TYPE_UWB            (1)
#define ALIRO_PROTO_TYPE_NOTIFICATION   (2)
#define ALIRO_PROTO_TYPE_SUPPLEMENTARY  (3)
#define ALIRO_PROTO_TYPE_THIRD_PARTY    (4)

// AP messages
//
#define ALIRO_PT_AP_RQ                  (0)
#define ALIRO_PT_AP_RS                  (1)

// UWB Ranging Service messages
//
#define ALIRO_PT_UWB_SSM1               (0)
#define ALIRO_PT_UWB_SSM2               (1)
#define ALIRO_PT_UWB_SSM3               (2)
#define ALIRO_PT_UWB_SSM4               (3)
#define ALIRO_PT_UWB_SUSPEND_REQ        (4)
#define ALIRO_PT_UWB_SUSPEND_RSP        (5)
#define ALIRO_PT_UWB_RESUME_REQ         (6)
#define ALIRO_PT_UWB_RESUME_RSP         (7)

// Notification messages
//
#define ALIRO_PT_NOTIFICATION_EVENT                 (0)
#define ALIRO_PT_NOTIFICATION_RANGING               (1)
#define ALIRO_PT_NOTIFICATION_RDR_STATUS_CHANGE     (2)
#define ALIRO_PT_NOTIFICATION_RDR_ACCESS_PROT_DONE  (3)
#define ALIRO_PT_NOTIFICATION_RDR_RKE_REQ           (4)
#define ALIRO_PT_NOTIFICATION_IAP                   (5)
#define ALIRO_PT_NOTIFICATION_IAP_RKE               (6)

// Supplementary service messages
//
#define ALIRO_PT_SUPPLEMENTART_TIME_SYNC    (0)

// 3rd Party messages
//
#define ALIRO_THIRD_PARTY_PASSTHROUGH       (0)


// UWB Attribute IDs  (Table 11-13)
//
#define ALIRO_ATTR_UWB_CONFIG_ID            (0)
#define ALIRO_ATTR_UWB_PULSE_SHAPE_COMBO    (1)
#define ALIRO_ATTR_UWB_SESSION_ID           (2)
#define ALIRO_ATTR_UWB_CHANNEL_BITMASK      (3)
#define ALIRO_ATTR_UWB_RAN_MULTIPLIER       (4)
#define ALIRO_ATTR_UWB_SLOT_BITMASK         (5)
#define ALIRO_ATTR_UWB_SYNC_CODE_INDEX_MASK (6)
#define ALIRO_ATTR_UWB_SYNC_CODE_INDEX      (7)
#define ALIRO_ATTR_UWB_HOP_CONFIG_BITMASK   (8)
#define ALIRO_ATTR_UWB_CHAPS_PER_SLOT       (9)
#define ALIRO_ATTR_UWB_NUM_RESPONDER_NODES  (10)
#define ALIRO_ATTR_UWB_SLOTS_PER_ROUND      (11)
#define ALIRO_ATTR_UWB_STS_INDEX_0          (12)
#define ALIRO_ATTR_UWB_TIME_0               (13)
#define ALIRO_ATTR_UWB_HOP_KEY_MODE         (14)
#define ALIRO_ATTR_UWB_MAC_MODE             (15)
#define ALIRO_ATTR_UWB_VENDOR_SPECIFIC      (16)
#define ALIRO_ATTR_UWB_STATUS               (17)

// Notification-Event Attribute IDs (Table 11-15)
//
#define ALIRO_ATTR_NFT_EVENT_BUSY           (0)
#define ALIRO_ATTR_NTF_EVENT_GENERAL_ERROR  (1)
#define ALIRO_ATTR_NTF_EVENT_RDR_DESC       (2)

// Notification-Event General Error Attribute IDs (Table 11-16)
//
#define ALIRO_ATTR_NTF_EVENT_GENERAL_ERROR_UNKNOWN  (0)
#define ALIRO_ATTR_NTF_EVENT_GENERAL_ERROR_UNAVAIL  (1)
#define ALIRO_ATTR_NTF_EVENT_GENERAL_ERROR_PARAM    (2)
#define ALIRO_ATTR_NTF_EVENT_GENERAL_ERROR_NO_URSK  (3)

// Notification-Ranging Attribute IDs (Table 11-18)
//
#define ALIRO_ATTR_NTF_RANGING_INITATE              (0)
#define ALIRO_ATTR_NTF_RANGING_RESUME               (1)
#define ALIRO_ATTR_NTF_RANGING_SETUP_LATER          (2)
#define ALIRO_ATTR_NTF_RANGING_RESUME_LATER         (3)
#define ALIRO_ATTR_NTF_RANGING_SECURE_FAILED        (4)
#define ALIRO_ATTR_NTF_RANGING_SUSPENDED            (5)

// Notification-Reader-Status-Changed Attribute IDs (Table 11-19)
//
#define ALIRO_ATTR_NTF_RDR_STATUS_CHANGE_STATE      (0)


