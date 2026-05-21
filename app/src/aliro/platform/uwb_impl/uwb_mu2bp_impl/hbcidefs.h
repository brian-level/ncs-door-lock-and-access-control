
#pragma once

#define UWB_HBCI_TAG "UWB_HBCI"
#define FW_CHUNK_LEN 2048
#define HBCI_HDR_LEN 4
#define MAX_HBCI_LEN (FW_CHUNK_LEN + HBCI_HDR_LEN + 1)
#define HBCI_HDR_LEN_MSB 0x02
#define HBCI_HDR_LEN_LSB 0x03

/* HBCI GENERAL COMMAND SET */
/* CLA definitions */
#define GENERAL_QRY_CLA 0x01
#define GENERAL_ANS_CLA 0x02
#define GENERAL_CMD_CLA 0x03
#define GENERAL_ACK_CLA 0x04

/* QRY INS */
#define QRY_STATUS_INS     0x21
#define QRY_CHIP_ID_INS    0x31
#define QRY_HELIOS_ID_INS  0x32
#define QRY_CA_ROOT_PK_INS 0x33
#define QRY_NXP_PK_INS     0x34

/* ANS INS */
#define ANS_HBCI_READY_INS           0x21
#define ANS_MODE_TEST_OS_READY_INS   0x22
#define ANS_MODE_PATCH_ROM_READY_INS 0x23
#define ANS_MODE_PATCH_HIF_READY_INS 0x24
#define ANS_MODE_PATCH_IM4_READY_INS 0x25

/* CMD INS */
#define CMD_MODE_TEST_OS_INS   0x22
#define CMD_MODE_PATCH_ROM_INS 0x23
#define CMD_MODE_HIF_INS       0x24
#define CMD_MODE_IM4_INS       0x25

/* ACK INS */
#define ACK_VALID_APDU_INS   0x01
#define ACK_LRC_MISMATCH_INS 0x82
#define ACK_INVALID_CLA_INS  0x83
#define ACK_INVALID_INS_INS  0x84
#define ACK_INVALID_LEN_INS  0x85

/* HBCI TESTOS  COMMAND SET */
/* Class defitions */
#define TEST_OS_QRY_CLA 0x11
#define TEST_OS_ANS_CLA 0x12
#define TEST_OS_CMD_CLA 0x13

/* Query INS definitions */
#define TEST_OS_WRITE_STATUS_INS    0x1
#define TEST_OS_AUTH_STATUS_INS     0x2
#define TEST_OS_JTAG2AHB_STATUS_INS 0x3
#define TEST_OS_PAYLOAD_STATUS_INS  0x4
#define TEST_OS_DEVICE_STATUS_INS   0x8
#define TEST_OS_ATTEMPTS_REM_INS    0x9

/* Answer INS definitions */
#define TEST_OS_WRITE_SUCCESS_INS        0x1
#define TEST_OS_AUTH_SUCCESS_INS         0x2
#define TEST_OS_JTAG2AHB_SUCCESS_INS     0x3
#define TEST_OS_PAYLOAD_SUCCESS_INS      0x4
#define TEST_OS_DEVICE_UNLOCKED_INS      0x8
#define TEST_OS_OTP_FULL_INS             0x81
#define TEST_OS_INVALID_PASSWORD_LEN_INS 0x82
#define TEST_OS_AUTH_FAIL_INS            0x83
#define TEST_OS_DEVICE_LOCKED_INS        0x84
#define TEST_OS_JTAG2AHB_FAIL_INS        0x85
#define TEST_OS_PAYLOAD_FAIL_INS         0x86

/* Command INS definitions */
#define TEST_OS_WRITE_PASSWORD_INS   0x1
#define TEST_OS_AUTH_PASSWORD_INS    0x2
#define TEST_OS_ENABLE_JTAG2AHB_INS  0x3
#define TEST_OS_DOWNLOAD_PAYLOAD_INS 0x4

/* HBCI Fw DOWNLOAD COMMAND SET */

/* Class defitions */
#define FW_DWNLD_QRY_CLA 0x51
#define FW_DWNLD_ANS_CLA 0x52
#define FW_DWNLD_CMD_CLA 0x53

/* Query INS definitions */
#define FW_DWNLD_QRY_IMAGE_STATUS 0x1

/* ANS INS defintions */
#define FW_DWNLD_IMAGE_SUCCESS                  0x01
#define FW_DWNLD_HEADER_SUCCESS                 0x04
#define FW_DWNLD_QUICKBOOT_SETTINGS_SUCCESS     0x05
#define FW_DWNLD_HEADER_TOO_LARGE               0x81
#define FW_DWNLD_HEADER_PARSE_ERR               0x82
#define FW_DWNLD_INVALID_CIPHER_TYPE_CRYPTO     0x83
#define FW_DWNLD_INVALID_CIPHER_TYPE_MODE       0x84
#define FW_DWNLD_INVALID_CIPHER_TYPE_HASH       0x85
#define FW_DWNLD_INVALID_CIPHER_TYPE_CURVE      0x86
#define FW_DWNLD_INVALID_ECC_KEY_LENGTH         0x87
#define FW_DWNLD_INVALID_PAYLOAD_DESCRIPTION    0x88
#define FW_DWNLD_INVALID_FW_VERSION             0x89
#define FW_DWNLD_INVALID_ECID_MASK              0x8A
#define FW_DWNLD_INVALID_ECID_VALUE             0x8B
#define FW_DWNLD_INVALID_ENCRYPTED_PAYLOAD_HASH 0x8C
#define FW_DWNLD_INVALID_HEADER_SIGNATURE       0x8D
#define FW_DWNLD_INSTALL_SETTINGS_TOO_LARGE     0x8E
#define FW_DWNLD_PAYLOAD_TOO_LARGE              0x8F
#define FW_DWNLD_SETTINGS_PARSE_ERR             0x90
#define FW_DWNLD_QUICKBOOT_SETTINGS_PARSE_ERR   0x91
#define FW_DWNLD_INVALID_STATIC_HASH            0x92
#define FW_DWNLD_INVALID_DYNAMIC_HASH           0x93
#define FW_DWNLD_EXECUTION_SETTINGS_ERR         0x94
#define FW_DWNLD_KEY_READ_ERR                   0x95

/* CMD INS defintions */
#define FW_DWNLD_DWNLD_IMAGE 0x01

#define SEG_PACKET   0x08
#define FINAL_PACKET 0x00

#define HBCI_HDR(arr, CLA, INS, SEG) \
    {                                \
        arr[0] = CLA;                \
        arr[1] = INS;                \
        arr[3] = ((SEG) << 4);       \
    }

