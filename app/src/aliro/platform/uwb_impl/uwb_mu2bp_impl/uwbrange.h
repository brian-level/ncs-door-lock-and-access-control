
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define UWB_RANGE_MEASUREMENT_TYPE_ONE_WAY  (0)
#define UWB_RANGE_MEASUREMENT_TYPE_TWO_WAY  (1)

#define UWB_MAC_MODE_2_BYTE                 (0)
#define UWB_MAC_MODE_8_BYTE                 (1)

#define UWB_RANGE_NTF_HEADER_LENGTH         (25)
#define UWB_RANGE_DATA_TWO_WAY_LENGTH       (31)

typedef struct
{
    uint8_t  mac_addr[8];
    uint8_t  frame_type;
    uint8_t  NLoS;
    uint16_t distance;
    int16_t  AoA_azimuth;
    int8_t   AoA_azimuth_fom;
    int16_t  AoA_elevation;
    int8_t   AoA_elevation_fom;
    uint64_t timestamp;
    uint32_t blink_number;
    uint8_t  dev_specific_info_size;
    uint8_t  blink_payload_size;
}
one_way_range_data_t;

typedef struct
{
    uint8_t  mac_addr[8];
    uint8_t  status;
    uint8_t  NLoS;
    uint16_t distance;
    int16_t  AoA_azimuth;
    int8_t   AoA_azimuth_fom;
    int16_t  AoA_elevation;
    int8_t   AoA_elevation_fom;
    int16_t  AoA_dst_azimuth;
    int8_t   AoA_dst_azimuth_fom;
    int16_t  AoA_dst_elevation;
    int8_t   AoA_dst_elevation_fom;
    uint8_t  slot_index;
}
two_way_range_data_t;

typedef struct
{
    uint8_t  vendor_extension_length;
    uint8_t  M_rxant;
    uint8_t  N_rxant;
    int16_t  PDoA1;
    int16_t  PDoA2;
    int8_t   SNRfirst;
    int8_t   SNRmain;
}
two_way_range_extension_t;

// see FiRa consortium UCI Generic Specification
// modified by see NXP_SR150_UCI_Specification_v1.23
//
typedef struct
{
    uint32_t sequence;
    uint32_t session_id;
    uint8_t  rcr_indication;
    uint32_t current_ranging_interval;
    uint8_t  ranging_measurement_type;
    uint8_t  mac_addr_mode_indicator;
    uint8_t  number_of_measurements;
}
range_data_t;

// see NCP_UCI_CCC_Specification_v1.9.pdf
typedef struct
{
    uint32_t  session_id;
    uint8_t   status;
    uint32_t  sts_index;
    uint16_t  rr_index;
    uint16_t  block_index;
    uint16_t  distance;
    uint8_t   fom_anchor;
    uint8_t   fom_initiator;
    uint8_t   ccm_tag[8];

    int16_t   AoA_azimuth;
    int8_t    AoA_azimuth_fom;
    int16_t   AoA_elevation;
    int8_t    AoA_elevation_fom;

    uint32_t  ant_pair;

    uint8_t   nPDoA;
    uint8_t   nRSSI;
}
ccc_range_data_t;

int UWBcccRangeData(const uint8_t inAntennaSel, const uint8_t *inData, const int inCount);
int UWBrangeData(const uint8_t inAntennaSel, const uint8_t *inData, const int inCount);
int UWBrangeInit(const bool inHaveDisplay, const int16_t inRSSIoffset[2]);

