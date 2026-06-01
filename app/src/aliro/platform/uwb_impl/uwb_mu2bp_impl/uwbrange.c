#include "uwbrange.h"
#include "uwbproto.h"
#include "uwbdefs.h"
#include "assertmacros.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uwbrange);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// define this non-0 if board is mounted horizontally (long edge on table)
// or 0 of mounted vertically (short edge on table)
//
#define UWB_ORIENT_HORIZ    (0)

// define this non-0 to extract extended vendor info
//
#define UWB_RANGE_VENDOR_EXTENSIONS (1)

#define UWB_RSSI_MAX    (-40)
#define UWB_RSSI_MIN    (-100)

static uint64_t s_last_measure;

static int16_t mRSSIoffset[ 2 ];
static bool mHaveDisplay;

static const char *FloatPrint( double inVal, char *inBuf, int inBufSize )
{
    // used since adding doubleing point printf support adds 10k in flash
    //
    int sign = inVal < 0;
    int ival = floor(fabs(inVal));
    int frac = (int)(( double )100.0 * ( double )fabs( inVal ) - ( double )100.0 * ( double )ival);

    snprintf( inBuf, inBufSize, "%s%d.%02d", sign ? "-" : "", ival, frac );
    return inBuf;
}

#ifdef CONFIG_DISPLAY
static void _DisplayRangeText(
            const int session_number,
            const uint8_t antsel,
            int rssi,
            double distance,
            double azimuth,
            double PDoA_1,
            double elevation,
            double PDoA_2,
            int az_mag,
            int el_mag,
            bool inout
            )
#else
static void _DisplayRange(
            const int session_number,
            const uint8_t antsel,
            int rssi,
            double distance,
            double azimuth,
            double PDoA_1,
            double elevation,
            double PDoA_2,
            bool inout
            )
#endif
{
    char text[128];
    char fbufA[16];
    char fbufB[16];
    char fbufC[16];
    uint64_t now;

    now = k_uptime_get();

#if 1 // debug text on cli
    int az_mag = 0;
    int el_mag = 0;

    snprintf( text, sizeof( text ), "%d +%ums %sm ant=%u rssi=%d   az %s   el %s  am=%d em=%d",
                session_number,
                ( uint32_t )( now - s_last_measure ),
                FloatPrint( distance, fbufA, sizeof( fbufA ) ),
                antsel,
                rssi,
                FloatPrint( azimuth, fbufB, sizeof( fbufB ) ),
                FloatPrint( elevation, fbufC, sizeof( fbufC ) ),
                az_mag, el_mag );
    LOG_INF( "%s", text );
#endif
#if 0
    char fbufD[16];
    char fbufE[16];
    int textlen;

    textlen = snprintf( text, sizeof( text ), "%d,%u,%d,%s,%s,%s,%s,%s,%u,%s,%s\r\n",
                        session_number,
                        antsel,
                        rssi,
                        FloatPrint( distance, fbufA, sizeof( fbufA ) ),
                        FloatPrint( azimuth, fbufB, sizeof( fbufB ) ),
                        FloatPrint( PDoA_1, fbufC, sizeof( fbufC ) ),
                        FloatPrint( elevation, fbufD, sizeof( fbufD ) ),
                        FloatPrint( PDoA_2, fbufE, sizeof( fbufE ) ),
                        ( uint32_t )( now - s_last_measure ),
                        ( antsel == ANTSEL_FRONT ) ? "outer" : "inner",
                        ( inout ) ? "inside" : "outside" );

    LOG_RAW( "%s", text );
#endif
    s_last_measure = now;
}

#ifdef CONFIG_DISPLAY
#include "Display.h"
static void _DisplayRange(
            const int session_number,
            const uint8_t antsel,
            int rssi,
            double distance,
            double azimuth,
            double PDoA_1,
            double elevation,
            double PDoA_2,
            bool inout )
{
    char text[128];
    char fbufA[16];
    int width;
    int xoff;
    int yoff;
    int az_mag;
    int az_neg;
    int az_limit;
    int el_mag;
    int el_neg;
    int el_limit;
    int ss_mag;
    int ss_limit;
    int ss_step;
    int ss_off;
    uint64_t now;

    az_limit = DisplayWidth() / 2;
    az_neg = azimuth < 0;
    az_mag = ( int )( ( azimuth * ( double )az_limit ) / ( double )60.0 );

    if ( az_mag < 0 )
    {
        az_mag = -az_mag;
    }

    if ( az_mag > az_limit )
    {
        az_mag = az_limit;
    }

    el_limit = ( DisplayHeight() - 8 ) / 2;
    el_neg = elevation < 0;
    el_mag = ( int )( ( elevation * ( double )el_limit ) / ( double )60.0 );

    if ( el_mag < 0 )
    {
        el_mag = -el_mag;
    }

    if ( el_mag > el_limit )
    {
        el_mag = el_limit;
    }

    _DisplayRangeText( session_number, antsel, rssi, distance, azimuth, PDoA_1, elevation, PDoA_2, az_mag, el_mag, inout );

    if ( !mHaveDisplay )
    {
        return;
    }

    now = k_uptime_get();

    DisplayClear();
    DisplaySetFont( 28 );
    snprintf( text, sizeof( text ), "%s", FloatPrint( distance, fbufA, sizeof( fbufA ) ) );
    width = DisplayTextWidth( text );
    xoff = ( DisplayWidth() - width + 1 ) / 2;
    yoff = 10;
    DisplayText( xoff, yoff, text );

    if ( az_neg )
    {
        DisplayFillRect( az_limit, 0, az_mag, 8 );
    }
    else
    {
        DisplayFillRect( az_limit - az_mag, 0, az_mag, 8 );
    }

    if ( el_neg )
    {
        DisplayFillRect( 0, DisplayHeight() / 2 + 8 - el_mag, 12, el_mag );
    }
    else
    {
        DisplayFillRect( 0, DisplayHeight() / 2 + 8, 12, el_mag );
    }

    if ( rssi < UWB_RSSI_MIN )
    {
        rssi = UWB_RSSI_MIN;
    }
    else if ( rssi > UWB_RSSI_MAX )
    {
        rssi = UWB_RSSI_MAX;
    }

    ss_limit = DisplayHeight();
    ss_mag = ( ( rssi - UWB_RSSI_MIN ) * ss_limit ) / ( UWB_RSSI_MAX - UWB_RSSI_MIN );

    ss_step = 4;
    xoff = DisplayWidth() - 16;
    ss_mag -= ( ss_step / 2 );

    for ( ss_off = ss_limit - ss_step + 1; ss_off >= 8; ss_off -= ss_step )
    {
        if ( ss_mag > 0 )
        {
            DisplayFillRect( xoff, ss_off, 14, ss_step - 1 );
        }
        else
        {
            DisplayRect( xoff, ss_off, 14, ss_step - 1 );
        }

        ss_mag -= ss_step;
    }

    DisplayFlush();

    s_last_measure = now;
}
#endif

int UWBrangeData( const uint8_t inAntennaSel, const uint8_t *inData, const int inCount )
{
    int ret = -EINVAL;
    range_data_t range;
    two_way_range_data_t two_way_data;
    uint8_t *cursor = ( uint8_t * )inData;
    int remaining;
    int measurement;
    int rssi;
    int i;

    double distance;
    double azimuth;
    double elevation;
    double PDoA_1 = 0.0;
    double PDoA_2 = 0.0;

    bool inout;

#if UWB_RANGE_VENDOR_EXTENSIONS
    two_way_range_extension_t ext_two_way_data;
    int measurements_length;
    const uint8_t *extension;
#endif
    require( inData, exit );
    require( inCount >= 27, exit );

    range.sequence                  = _UWB_GET_UINT32( &cursor );
    range.session_id                = _UWB_GET_UINT32( &cursor );
    range.rcr_indication            = _UWB_GET_UINT8( &cursor );
    range.current_ranging_interval  = _UWB_GET_UINT32( &cursor );
    range.ranging_measurement_type  = _UWB_GET_UINT8( &cursor );
    /* reserved = */                  _UWB_GET_UINT8( &cursor );
    range.mac_addr_mode_indicator   = _UWB_GET_UINT8( &cursor );

    for ( i = 0; i < 8; i++ )
    {
        _UWB_GET_UINT8( &cursor ); // reserved
    }

    range.number_of_measurements    = _UWB_GET_UINT8( &cursor );

    LOG_DBG( "Range %02u %08X type=%02u, num=%u",
             range.sequence, range.session_id, range.ranging_measurement_type,
             range.number_of_measurements );

#if UWB_RANGE_VENDOR_EXTENSIONS
    measurements_length = UWB_RANGE_NTF_HEADER_LENGTH + ( UWB_RANGE_DATA_TWO_WAY_LENGTH *
                          range.number_of_measurements );

    memset( &ext_two_way_data, 0, sizeof( ext_two_way_data ) );
    if ( inCount > measurements_length )
    {
        extension = inData + measurements_length;
    }
    else
    {
        extension = NULL;
    }

#endif

    if ( range.ranging_measurement_type == UWB_RANGE_MEASUREMENT_TYPE_ONE_WAY )
    {
        // TODO
        LOG_INF( "Not handling 1-way data yet" );
        ret = -EOPNOTSUPP;
        goto exit;
    }
    else
    {
        for ( measurement = 0; measurement < range.number_of_measurements; measurement++ )
        {
            remaining = inCount - ( cursor - inData );
            memset( two_way_data.mac_addr, 0, sizeof( two_way_data.mac_addr ) );

            if ( range.mac_addr_mode_indicator == UWB_MAC_MODE_2_BYTE )
            {
                require( remaining >= 18, exit );

                for ( i = 0; i < 2; i++ )
                {
                    two_way_data.mac_addr[i] = _UWB_GET_UINT8( &cursor );
                }
            }
            else /* 4 byte mac mode */
            {
                require( remaining >= 24, exit );

                for ( i = 0; i < 8; i++ )
                {
                    two_way_data.mac_addr[i] = _UWB_GET_UINT8( &cursor );
                }
            }

            two_way_data.status = _UWB_GET_UINT8( &cursor );

            if ( two_way_data.status  != 0x00 && two_way_data.status  != 0x1B )
            {
                LOG_WRN( "Range-error [%02X]", two_way_data.status );
                // TODO - decode the actual code? in practice is us usually
                // 0x21, 0x81, or 0x82
                ret = -ENETDOWN;
                goto exit;
            }

            two_way_data.NLoS               = _UWB_GET_UINT8( &cursor );                    // byte 28
            two_way_data.distance           = _UWB_GET_UINT16( &cursor );                   // byte 29
            two_way_data.AoA_azimuth        = ( int16_t )_UWB_GET_UINT16( &cursor );        // byte 31
            two_way_data.AoA_azimuth_fom    = _UWB_GET_UINT8( &cursor );                    // byte 33
            two_way_data.AoA_elevation      = ( int16_t )_UWB_GET_UINT16( &cursor );        // byte 34
            two_way_data.AoA_elevation_fom  = _UWB_GET_UINT8( &cursor );                    // byte 36
            two_way_data.AoA_dst_azimuth        = ( int16_t )_UWB_GET_UINT16( &cursor );    // byte 37
            two_way_data.AoA_dst_azimuth_fom    = _UWB_GET_UINT8( &cursor );                // byte 39
            two_way_data.AoA_dst_elevation      = ( int16_t )_UWB_GET_UINT16( &cursor );    // byte 40
            two_way_data.AoA_dst_elevation_fom  = _UWB_GET_UINT8( &cursor );                // byte 42
            two_way_data.slot_index         = _UWB_GET_UINT8( &cursor );                    // byte 43

            // NXP hides a 1-byte RSSI in the first reserved byte at byte 44
            rssi = ( int )(( double ) -0.5 * ( double )( int )_UWB_GET_UINT8( &cursor ) );    // byte 44

            // apply fudge factor to account for insertion loss of antenna extension e.g.
            rssi += mRSSIoffset[inAntennaSel];

            // reserved bytes
            if ( range.mac_addr_mode_indicator == UWB_MAC_MODE_2_BYTE )
            {
                cursor += ( 12 - 1 );
            }
            else
            {
                cursor += ( 6 - 1 );
            }

            distance = ( double )two_way_data.distance / ( double )100.0;
            // angles are signed in 9.7 format
            azimuth = ( double )( int )two_way_data.AoA_azimuth / ( double )( 1 << 7 );
            elevation = ( double )( int )two_way_data.AoA_elevation / ( double )( 1 << 7 );

#if UWB_RANGE_VENDOR_EXTENSIONS

            if ( extension )
            {
                uint8_t *extcursor;
                volatile uint8_t abyte;
                int remaining = inCount - ( extension - inData );

                extcursor = ( uint8_t* )extension;
                ext_two_way_data.vendor_extension_length     = _UWB_GET_UINT8( &extcursor );    // byte 56 or 50

                do // try
                {
                    if ( remaining < ext_two_way_data.vendor_extension_length )
                    {
                        LOG_WRN( "Truncated vend ext");
                        break;
                    }

                    abyte = _UWB_GET_UINT8( &extcursor );   // nxp specific data type  byte 57
                    if ( abyte != 0 ) // look for "specific data V1 for TWR"
                    {
                        break;
                    }

                    abyte = _UWB_GET_UINT8( &extcursor );   // WiFi coex status byte 58
                    // Note - the spec 2.0.7 has just 2 bytes before rx mode but I see three
                    // and the one extra is not documented.
                    abyte = _UWB_GET_UINT8( &extcursor );   // missing in spec, byte 59
                    abyte = _UWB_GET_UINT8( &extcursor );   // RX mode          byte 60
                    if ( abyte != 0x01 ) // look for AoA mode (1) NOT ToA mode (0)
                    {
                        break;
                    }

                    ext_two_way_data.M_rxant = _UWB_GET_UINT8( &extcursor ); // byte 61
                    if ( ext_two_way_data.M_rxant == 0 || ext_two_way_data.M_rxant > 3 )
                    {
                        break;
                    }

                    // skip over rx antenna pair id list
                    extcursor+= ext_two_way_data.M_rxant;

                    abyte = _UWB_GET_UINT8( &extcursor );   // RX mode   byte 64 ( 61 + M + 1, M is 2 in our case
                    ext_two_way_data.N_rxant = _UWB_GET_UINT8( &extcursor ); // byte 65
                    if ( ext_two_way_data.N_rxant == 0 || ext_two_way_data.N_rxant > 3 )
                    {
                        break;
                    }

                    // skip over rx info for debug
                    extcursor+= ext_two_way_data.N_rxant;

                    // there are M records of { aoa(2), pdoa(2), pdoaI(2) }
                    //
                    extcursor+= 2;  // skip AoA
                    ext_two_way_data.PDoA1 = (int16_t)_UWB_GET_UINT16( &extcursor );
                    extcursor+= 2;  // skip poda index

                    if ( ext_two_way_data.M_rxant > 1 )
                    {
                        extcursor+= 2;  // skip AoA
                        ext_two_way_data.PDoA2 = (int16_t)_UWB_GET_UINT16( &extcursor );
                        extcursor+= 2;  // skip poda index
                    }
                }
                while ( 0 ); // catch

                if ( ext_two_way_data.M_rxant > 0 )
                {
                    PDoA_1 = ( double )( int )ext_two_way_data.PDoA1 / ( double )( 1 << 7 );
                }

                if ( ext_two_way_data.M_rxant > 1 )
                {
                    PDoA_2 = ( double )( int )ext_two_way_data.PDoA2 / ( double )( 1 << 7 );
                }

                extension += ext_two_way_data.vendor_extension_length;
            }
#endif

            inout = false;

#if UWB_ORIENT_HORIZ
            _DisplayRange( range.session_id, inAntennaSel, rssi, distance, elevation, PDoA_2, azimuth, PDoA_1, inout );
#else
            _DisplayRange( range.session_id, inAntennaSel, rssi, distance, azimuth, PDoA_1, elevation, PDoA_2, inout );
#endif
        }
    }

    ret = 0;
exit:
    return ret;
}

int UWBrangeInit(
            const bool inHaveDisplay,
            const int16_t inRSSIoffset[2] )
{
    int ret = 0;

    memcpy( mRSSIoffset, inRSSIoffset, sizeof( mRSSIoffset ) );
    mHaveDisplay = inHaveDisplay;
    return ret;
}

