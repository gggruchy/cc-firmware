#ifndef SUNXI_GPIO
#define SUNXI_GPIO

#include "stdlib.h"  
#include "stdio.h"  
#include "string.h"
#include "unistd.h"
#include "fcntl.h"   //define O_WRONLY and O_RDONLY  
#define SPI_GPIO_CS 0 

#define CS0_EXPORT_GPIO  "echo  33 > /sys/class/gpio/export"
#define CS0_GPIO_OUTPUT  "echo  out > /sys/class/gpio/gpio33/direction"
#define CS0_GPIO_VAL_L  "echo  0 > /sys/class/gpio/gpio33/value"
#define CS0_GPIO_VAL_H "echo  1 > /sys/class/gpio/gpio33/value"
#define CS1_EXPORT_GPIO  "echo  192 > /sys/class/gpio/export"
#define CS1_GPIO_OUTPUT  "echo  out > /sys/class/gpio/gpio192/direction"
#define CS1_GPIO_VAL_L  "echo  0 > /sys/class/gpio/gpio192/value"
#define CS1_GPIO_VAL_H "echo  1 > /sys/class/gpio/gpio192/value"

int spi_cs_init();

void cs0_Init();

void cs1_Init();

void cs0_h();

void cs0_l();

void cs1_h();

void cs1_l();

#endif