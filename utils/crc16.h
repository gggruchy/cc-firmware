#ifndef CRC16_H
#define CRC16_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdlib.h>
#define CRC_POLY_CCITT 0x1021
#define CRC_START_16 0x0000

    uint16_t crc_16(const uint8_t *input_str, size_t num_bytes);

#ifdef __cplusplus
}
#endif

#endif