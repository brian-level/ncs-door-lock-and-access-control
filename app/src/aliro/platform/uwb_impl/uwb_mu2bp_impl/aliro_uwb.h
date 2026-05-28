
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

static inline uint8_t _ALIRO_GET_UINT8(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint8_t val = *cursor++;
    *pcursor = cursor;
    return val;
}

static inline uint16_t _ALIRO_GET_UINT16(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint16_t val = (uint16_t)*cursor++;

    val <<= 8;
    val |= (uint16_t)*cursor++;
    *pcursor = cursor;
    return val;
}

static inline uint32_t _ALIRO_GET_UINT32(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint32_t val = (uint32_t) *cursor++;

    val <<= 8;
    val |= (uint32_t)*cursor++;
    val <<= 8;
    val |= (uint32_t)*cursor++;
    val <<= 8;
    val |= (uint32_t)*cursor++;
    *pcursor = cursor;
    return val;
}

static inline void _ALIRO_GET_DATA(uint8_t **pcursor, uint8_t *val, const int count)
{
    uint8_t *cursor = *pcursor;
    int i;

    for (i = 0; i < count; i++)
    {
        *val++ = *cursor++;
    }

    *pcursor = cursor;
}

static inline int _ALIRO_PUT_UINT32(uint8_t **pcursor, int *room, const uint32_t data)
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

    *cursor++ = data >> 24;
    *cursor++ = data >> 16;
    *cursor++ = data >> 8;
    *cursor++ = data & 0xFF;
    *pcursor = cursor;
    *room = *room - sizeof(uint32_t);
    ret = 0;
exit:
    return ret;
}

static inline int _ALIRO_PUT_UINT16(uint8_t **pcursor, int *room, const uint16_t data)
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

    *cursor++ = data >> 8;
    *cursor++ = data & 0xFF;
    *pcursor = cursor;
    *room = *room - sizeof(uint16_t);
    ret = 0;
exit:
    return ret;
}

static inline int _ALIRO_PUT_UINT8(uint8_t **pcursor, int *room, const uint8_t data)
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

static inline int _ALIRO_PUT_DATA(uint8_t **pcursor, int *room, const uint8_t *data, const int len)
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

int AliroUWBbuildM1(uint8_t *outbuf, size_t outbufSize, size_t *bytesMade);

#ifdef __cplusplus
}
#endif


