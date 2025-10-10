#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// #define pins_per_bank       16

// #define SQT_UART 'u'
// #define SQT_CAN 'c'
// #define SQT_DEBUGFILE 'f'
// #define SQT_MEM 'm'






struct DspMemOps* GetUartOps();
#ifdef __cplusplus
}
#endif

#endif


