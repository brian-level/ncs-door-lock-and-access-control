
#include "UwbCanned.h"

/* Adapted from
     MTD-SCP-102-A_DS-TWR_SR150_Unicast_v040600_2.py
*/
// Initialize the UWBD for specific platform variant
const uint8_t UWB_INIT_BOARD_VARIANT[] = {0x2E, 0x00, 0x00, 0x02, 0x73, 0x04};
const uint32_t UWB_INIT_BOARD_VARIANT_SIZE = sizeof(UWB_INIT_BOARD_VARIANT);

// Reset the UWB device
const uint8_t UWB_RESET_DEVICE[] = {0x20, 0x00, 0x00, 0x01, 0x00};
const uint32_t UWB_RESET_DEVICE_SIZE = sizeof(UWB_RESET_DEVICE);

// Get device core info cmd
const uint8_t UWB_CORE_GET_DEVICE_INFO_CMD[] = {0x20, 0x02, 0x00, 0x00};
const uint32_t UWB_CORE_GET_DEVICE_INFO_CMD_SIZE = sizeof(UWB_CORE_GET_DEVICE_INFO_CMD);

// Get device capability info cmd
const uint8_t UWB_CORE_GET_CAPS_INFO_CMD[] = {0x20, 0x03, 0x00, 0x00};
const uint32_t UWB_CORE_GET_CAPS_INFO_CMD_SIZE = sizeof(UWB_CORE_GET_CAPS_INFO_CMD);

// Configure parameters of the UWB device
#if 1
const uint8_t UWB_CORE_SET_CONFIG[] = {0x20, 0x04, 0x00, 0x21,
                                       0x07,                                             // Number of parameters
                                       0x01, 0x01, 0x01,                                 // LOW_POWER_MODE
                                       0xE4, 0x02, 0x01, 0x00,                           // DPD_WAKEUP_SRC
                                       0xE4, 0x03, 0x01, 0x14,                           // WTX_COUNT_CONFIG
                                       0xE4, 0x04, 0x02, 0xF4, 0x01,                     // DPD_ENTRY_TIMEOUT
                                       0xE4, 0x34, 0x02, 0xE8, 0x03,                     // clock present waiting time
                                       0xE4, 0x28, 0x04, 0x2F, 0x2F, 0x2F, 0x00,         // TX_PULSE_SHAPE_CONFIG - 47
                                       0xE4, 0x33, 0x01, 0x01                            // NXP_EXTENDED_NTF_CONFIG
                                      };
#else
const uint8_t UWB_CORE_SET_CONFIG[] = {0x20, 0x04, 0x00, 0x15,
                                       0x05,                                             // Number of parameters
                                       0x01, 0x01, 0x01,                                 // LOW_POWER_MODE
                                       0xE4, 0x02, 0x01, 0x00,                           // DPD_WAKEUP_SRC
                                       0xE4, 0x03, 0x01, 0x14,                           // WTX_COUNT_CONFIG
                                       0xE4, 0x04, 0x02, 0xF4, 0x01,                     // DPD_ENTRY_TIMEOUT
                                       0xE4, 0x33, 0x01, 0x01                            // NXP_EXTENDED_NTF_CONFIG
                                      };
#endif
const uint32_t UWB_CORE_SET_CONFIG_SIZE = sizeof(UWB_CORE_SET_CONFIG);

// Set Antenna define - L-R-V like 2BP module
const uint8_t UWB_CORE_SET_ANTENNAS_DEFINE_2BP[] = {0x20, 0x04, 0x00, 0x30,
                                                    0x03,                                             // Number of parameters
                                                    0xE4, 0x61, 0x06, 0x01,                           // ANTENNA_TX_IDX_DEFINE
                                                    0x01, 0x01, 0x00, 0x00, 0x00,   // ID 0x01 - EF1 = 0 EF2 = X => Tx, Type2BP ANT0
                                                    0xE4, 0x60, 0x13, 0x03,                           // ANTENNA_RX_IDX_DEFINE
                                                    0x01, 0x01, 0x02, 0x00, 0x02, 0x00,   // ID 0x01 - RX1 - EF1 = X EF2 = 1 => Rx1h, Type2BP ANT1
                                                    0x02, 0x01, 0x02, 0x00, 0x00, 0x00,   // ID 0x02 - RX1 - EF1 = X EF2 = 0 => Rx1v, Type2BP ANT2
                                                    0x03, 0x02, 0x01, 0x00, 0x01, 0x00,   // ID 0x03 - RX2 - EF1 = 1 EF2 = X => Rx2, Type2BP ANT0
                                                    0xE4, 0x62, 0x0D, 0x02,                           // ANTENNAS_RX_PAIR_DEFINE
                                                    0x01, 0x02, 0x03, 0x00, 0x00, 0x00,   // ID 0x01 - ANT2(V) ANT1, Type2BP ANT2 & ANT0 (H)
                                                    0x02, 0x01, 0x03, 0x00, 0x00, 0x00    // ID 0x02 - ANT3(H) ANT1, Type2BP ANT1 & ANT0 (V)
                                                   };
const uint32_t UWB_CORE_SET_ANTENNAS_DEFINE_2BP_SIZE = sizeof(UWB_CORE_SET_ANTENNAS_DEFINE_2BP);

// Set Antenna define - L-R-Back like 2JE/Level
const uint8_t UWB_CORE_SET_ANTENNAS_DEFINE_2JE[] = {0x20, 0x04, 0x00, 0x35,
                                                    0x03,                                             // Number of parameters
                                                    0xE4, 0x61, 0x0B, 0x02,                           // ANTENNA_TX_IDX_DEFINE
                                                    0x01, 0x41, 0x00, 0x40, 0x00,   // ID 0x01 - EF1 = 0 EF2 = X, GPIO11 = 1 => Tx - Outdoor R, Type2BP ANT0
                                                    0x02, 0x41, 0x00, 0x00, 0x00,   // ID 0x01 - EF1 = 0 EF2 = X, GPIO11 = 0 => Tx - Indoor, Type2BP ANT0
                                                    0xE4, 0x60, 0x13, 0x03,                           // ANTENNA_RX_IDX_DEFINE
                                                    0x01, 0x01, 0x02, 0x00, 0x00, 0x00,   // ID 0x01 - RX1 - EF1 = X EF2 = 0, GPIO11 = X => Outdoor L, Type2BP ANT2
                                                    0x02, 0x02, 0x41, 0x00, 0x41, 0x00,   // ID 0x03 - RX2 - EF1 = 1 EF2 = X, GPIO11 = 1 => Outdoor R, Type2BP ANT0(o)
                                                    0x03, 0x02, 0x41, 0x00, 0x01, 0x00,   // ID 0x02 - RX2 - EF1 = 1 EF2 = X, GPIO11 = 0 => Indoor, Type2BP ANT0(i)
                                                    0xE4, 0x62, 0x0D, 0x02,                           // ANTENNAS_RX_PAIR_DEFINE
                                                    0x01, 0x01, 0x02, 0x00, 0x00, 0x00,   // ID 0x01 - RX ID 1 & RX ID 3, Outdoor L + Outdoor R, Type 2BP ANT2 & ANT0(o)
                                                    0x02, 0x01, 0x03, 0x00, 0x00, 0x00    // ID 0x02 - RX ID 1 & RX ID 2, Outdoor L + Indoor, Type 2BP ANT2 & ANT0(i)
                                                   };
const uint32_t UWB_CORE_SET_ANTENNAS_DEFINE_2JE_SIZE = sizeof(UWB_CORE_SET_ANTENNAS_DEFINE_2JE);

// Set Antenna define - Front-Back (no azimuth/elevation)
const uint8_t UWB_CORE_SET_ANTENNAS_DEFINE_1F1B[] = {0x20, 0x04, 0x00, 0x1F,
                                                     0x02,                                             // Number of parameters
                                                     0xE4, 0x61, 0x0B, 0x02,                           // ANTENNA_TX_IDX_DEFINE
                                                     0x01, 0x41, 0x00, 0x40, 0x00,   // ID 0x01 - EF1 = 0 EF2 = X, GPIO11 = 1 => Tx - Outdoor, Type2BP ANT0
                                                     0x02, 0x41, 0x00, 0x00, 0x00,   // ID 0x01 - EF1 = 0 EF2 = X, GPIO11 = 0 => Tx - Indoor, Type2BP ANT0
                                                     0xE4, 0x60, 0x0D, 0x02,                           // ANTENNA_RX_IDX_DEFINE
                                                     0x01, 0x02, 0x41, 0x00, 0x41, 0x00,   // ID 0x02 - RX2 - EF1 = 1 EF2 = X, GPIO11 = 1 => Outdoor, Type2BP ANT0
                                                     0x02, 0x02, 0x41, 0x00, 0x01, 0x00,   // ID 0x02 - RX2 - EF1 = 1 EF2 = X, GPIO11 = 0 => Indoor, Type2BP ANT0
                                                    };
const uint32_t UWB_CORE_SET_ANTENNAS_DEFINE_1F1B_SIZE = sizeof(UWB_CORE_SET_ANTENNAS_DEFINE_1F1B);

// Init ranging session
uint8_t UWB_SESSION_INIT_RANGING[] = {0x21, 0x00, 0x00, 0x05, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00, /* 1 byte sess type */ 0x00};
const uint32_t UWB_SESSION_INIT_RANGING_SIZE = sizeof(UWB_SESSION_INIT_RANGING);

// Set Application configurations parameters
// Generic settings
uint8_t UWB_SESSION_SET_APP_CONFIG[] = {0x21, 0x03, 0x00, 0x72, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00,
                                        0x21,                                             // Number of parameters
//   0x00, 0x01, 0x00,                                 // DEVICE_TYPE
                                        0x01, 0x01, 0x02,                                 // RANGING_METHOD
                                        0x02, 0x01, 0x00,                                 // STS_CONFIG
                                        0x03, 0x01, 0x00,                                 // MULTI_NODE_MODE
                                        0x04, 0x01, + /* 1-byte channel_ud */ 0x09,       // CHANNEL_NUMBER
                                        0x05, 0x01, 0x01,                                 // NUMBER_OF_CONTROLEES
//   0x06, 0x02, 0x00, 0x00,                           // DEVICE_MAC_ADDRESS
//   0x07, 0x02, 0x00, 0x00,                           // DST_MAC_ADDRESS
                                        0x08, 0x02, 0x60, 0x09,                           // SLOT_DURATION (2400 rtsu = 2000us)
                                        0x09, 0x04, 0xC8, 0x00, 0x00, 0x00,               // RANGING_INTERVAL (200ms)
                                        0x0A, 0x04, 0x00, 0x00, 0x00, 0x00,               // STS_INDEX
                                        0x0B, 0x01, 0x00,                                 // MAC_FCS_TYPE
                                        0x0C, 0x01, 0x03,                                 // RANGING_ROUND_CONTROL
                                        0x0D, 0x01, 0x01,                                 // AOA_RESULT_REQ
                                        0x0E, 0x01, 0x01,                                 // RNG_DATA_NTF
                                        0x0F, 0x02, 0x00, 0x00,                           // RNG_DATA_NTF_PROXIMITY_NEAR
                                        0x10, 0x02, 0x20, 0x4E,                           // RNG_DATA_NTF_PROXIMITY_FAR
//   0x11, 0x01, 0x00                                  // DEVICE_ROLE
                                        0x12, 0x01, 0x03,                                 // RFRAME_CONFIG
                                        0x13, 0x01, 0x01,                                 // RSSI_REPORTING
                                        0x14, 0x01, 0x0A,                                 // PREAMBLE_CODE_INDEX
                                        0x15, 0x01, 0x02,                                 // SFD_ID
                                        0x16, 0x01, 0x00,                                 // PSDU_DATA_RATE
                                        0x17, 0x01, 0x01,                                 // PREAMBLE_DURATION
                                        0x1A, 0x01, 0x01,                                 // RANGING_TIME_STRUCT
                                        0x1B, 0x01, 0x19,                                 // SLOTS_PER_RR
                                        0xA2, 0x01, 0x01,                                 // RESPONDER_SLOT_INDEX
                                        0x1F, 0x01, 0x00,                                 // PRF_MODE
                                        0x22, 0x01, 0x01,                                 // SCHEDULED_MODE
                                        0x23, 0x01, 0x00,                                 // KEY_ROTATION
                                        0x24, 0x01, 0x00,                                 // KEY_ROTATION_RATE
                                        0x25, 0x01, 0x32,                                 // SESSION_PRIORITY
                                        0x26, 0x01, 0x00,                                 // MAC_ADDRESS_MODE
//  0x27, 0x02, 0x00, 0x00,                           // VENDOR_ID
//  0x28, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // STATIC_STS_IV
                                        0x29, 0x01, 0x01,                                 // NUMBER_OF_STS_SEGMENTS
                                        0x2A, 0x02, 0x00, 0x00,                           // MAX_RR_RETRY
//  0x2B, 0x04, 0x00, 0x00, 0x00, 0x00,               // UWB_INITIATION_TIME
                                        0x2C, 0x01, 0x00,                                 // RANGING_ROUND_HOPPING
//  0x2D, 0x01, 0x00,                                 // BLOCK_STRIDING
//  0x2E, 0x01, 0x00,                                 // RESULT_REPORT_CONFIG
                                        0x2F, 0x01, 0x00                                  // IN_BAND_TERMINATION_ATTEMPT_COUNT
//  0x30, 0x04, 0x00, 0x00, 0x00, 0x00,               // SUB_SESSION_ID
                                       };
const uint32_t UWB_SESSION_SET_APP_CONFIG_SIZE = sizeof(UWB_SESSION_SET_APP_CONFIG);

#if 0
uint8_t UWB_SESSION_SET_XAPP_CONFIG[] = {0x21, 0x03, 0x00, 30, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00,
                                         8,                                               // Number of parameters
                                         0x03, 0x01, 0x00,                                 // MULTI_NODE_MODE
                                         0x26, 0x01, 0x00,                                 // MAC_ADDRESS_MODE
                                         0x22, 0x01, 0x01,                                 // SCHEDULED_MODE
                                         0x01, 0x01, 0x02,                                 // RANGING_METHOD
                                         0x29, 0x01, 0x01,                                 // NUMBER_OF_STS_SEGMENTS
                                         0x2A, 0x02, 0x00, 0x00,                           // MAX_RR_RETRY
                                         0x0D, 0x01, 0x01,                                 // AOA_RESULT_REQ
                                         0x13, 0x01, 0x01,                                 // RSSI_REPORTING
                                        };
#else
// Set Application configurations parameters
// Generic settings
uint8_t UWB_SESSION_SET_XAPP_CONFIG[] = {0x21, 0x03, 0x00, 0x0B, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00,
                                         0x2,                                              // Number of parameters
                                         0x0D, 0x01, 0x01,                                 // AOA_RESULT_REQ
                                         0x13, 0x01, 0x01,                                 // RSSI_REPORTING
                                        };
#endif
const uint32_t UWB_SESSION_SET_XAPP_CONFIG_SIZE = sizeof(UWB_SESSION_SET_XAPP_CONFIG);

uint8_t UWB_SESSION_SET_APP_CONFIG_NXP[] = {0x2F, 0x00, 0x00, 0x31, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00,
                                            0x0D,                                             // Number of parameters
//  0x00, 0x01, 0x01,                           // MAC_PAYLOAD_ENCRYPTION
                                            0x02, 0x02, 0x01, 0x01,                     // ANTENNAS_CONFIGURATION_TX
                                            0x03, 0x04, 0x01, 0x02, 0x01, 0x02,         // ANTENNAS_CONFIGURATION_RX for 3D, Type2BP
//  0x60, 0x02, 0x11, 0x00,                     // CIR_CAPTURE_MODE
//  0x61, 0x01, 0x00,                           // RX_ANTENNA_POLARIZATION_OPTION
                                            0x62, 0x01, 0x03,                           // SESSION_SYNC_ATTEMPTS
                                            0x63, 0x01, 0x03,                           // SESSION_SHED_ATTEMPTS
                                            0x64, 0x01, 0x00,                           // SCHED_STATUS_NTF
                                            0x65, 0x01, 0x00,                           // TX_POWER_DELTA_FCC
                                            0x66, 0x01, 0x00,                           // TEST_KDF_FEATURE
                                            0x67, 0x01, 0x00,                           // TX_POWER_TEMP_COMPENSATION
//  0x68, 0x01, 0x03,                           // WIFI_COEX_MAX_TOLERANCE_COUNT
//  0x69, 0x01, 0x00,                           // ADAPTIVE_HOPPING_THRESHOLD
//  0x6E, 0x01, 0x00,                           // AUTHENTICITY_TAG
//  0x6F, 0x02, 0x1E, 0x14,                     // RX_NBIC_CONFIG
                                            0x70, 0x01, 0x03,                           // MAC_CFG
//  0x71, 0x01, 0x00                            // SESSION_INBAND_DATA_TX_BLOCKS
//  0x72, 0x01, 0x00                            // SESSION_INBAND_DATA_RX_BLOCKS
                                            0x7F, 0x01, 0x01,                           // TX_ADAPTIVE_PAYLOAD_POWER
                                            0x84, 0x01, 0x00,                           // ENABLE_FOV
                                            0x85, 0x02, 0x01, 0x30,                     // AZIMUTH_FIELD_OF_VIEW
                                            0x7F, 0x01, 0x01,                           // TX_ADAPTIVE_PAYLOAD_POWER
                                           };
const uint32_t UWB_SESSION_SET_APP_CONFIG_NXP_SIZE = sizeof(UWB_SESSION_SET_APP_CONFIG_NXP);

const uint8_t UWB_VENDOR_COMMAND[] = {0x2E, 0x02, 0x00, 0x00};
const uint32_t UWB_VENDOR_COMMAND_SIZE = sizeof(UWB_VENDOR_COMMAND);

// Set Application configurations parameters
// Specific settings for Initiator
uint8_t UWB_SESSION_SET_INITIATOR_CONFIG[] = {0x21, 0x03, 0x00, 0x13, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00,
                                              0x04,                                           // Number of parameters
                                              0x00, 0x01, 0x01,                               // DEVICE_TYPE: Controller
                                              0x06, 0x02, 0x11, 0x11,                         // DEVICE_MAC_ADDRESS: 0x1111
                                              0x07, 0x02, 0x22, 0x22,                         // DST_MAC_ADDRESS: 0x2222
                                              0x11, 0x01, 0x01                                // DEVICE_ROLE: Initiator
                                             };
const uint32_t UWB_SESSION_SET_INITIATOR_CONFIG_SIZE = sizeof(UWB_SESSION_SET_INITIATOR_CONFIG);

// Specific settings for Responder
uint8_t UWB_SESSION_SET_RESPONDER_CONFIG[] = {0x21, 0x03, 0x00, 0x13, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00,
                                              0x04,                                           // Number of parameters
                                              0x00, 0x01, 0x00,                               // DEVICE_TYPE: Controlee
                                              0x06, 0x02, 0x22, 0x22,                         // DEVICE_MAC_ADDRESS: 0x2222
                                              0x07, 0x02, 0x11, 0x11,                         // DST_MAC_ADDRESS: 0x1111
                                              0x11, 0x01, 0x00                                // DEVICE_ROLE: Responder
                                             };
const uint32_t UWB_SESSION_SET_RESPONDER_CONFIG_SIZE = sizeof(UWB_SESSION_SET_RESPONDER_CONFIG);

// Set Debug configurations parameters (this is really set-vendor-app-config command)
uint8_t UWB_SESSION_SET_DEBUG_CONFIG[] = {0x2F, 0x00, 0x00, 0x0E, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00,
                                          0x03,                                       // Number of parameters
                                          0x30, 0x01, 0x00,                           // CIR_LOG_NTF
                                          0x31, 0x01, 0x00,                           // PSDU_LOG_NTF
                                          0x7B, 0x01, 0x00,                           // RFRAME_LOG_NTF
                                          // 0x7D, 0x04, 0x34, 0x03, 0x5C, 0x03,       // CIR_CAPTURE_WINDOW (820 - 860)
                                         };
const uint32_t UWB_SESSION_SET_DEBUG_CONFIG_SIZE = sizeof(UWB_SESSION_SET_DEBUG_CONFIG);

// Start UWB ranging session
uint8_t UWB_RANGE_START[] = {0x22, 0x00, 0x00, 0x04, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00 };
const uint32_t UWB_RANGE_START_SIZE = sizeof(UWB_RANGE_START);

// Stop UWB ranging session
uint8_t UWB_RANGE_STOP[] = {0x22, 0x01, 0x00, 0x04, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00 };
const uint32_t UWB_RANGE_STOP_SIZE = sizeof(UWB_RANGE_STOP);

// Deinit UWB session
uint8_t UWB_SESSION_DEINIT[] = {0x21, 0x01, 0x00, 0x04, /* 4-byte session_id: */ 0x00, 0x00, 0x00, 0x00 };
const uint32_t UWB_SESSION_DEINIT_SIZE = sizeof(UWB_SESSION_DEINIT);

uint8_t UWB_SET_CALIBRATION_TX_POWER_CH5[] = {0x2F, 0x21, 0x00, 0x0E,
                                              0x05,                                                   // Channel Number
                                              0x04,                                                   // TX_POWER_PER_ANTENNA
                                              0x0B,                                                   // PAYLOAD LENGHT
                                              0x02,                                                   // Number of entries
                                              0x01, 0x00, 0x00, 0x20, 0x00,                     //     TX ID 0x01, TX_Peak_POWER_IND, TX_RMS_POWER_IND
                                              0x02, 0x00, 0x00, 0x00, 0x00                      //     TX ID 0x02, TX_Peak_POWER_IND, TX_RMS_POWER_IND
                                             };
const uint32_t UWB_SET_CALIBRATION_TX_POWER_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_TX_POWER_CH5);

uint8_t UWB_SET_CALIBRATION_TX_POWER_CH9[] = {0x2F, 0x21, 0x00, 0x0E,
                                              0x09,                                                   // Channel Number
                                              0x04,                                                   // TX_POWER_PER_ANTENNA
                                              0x0B,                                                   // PAYLOAD LENGHT
                                              0x02,                                                   // Number of entries
                                              0x01, 0x04, 0x00, 0x2D, 0x00,                     //     TX ID 0x01, TX_Peak_POWER_IND, TX_RMS_POWER_IND
                                              0x02, 0x00, 0x00, 0x00, 0x00                      //     TX ID 0x02, TX_Peak_POWER_IND, TX_RMS_POWER_IND
                                             };
const uint32_t UWB_SET_CALIBRATION_TX_POWER_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_TX_POWER_CH9);

uint8_t UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5[] = {0x2F, 0x21, 0x00, 0x0A,
                                                           0x05,                                                   // Channel Number
                                                           0x01,                                                   // RF_CLK_ACCURACY_CALIB
                                                           0x07,                                                   // PAYLOAD LENGHT
                                                           0x03,                                                   // Number of entries
                                                           0x12, 0x00,                                       //     CAP1
                                                           0x12, 0x00,                                       //     CAP2
                                                           0x21, 0x00                                        //     GM CURRNT CONTROL
                                                          };
const uint32_t UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5);

uint8_t UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9[] = {0x2F, 0x21, 0x00, 0x0A,
                                                           0x09,                                                   // Channel Number
                                                           0x01,                                                   // RF_CLK_ACCURACY_CALIB
                                                           0x07,                                                   // PAYLOAD LENGHT
                                                           0x03,                                                   // Number of entries
                                                           0x12, 0x00,                                       //     CAP1
                                                           0x12, 0x00,                                       //     CAP2
                                                           0x21, 0x00                                        //     GM CURRNT CONTROL
                                                          };
const uint32_t UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9);

const uint8_t UWB_CORE_SET_PDOA_CALIB_TABLE_DEFINE[] = {0x20, 0x04, 0x00, 0x06,
                                                        0x01,
                                                        0xE4, 0x46, 0x02,
                                                        0x0C,                                 // CALIBRATION STEP SIZE - ALLOWED RANGE : 10° TO 15° (DEFAULT = 12°)
                                                        0x0B                                  // NUMBER OF STEPS - ALLOWED RANGE : 3<=M<=21(TO INCLUDE 0°)(DEFAULT = 11)
                                                       };
const uint32_t UWB_CORE_SET_PDOA_CALIB_TABLE_DEFINE_SIZE = sizeof(UWB_CORE_SET_PDOA_CALIB_TABLE_DEFINE);

const uint8_t UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5[] = {0x2F, 0x21, 0x00, 0x10,
                                                              0x05,                                                   // Channel ID
                                                              0x02,                                                   // RX_ANT_DELAY_CALIB
                                                              0x0D,                                                   // PAYLOAD LENGHT
                                                              0x04,                                                   // Number of parameters
                                                              0x01, 0xE6, 0x3A,                           //     RX ID 0x01 - RX Delay
                                                              0x02, 0xE6, 0x3A,                           //     RX ID 0x02 - RX Delay
                                                              0x03, 0xE6, 0x3A,                           //     RX ID 0x03 - RX Delay
                                                              0x04, 0xE6, 0x3A                            //     RX ID 0x04 - RX Delay
                                                             };
const uint32_t UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5);

const uint16_t UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_2BP  = { 0x3AD0 };
const uint16_t UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_2JE  = { 0x3AD0 };

uint8_t UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9[] = {0x2F, 0x21, 0x00, 0x10,
                                                        0x09,                                                   // Channel ID
                                                        0x02,                                                   // RX_ANT_DELAY_CALIB
                                                        0x0D,                                                   // PAYLOAD LENGHT
                                                        0x04,                                                   // Number of parameters
                                                        0x01, 0xD0, 0x3A,                           //     RX ID 0x01 - RX Delay
                                                        0x02, 0xD0, 0x3A,                           //     RX ID 0x02 - RX Delay
                                                        0x03, 0xD0, 0x3A,                           //     RX ID 0x03 - RX Delay
                                                        0x04, 0xD0, 0x3A                            //     RX ID 0x04 - RX Delay
                                                       };
const uint32_t UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9);
const uint32_t UWB_SET_CALIBRATION_VAR_OFFSET = 8;

// Configure AoA calibration of the UWB device
const uint8_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH5[] = {0x2F, 0x21, 0x00, 0xF7,
                                                                         0x05,                                                   // Channel ID
                                                                         0x62,                                                   // AOA_ANTENNAS_PDOA_CALIB
                                                                         0xF4,                                                   // PAYLOAD LENGHT
                                                                         0x01,                                                   // Number of parameters
                                                                         0x01,                                                   // RX PAIR ID 0x01
                                                                         // Pan  -60,        -48,        -36,        -24,        -12,          0,        +12,        +24,        +36,        +48,        +60,
                                                                         0x80, 0x3E, 0x75, 0x2F, 0x69, 0x1F, 0x76, 0x14, 0x0D, 0x09, 0xA6, 0xFD, 0x8C, 0xF4, 0x71, 0xED, 0x86, 0xE5, 0x5E, 0xDB, 0x7E, 0xD4,
                                                                         0x44, 0x3B, 0x78, 0x32, 0x2B, 0x24, 0xDF, 0x15, 0x5B, 0x09, 0x06, 0xFD, 0x77, 0xF2, 0x42, 0xEA, 0x84, 0xE1, 0xED, 0xD6, 0xE5, 0xD0,
                                                                         0xA4, 0x36, 0x32, 0x31, 0xB4, 0x26, 0xB8, 0x19, 0xAE, 0x0B, 0x60, 0xFE, 0x53, 0xF2, 0xFE, 0xE6, 0x68, 0xDC, 0xFB, 0xD4, 0x4B, 0xCE,
                                                                         0x71, 0x33, 0x2A, 0x2D, 0x01, 0x24, 0x04, 0x18, 0x42, 0x0B, 0x41, 0xFE, 0x3A, 0xF2, 0x1E, 0xE7, 0x91, 0xDE, 0x56, 0xD6, 0xCC, 0xCD,
                                                                         0x48, 0x34, 0x81, 0x2C, 0x3D, 0x24, 0x92, 0x1A, 0x00, 0x0E, 0x42, 0x01, 0x5D, 0xF4, 0x7B, 0xE8, 0x6C, 0xDE, 0x6C, 0xD5, 0xA5, 0xCD,
                                                                         0x9F, 0x35, 0x77, 0x2C, 0xAF, 0x22, 0x1B, 0x18, 0x29, 0x0C, 0x00, 0x00, 0xAA, 0xF5, 0x79, 0xEB, 0x10, 0xE1, 0xD1, 0xD7, 0xC5, 0xD0,
                                                                         0xF9, 0x35, 0x19, 0x2E, 0x9F, 0x25, 0xDB, 0x1A, 0x44, 0x0E, 0x48, 0x02, 0x4A, 0xF6, 0x86, 0xEC, 0xA4, 0xE3, 0x10, 0xDB, 0x4B, 0xD3,
                                                                         0x77, 0x34, 0x50, 0x2A, 0x1A, 0x1E, 0x41, 0x14, 0x27, 0x0B, 0x3F, 0x01, 0xB0, 0xF6, 0x3B, 0xEC, 0x81, 0xE1, 0xBE, 0xD8, 0x35, 0xD3,
                                                                         0x6A, 0x30, 0x68, 0x24, 0xA3, 0x1E, 0xE2, 0x17, 0x88, 0x0A, 0x85, 0x00, 0x95, 0xF8, 0xFD, 0xEF, 0x66, 0xE6, 0xF4, 0xD9, 0xF1, 0xCF,
                                                                         0xC3, 0x2B, 0x13, 0x20, 0xA0, 0x1B, 0x6A, 0x0B, 0x83, 0x00, 0x41, 0xF9, 0x95, 0xF4, 0x57, 0xEE, 0x61, 0xE7, 0x6C, 0xDC, 0x92, 0xD2,
                                                                         0xDB, 0x23, 0x1E, 0x21, 0x63, 0x12, 0xA3, 0x07, 0xD8, 0xFF, 0x7E, 0xF7, 0xE9, 0xF0, 0x47, 0xEB, 0x83, 0xE6, 0xD9, 0xE0, 0xF1, 0xD4
                                                                        };
const uint32_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH5);

const uint8_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH5[] = {0x2F, 0x21, 0x00, 0xF7,
                                                                         0x05,                                                   // Channel ID
                                                                         0x62,                                                   // AOA_ANTENNAS_PDOA_CALIB
                                                                         0xF4,                                                   // PAYLOAD LENGHT
                                                                         0x01,                                                   // Number of parameters
                                                                         0x02,                                                   // RX PAIR ID 0x02
                                                                         // Tilt -60,        -48,        -36,        -24,        -12,          0,        +12,        +24,        +36,        +48,        +60,
                                                                         0x4F, 0xEC, 0x4A, 0xEF, 0x4A, 0xF2, 0xCD, 0xF5, 0x87, 0xFB, 0x79, 0x02, 0x69, 0x08, 0x8C, 0x0C, 0xDE, 0x0C, 0xE6, 0x0A, 0x48, 0x07,
                                                                         0x69, 0xDF, 0x8E, 0xE5, 0xA3, 0xEB, 0x54, 0xF1, 0x2A, 0xF8, 0x0C, 0x01, 0x1B, 0x09, 0xA6, 0x0F, 0x79, 0x11, 0x1C, 0x11, 0xFC, 0x11,
                                                                         0x92, 0xD8, 0xD9, 0xDE, 0xB3, 0xE6, 0xD9, 0xED, 0x7B, 0xF7, 0x7C, 0x01, 0xE5, 0x0B, 0x81, 0x13, 0xF8, 0x19, 0xAD, 0x20, 0xB8, 0x21,
                                                                         0x19, 0xD2, 0x97, 0xD9, 0x0B, 0xE2, 0xDB, 0xEB, 0x6C, 0xF7, 0x6E, 0x02, 0x14, 0x0F, 0x24, 0x17, 0xE7, 0x23, 0x62, 0x2D, 0xF0, 0x2E,
                                                                         0x71, 0xCA, 0x9F, 0xD5, 0x0B, 0xDE, 0x1A, 0xEA, 0xD1, 0xF6, 0x5F, 0x03, 0xC9, 0x10, 0x00, 0x1A, 0xDF, 0x29, 0xA2, 0x34, 0x49, 0x3A,
                                                                         0xC7, 0xC4, 0x1B, 0xD2, 0x8C, 0xDB, 0xF3, 0xE7, 0x16, 0xF6, 0x00, 0x00, 0x06, 0x10, 0x52, 0x1B, 0x94, 0x2A, 0xF2, 0x35, 0xA1, 0x3A,
                                                                         0x24, 0xC4, 0x25, 0xD0, 0xE4, 0xD8, 0xA3, 0xE6, 0xCC, 0xF5, 0x26, 0x03, 0xBB, 0x09, 0x4E, 0x19, 0x38, 0x24, 0x81, 0x31, 0x27, 0x34,
                                                                         0x79, 0xC7, 0x7F, 0xCF, 0x09, 0xD7, 0x5C, 0xE6, 0xF3, 0xF3, 0x44, 0x01, 0x76, 0x07, 0xF1, 0x14, 0x65, 0x1F, 0x4F, 0x2A, 0x54, 0x2D,
                                                                         0xD1, 0xC6, 0x92, 0xCE, 0xC8, 0xD6, 0xED, 0xE7, 0x15, 0xF2, 0x6D, 0xFE, 0x0D, 0x05, 0x6D, 0x0D, 0x78, 0x19, 0x2E, 0x22, 0xD0, 0x26,
                                                                         0xBA, 0xCE, 0x49, 0xD5, 0x1A, 0xDF, 0x80, 0xEA, 0xEE, 0xF0, 0xC8, 0xFB, 0xF1, 0x01, 0xE0, 0x05, 0xC3, 0x0F, 0x3A, 0x1A, 0xB9, 0x22,
                                                                         0x58, 0xE0, 0x87, 0xE4, 0x7C, 0xE8, 0x9C, 0xEE, 0xB2, 0xF3, 0xB6, 0xFB, 0x63, 0xFF, 0xED, 0x00, 0xEE, 0x03, 0xBD, 0x0D, 0xB5, 0x16
                                                                        };
const uint32_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH5);

const uint8_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_2BP[] =
{
    // Pan  -60,        -48,        -36,        -24,        -12,          0,        +12,        +24,        +36,        +48,        +60,
    0x80, 0x49, 0x40, 0x39, 0xD4, 0x30, 0x8C, 0x20, 0xA8, 0x11, 0x18, 0xFE, 0x9B, 0xEE, 0x17, 0xE3, 0xE7, 0xD3, 0x2D, 0xCC, 0x2F, 0xC1,
    0x5A, 0x3E, 0xC1, 0x39, 0x29, 0x2E, 0x7C, 0x1E, 0x4B, 0x0E, 0x45, 0xFD, 0xE2, 0xEC, 0x56, 0xDF, 0x57, 0xD6, 0x53, 0xC8, 0x2C, 0xC3,
    0x0B, 0x3D, 0x05, 0x3A, 0xAB, 0x2C, 0xD4, 0x1E, 0x1D, 0x0E, 0x9C, 0xFC, 0x10, 0xEC, 0xA9, 0xE0, 0x2B, 0xD5, 0x2A, 0xC9, 0x39, 0xC3,
    0xDA, 0x41, 0xB5, 0x33, 0x65, 0x2B, 0x8C, 0x1C, 0xFF, 0x0D, 0x27, 0xFF, 0x93, 0xEE, 0x87, 0xDF, 0x8F, 0xD2, 0xE1, 0xCB, 0x32, 0xBB,
    0x25, 0x4A, 0xAD, 0x37, 0x4C, 0x2B, 0xF3, 0x1D, 0xA3, 0x0D, 0xCB, 0xFC, 0xDB, 0xEC, 0x0F, 0xDF, 0x30, 0xD3, 0x54, 0xC7, 0x8D, 0xBC,
    0x8B, 0x44, 0x9A, 0x39, 0x2C, 0x2B, 0x3D, 0x1D, 0x00, 0x0F, 0x00, 0x00, 0x28, 0xF0, 0x18, 0xE2, 0x0D, 0xD6, 0x0A, 0xCC, 0xC4, 0xC4,
    0x7B, 0x3D, 0x10, 0x35, 0x6C, 0x2B, 0xFE, 0x1E, 0xC8, 0x0F, 0xED, 0x00, 0x4A, 0xF1, 0x4E, 0xE4, 0xC2, 0xD9, 0xFC, 0xD0, 0xF9, 0xC9,
    0x24, 0x41, 0xBE, 0x39, 0x75, 0x2D, 0x98, 0x21, 0x9B, 0x14, 0xA8, 0x04, 0x23, 0xF3, 0x6C, 0xE4, 0x5B, 0xD6, 0x32, 0xCC, 0x6E, 0xC6,
    0x8A, 0x46, 0xBA, 0x38, 0x50, 0x2E, 0xA4, 0x1E, 0x22, 0x0F, 0x22, 0x03, 0xFF, 0xF6, 0x71, 0xEA, 0x5D, 0xDE, 0xB1, 0xCF, 0x21, 0xC1,
    0x18, 0x43, 0x0A, 0x3A, 0x96, 0x2D, 0x6F, 0x27, 0x13, 0x18, 0x29, 0x04, 0x4B, 0xF2, 0x5A, 0xE7, 0x9E, 0xDC, 0x98, 0xD2, 0x6B, 0xC4,
    0x10, 0x43, 0x77, 0x37, 0x25, 0x34, 0x7C, 0x25, 0x47, 0x12, 0xE6, 0x00, 0xCD, 0xEF, 0xEB, 0xDF, 0x3E, 0xD7, 0xEC, 0xCF, 0xBB, 0xC9
};
const uint8_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_2JE[] =
{
    // Pan  -60,        -48,        -36,        -24,        -12,          0,        +12,        +24,        +36,        +48,        +60,
    0xE3, 0xC4, 0xC2, 0xCB, 0xA8, 0xD5, 0xD2, 0xE1, 0xF1, 0xEF, 0x53, 0xFF, 0xC3, 0x0E, 0x43, 0x1D, 0x43, 0x29, 0xE0, 0x32, 0x53, 0x3E,
};

uint8_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9[] = {0x2F, 0x21, 0x00, 0xF7,
                                                                   0x09,                                                   // Channel ID
                                                                   0x62,                                                   // AOA_ANTENNAS_PDOA_CALIB
                                                                   0xF4,                                                   // PAYLOAD LENGTH
                                                                   0x01,                                                   // Number of parameters
                                                                   0x01,                                                   // RX PAIR ID 0x01
                                                                   // Pan  -60,        -48,        -36,        -24,        -12,          0,        +12,        +24,        +36,        +48,        +60,
                                                                   0x80, 0x49, 0x40, 0x39, 0xD4, 0x30, 0x8C, 0x20, 0xA8, 0x11, 0x18, 0xFE, 0x9B, 0xEE, 0x17, 0xE3, 0xE7, 0xD3, 0x2D, 0xCC, 0x2F, 0xC1,
                                                                   0x5A, 0x3E, 0xC1, 0x39, 0x29, 0x2E, 0x7C, 0x1E, 0x4B, 0x0E, 0x45, 0xFD, 0xE2, 0xEC, 0x56, 0xDF, 0x57, 0xD6, 0x53, 0xC8, 0x2C, 0xC3,
                                                                   0x0B, 0x3D, 0x05, 0x3A, 0xAB, 0x2C, 0xD4, 0x1E, 0x1D, 0x0E, 0x9C, 0xFC, 0x10, 0xEC, 0xA9, 0xE0, 0x2B, 0xD5, 0x2A, 0xC9, 0x39, 0xC3,
                                                                   0xDA, 0x41, 0xB5, 0x33, 0x65, 0x2B, 0x8C, 0x1C, 0xFF, 0x0D, 0x27, 0xFF, 0x93, 0xEE, 0x87, 0xDF, 0x8F, 0xD2, 0xE1, 0xCB, 0x32, 0xBB,
                                                                   0x25, 0x4A, 0xAD, 0x37, 0x4C, 0x2B, 0xF3, 0x1D, 0xA3, 0x0D, 0xCB, 0xFC, 0xDB, 0xEC, 0x0F, 0xDF, 0x30, 0xD3, 0x54, 0xC7, 0x8D, 0xBC,
                                                                   0x8B, 0x44, 0x9A, 0x39, 0x2C, 0x2B, 0x3D, 0x1D, 0x00, 0x0F, 0x00, 0x00, 0x28, 0xF0, 0x18, 0xE2, 0x0D, 0xD6, 0x0A, 0xCC, 0xC4, 0xC4,
                                                                   0x7B, 0x3D, 0x10, 0x35, 0x6C, 0x2B, 0xFE, 0x1E, 0xC8, 0x0F, 0xED, 0x00, 0x4A, 0xF1, 0x4E, 0xE4, 0xC2, 0xD9, 0xFC, 0xD0, 0xF9, 0xC9,
                                                                   0x24, 0x41, 0xBE, 0x39, 0x75, 0x2D, 0x98, 0x21, 0x9B, 0x14, 0xA8, 0x04, 0x23, 0xF3, 0x6C, 0xE4, 0x5B, 0xD6, 0x32, 0xCC, 0x6E, 0xC6,
                                                                   0x8A, 0x46, 0xBA, 0x38, 0x50, 0x2E, 0xA4, 0x1E, 0x22, 0x0F, 0x22, 0x03, 0xFF, 0xF6, 0x71, 0xEA, 0x5D, 0xDE, 0xB1, 0xCF, 0x21, 0xC1,
                                                                   0x18, 0x43, 0x0A, 0x3A, 0x96, 0x2D, 0x6F, 0x27, 0x13, 0x18, 0x29, 0x04, 0x4B, 0xF2, 0x5A, 0xE7, 0x9E, 0xDC, 0x98, 0xD2, 0x6B, 0xC4,
                                                                   0x10, 0x43, 0x77, 0x37, 0x25, 0x34, 0x7C, 0x25, 0x47, 0x12, 0xE6, 0x00, 0xCD, 0xEF, 0xEB, 0xDF, 0x3E, 0xD7, 0xEC, 0xCF, 0xBB, 0xC9
                                                                  };
const uint32_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9);

const uint8_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH9[] = {0x2F, 0x21, 0x00, 0xF7,
                                                                         0x09,                                                   // Channel ID
                                                                         0x62,                                                   // AOA_ANTENNAS_PDOA_CALIB
                                                                         0xF4,                                                   // PAYLOAD LENGHT
                                                                         0x01,                                                   // Number of parameters
                                                                         0x02,                                                   // RX PAIR ID 0x02
                                                                         // Tilt -60,        -48,        -36,        -24,        -12,          0,        +12,        +24,        +36,        +48,        +60,
                                                                         0x9F, 0xEC, 0x9E, 0xE3, 0x45, 0xE9, 0xE9, 0xF5, 0x32, 0x09, 0x14, 0x0E, 0xC5, 0x07, 0x44, 0x13, 0x44, 0x27, 0x38, 0x2B, 0xB0, 0x2C,
                                                                         0x17, 0xD9, 0x70, 0xDD, 0x09, 0xEB, 0xB3, 0xF0, 0x3C, 0xFB, 0xF0, 0x09, 0xEA, 0x0A, 0xAB, 0x1C, 0x61, 0x27, 0x2F, 0x2D, 0xFF, 0x39,
                                                                         0x3C, 0xD8, 0xBC, 0xDC, 0xEB, 0xE0, 0x58, 0xF0, 0x24, 0xFB, 0x85, 0x04, 0xD6, 0x0E, 0x74, 0x21, 0xC7, 0x28, 0x55, 0x35, 0x05, 0x43,
                                                                         0x51, 0xCD, 0x7F, 0xD8, 0x21, 0xE2, 0xE1, 0xE7, 0xCE, 0xF9, 0x0A, 0x02, 0x49, 0x12, 0x4F, 0x23, 0xCA, 0x2D, 0x63, 0x3D, 0x7C, 0x4A,
                                                                         0xA0, 0xD4, 0xF8, 0xD5, 0x61, 0xE0, 0xE4, 0xE3, 0xCD, 0xF4, 0x9F, 0x00, 0x0D, 0x14, 0x0C, 0x24, 0xEF, 0x2F, 0x68, 0x42, 0x50, 0x4D,
                                                                         0x03, 0xD1, 0x82, 0xD2, 0x99, 0xDB, 0xAD, 0xE2, 0xC6, 0xF0, 0x00, 0x00, 0xAB, 0x13, 0x99, 0x22, 0x94, 0x30, 0xF7, 0x41, 0x20, 0x4D,
                                                                         0xFF, 0xCD, 0x6C, 0xD0, 0x75, 0xDA, 0x52, 0xE2, 0x67, 0xF0, 0xA8, 0xFE, 0xAD, 0x0F, 0x26, 0x1E, 0x0C, 0x2F, 0x9A, 0x3C, 0x29, 0x48,
                                                                         0xF9, 0xCA, 0xA1, 0xD2, 0x16, 0xDB, 0x19, 0xE3, 0x70, 0xF0, 0x9F, 0xFC, 0x95, 0x0C, 0xCA, 0x19, 0x53, 0x2A, 0x6A, 0x37, 0x53, 0x40,
                                                                         0x49, 0xD1, 0x13, 0xD4, 0x9A, 0xDA, 0xC0, 0xE6, 0x75, 0xEE, 0x2A, 0xFB, 0xA3, 0x0A, 0xC7, 0x11, 0x81, 0x24, 0x34, 0x2D, 0x59, 0x38,
                                                                         0x91, 0xD8, 0x3D, 0xDF, 0x6B, 0xE5, 0xF2, 0xE7, 0xBB, 0xEB, 0xA4, 0xFB, 0x97, 0x07, 0x49, 0x0A, 0xF9, 0x19, 0x6F, 0x24, 0xF8, 0x2C,
                                                                         0xA5, 0xE6, 0xF3, 0xDF, 0xA3, 0xE2, 0x6E, 0xE5, 0xCB, 0xEF, 0x76, 0xFC, 0x52, 0x04, 0x19, 0x06, 0xDA, 0x0A, 0x57, 0x17, 0x6F, 0x21
                                                                        };
const uint32_t UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH9);

const uint8_t UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH5[] = {0x2F, 0x21, 0x00, 0x0A,
                                                             0x05,                                                   // Channel ID
                                                             0x03,                                                   // PDOA_OFFSET_CALIB
                                                             0x07,                                                   // PAYLOAD LENGHT
                                                             0x02,                                                   // Number of parameters
                                                             0x01, 0x26, 0x01,                            // RX_ANTENNA_PAIR:0x01
                                                             0x02, 0xB3, 0x0A                             // RX_ANTENNA_PAIR:0x02
                                                            };
const uint32_t UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH5);

const uint16_t UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_2BP[] =
{
    0xFE40,                            // RX_ANTENNA_PAIR:0x01
    0xFD67                             // RX_ANTENNA_PAIR:0x02
};

const uint16_t UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_2JE[] =
{
    0xB274,                           // RX_ANTENNA_PAIR:0x01
    0x0000                            // RX_ANTENNA_PAIR:0x02
};

uint8_t UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9[] = {0x2F, 0x21, 0x00, 0x0A,
                                                       0x09,                                                   // Channel ID
                                                       0x03,                                                   // PDOA_OFFSET_CALIB
                                                       0x07,                                                   // PAYLOAD LENGHT
                                                       0x02,                                                   // Number of parameters
                                                       0x01, 0x40, 0xFE,                            // RX_ANTENNA_PAIR:0x01
                                                       0x02, 0x67, 0xFD                             // RX_ANTENNA_PAIR:0x02
                                                      };
const uint32_t UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9);

const uint8_t UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH5[] = {0x2F, 0x21, 0x00, 0x0A,
                                                              0x05,                                                   // Channel ID
                                                              0x66,                                                   // AOA_THRESHOLD_PDOA
                                                              0x07,                                                   // PAYLOAD LENGHT
                                                              0x02,                                                   // Number of parameters
                                                              0x01, 0x10, 0x59,                            // RX_ANTENNA_PAIR:0x01
                                                              0x02, 0x61, 0xB3                             // RX_ANTENNA_PAIR:0x02
                                                             };
const uint32_t UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH5);

const uint16_t UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_2BP[] =
{
    0x587E,                            // RX_ANTENNA_PAIR:0x01
    0xA7B4                             // RX_ANTENNA_PAIR:0x02
};

const uint16_t UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_2JE[] =
{
    0x0C73,                            // RX_ANTENNA_PAIR:0x01
    0x5A00                             // RX_ANTENNA_PAIR:0x02
};

uint8_t UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9[] = {0x2F, 0x21, 0x00, 0x0A,
                                                        0x09,                                                   // Channel ID
                                                        0x66,                                                   // AOA_THRESHOLD_PDOA
                                                        0x07,                                                   // PAYLOAD LENGHT
                                                        0x02,                                                   // Number of parameters
                                                        0x01, 0x7E, 0x58,                            // RX_ANTENNA_PAIR:0x01
                                                        0x02, 0xB4, 0xA7                             // RX_ANTENNA_PAIR:0x02
                                                       };
const uint32_t UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9);

const uint8_t UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH5[] = {0x2F, 0x21, 0x00, 0x0A,
                                                                           0x05,                                                   // Channel ID
                                                                           0x65,                                                   // PDOA_MANUFACT_ZERO_OFFSET_CALIB
                                                                           0x07,                                                   // PAYLOAD LENGHT
                                                                           0x02,                                                   // Number of parameters
                                                                           0x01, 0x00, 0x00,                            // RX PAIR ID 0x01 | PDOA_OFFSET 00.00°
                                                                           0x02, 0x00, 0x00                             // RX PAIR ID 0x02 | PDOA_OFFSET 00.00°
                                                                          };
const uint32_t UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH5);

const uint16_t UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9_2BP[] =
{
    0x0000,                            // RX PAIR ID 0x01 | PDOA_OFFSET 00.00°
    0x0000                             // RX PAIR ID 0x02 | PDOA_OFFSET 00.00°
};
const uint16_t UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9_2JE[] =
{
    0x0000,                            // RX PAIR ID 0x01 | PDOA_OFFSET 00.00°
    0x0000                             // RX PAIR ID 0x02 | PDOA_OFFSET 00.00°
};

uint8_t UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9[] = {0x2F, 0x21, 0x00, 0x0A,
                                                                     0x09,                                                   // Channel ID
                                                                     0x65,                                                   // PDOA_MANUFACT_ZERO_OFFSET_CALIB
                                                                     0x07,                                                   // PAYLOAD LENGHT
                                                                     0x02,                                                   // Number of parameters
                                                                     0x01, 0x00, 0x00,                            // RX PAIR ID 0x01 | PDOA_OFFSET 00.00°
                                                                     0x02, 0x00, 0x00                             // RX PAIR ID 0x02 | PDOA_OFFSET 00.00°
                                                                    };
const uint32_t UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9);

const uint8_t UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH5[] = {0x2F, 0x21, 0x00, 0x26,
                                                                 0x05,                                               // Channel ID
                                                                 0x63,                                               // PDOA_MULTIPOINT_CALIB
                                                                 0x23,                                               // Length
                                                                 0x02,                                               // Number of Entries
                                                                 0x01,                                               // RX_ANTENNA_PAIR : 0x01
                                                                 0x80, 0x5C, 0x00, 0x00,                             // azimuth  00°(0x80) elevation -36°(0x5C)
                                                                 0x80, 0xA4, 0x00, 0x00,                             // azimuth  00°(0x80) elevation +36°(0xA4)
                                                                 0x5C, 0x80, 0x00, 0x00,                             // azimuth -36°(0x5C) elevation  00°(0x80)
                                                                 0xA4, 0x80, 0x00, 0x00,                             // azimuth +36°(0xA4) elevation  00°(0x80) PDoA +0° (0x0000)
                                                                 0x02,                                               // RX_ANTENNA_PAIR : 0x02
                                                                 0x80, 0x5C, 0x00, 0x00,                             // azimuth  00°(0x80) elevation -36°(0x5C)
                                                                 0x80, 0xA4, 0x00, 0x00,                             // azimuth  00°(0x80) elevation +36°(0xA4)
                                                                 0x5C, 0x80, 0x00, 0x00,                             // azimuth -36°(0x5C) elevation  00°(0x80)
                                                                 0xA4, 0x80, 0x00, 0x00                              // azimuth +36°(0xA4) elevation  00°(0x80) PDoA +0° (0x0000)
                                                                };
const uint32_t UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH5_SIZE = sizeof(UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH5);

const uint8_t UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH9[] = {0x2F, 0x21, 0x00, 0x26,
                                                                 0x09,                                               // Channel ID
                                                                 0x63,                                               // PDOA_MULTIPOINT_CALIB
                                                                 0x23,                                               // Length
                                                                 0x02,                                               // Number of Entries
                                                                 0x01,                                               // RX_ANTENNA_PAIR : 0x01
                                                                 0x80, 0x5C, 0x00, 0x00,                             // azimuth  00°(0x80) elevation -36°(0x5C)
                                                                 0x80, 0xA4, 0x00, 0x00,                             // azimuth  00°(0x80) elevation +36°(0xA4)
                                                                 0x5C, 0x80, 0x00, 0x00,                             // azimuth -36°(0x5C) elevation  00°(0x80)
                                                                 0xA4, 0x80, 0x00, 0x00,                             // azimuth +36°(0xA4) elevation  00°(0x80) PDoA +0° (0x0000)
                                                                 0x02,                                               // RX_ANTENNA_PAIR : 0x02
                                                                 0x80, 0x5C, 0x00, 0x00,                             // azimuth  00°(0x80) elevation -36°(0x5C)
                                                                 0x80, 0xA4, 0x00, 0x00,                             // azimuth  00°(0x80) elevation +36°(0xA4)
                                                                 0x5C, 0x80, 0x00, 0x00,                             // azimuth -36°(0x5C) elevation  00°(0x80)
                                                                 0xA4, 0x80, 0x00, 0x00                              // azimuth +36°(0xA4) elevation  00°(0x80) PDoA +0° (0x0000)
                                                                };
const uint32_t UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH9_SIZE = sizeof(UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH9);

// To read out calibration values from OTP.
const uint8_t UWB_EXT_READ_CALIB_DATA_XTAL_CAP[] = {0x2A, 0x01, 0x00, 0x03, /* 1-byte channel_id: */ 0x09, 0x01, 0x02};
const uint32_t UWB_EXT_READ_CALIB_DATA_XTAL_CAP_SIZE = sizeof(UWB_EXT_READ_CALIB_DATA_XTAL_CAP);

uint8_t UWB_EXT_READ_CALIB_DATA_TX_POWER[] = {0x2A, 0x01, 0x00, 0x03, /* 1-byte channel_id: */ 0x09, 0x01, 0x01};
const uint32_t UWB_EXT_READ_CALIB_DATA_TX_POWER_SIZE = sizeof(UWB_EXT_READ_CALIB_DATA_TX_POWER);

uint8_t UWB_EXT_READ_TRNG[] = {0x2A, 0x02, 0x00, 0x01, /* 2-byte length to read */ 0x02};
const uint32_t UWB_EXT_READ_TRNG_SIZEE = sizeof(UWB_EXT_READ_TRNG);

