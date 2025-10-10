#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

typedef enum
{
    SERIAL_OPT_PARITY_EVEN,
    SERIAL_OPT_PARITY_ODD,
    SERIAL_OPT_PARITY_NONE,

    SERIAL_OPT_STOP_BIT_1,
    SERIAL_OPT_STOP_BIT_1_5,
    SERIAL_OPT_STOP_BIT_2,

    SERIAL_OPT_DATA_BIT_5,
    SERIAL_OPT_DATA_BIT_6,
    SERIAL_OPT_DATA_BIT_7,
    SERIAL_OPT_DATA_BIT_8,
} serial_opt_t;

int serial_open(const char *devpath, int parity, int stop_bit, int data_bit, int baudrate);
int serial_write(int fd, const uint8_t *data, uint32_t length);
int serial_read(int fd, uint8_t *data, uint32_t length);
int serial_tcflush(int fd);

#endif