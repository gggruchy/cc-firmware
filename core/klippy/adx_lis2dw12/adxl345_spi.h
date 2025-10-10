#ifndef ADXL345_SPI
#define ADXL345_SPI

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <pthread.h>

// #define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

int spi_transfer(int fd, uint32_t speed,uint8_t const *tx, uint8_t const *rx, size_t len, const char* adxl_name);
int spi_init(int device_id,uint32_t speed);

#endif