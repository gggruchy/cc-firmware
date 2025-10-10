#ifndef __SHARESPACE__H__
#define __SHARESPACE__H__

#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdbool.h>
#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <sys/ioctl.h>
#include "../chelper/pyhelper.h"
#include "dspMemOps.h"
#include "SharespaceInit.h"
// #include "Debug.h"

#define CHOOSE_DSP_WRITE_SPACE		0
#define CHOOSE_ARM_WRITE_SPACE		1
#define CHOOSE_DSP_LOG_SPACE		2

// #define SHARESPACE_READ 1
// #define SHARESPACE_WRITE 0

extern uint16_t sharespace_arm_addr[2];
extern uint16_t sharespace_dsp_addr[2];


struct msg_head_t1 {
    uint32_t msg_start_addr;
    uint32_t msg_end_addr;
};


extern  int fd_sharespace;
extern uint16_t sharespace_arm_addr[2];
extern uint16_t sharespace_dsp_addr[2];

#endif