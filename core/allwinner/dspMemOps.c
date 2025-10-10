#include <linux/can.h> // // struct can_frame
#include <math.h>      // fabs

#include <stddef.h>      // offsetof
#include <stdint.h>      // uint64_t
#include <stdio.h>       // snprintf
#include <stdlib.h>      // malloc
#include <string.h>      // memset
#include <termios.h>     // tcflush
#include <unistd.h>      // pipe
#include "compiler.h"    // __visible
#include "list.h"        // list_add_tail
#include "msgblock.h"    // message_alloc
#include "pollreactor.h" //
#include "pyhelper.h"    //
#include "serialqueue.h" //

#include <stddef.h>
#include <stdint.h>
#include "dspMemOps.h"
#include "msgbox.h"
#include "Sharespace.h"
#include "debug.h"
#define LOG_TAG "dspmem"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

extern struct DspMemOps *GetDspKbufOps();

struct DspMemOps *pDspMemOps; // pUartOps

struct DspMemOps *GetDspMemOps()
{
    pDspMemOps = GetDspKbufOps();
    return pDspMemOps; // GetDspKbufOps GetDspUartOps  GetDspIonMemOps GetDspsharespaceOps
}

void GAM_DEBUG_printf_HEX(uint8_t write, uint8_t *buf, uint32_t buf_len);
int do_read_sq(uint8_t *buf, int buflen) //---13-3task-G-G--RW_task-
{
    // double eventtime = get_monotonic();
    int ret = pDspMemOps->mem_read(buf); //  read_dsp_space(fd, buf);
    // double eventtime1 = get_monotonic();
    // if (eventtime1 - eventtime > 0.0005)
    // {
    // GAM_DEBUG_send_MOVE("read : %f\n",eventtime1 - eventtime );
    // GAM_DEBUG_printf_HEX(0,buf,ret);
    // }

    // {
    //     uint8_t *tbuf = (uint8_t *)buf;
    //     // if((tbuf[2] == 86)   ) //if((tbuf[2] != 71) && (tbuf[2] != 0x4e)  && (tbuf[2] != 0x45)   )          //71
    //     {
    //         // printf("528:" );
    //         GAM_DEBUG_printf_HEX(0,tbuf,ret);
    //     }
    // }

    if (ret > 0)
    {
        if ((buf[0] == 0) || (buf[0] == 0X5a) || (buf[ret - 1] != 0X7e))
        {
            printf("re_do_read--%d----", ret);
            GAM_DEBUG_printf_HEX(0, buf, ret);
            ret = 0;
        }
    }
    pDspMemOps->set_read_pos(ret); //    set_read_dsp_space_pos(ret);
    if (ret > 0)
    {
        pDspMemOps->read_lenth += ret;
    }
    return ret;
}

void do_write_sq(struct serialqueue *sq, void *buf, int buflen) //----3-G-G-2022-04-08-----------------
{
    if (sq->serial_fd_type == SQT_MEM)
    {
        // double eventtime = get_monotonic();
        pDspMemOps->mem_write(buf, buflen); // write_into_arm_write_space( );   //-6-g-g-2022-06-16
#if WRITE_MSGBOX_ENABLE || SYNC_MSGBOX_ENABLE
        msgbox_send_signal(MSGBOX_IS_WRITE, sharespace_arm_addr[SHARESPACE_WRITE]); //-
                                                                                    //  double eventtime1 = get_monotonic();
                                                                                    // if (eventtime1 - eventtime > 0.0005)
                                                                                    // {
                                                                                    // GAM_DEBUG_send_MOVE("send time: %f\n",eventtime1 - eventtime );
                                                                                    // GAM_DEBUG_send_MOVE("write %f:\n",eventtime1 - eventtime );

#if UART_DATA_DEBUG
        {
            uint8_t *tbuf = (uint8_t *)buf;
            if (tbuf[2] == 17 || tbuf[2] == 63 || tbuf[2] == 60) // 0x28-get_clock
            {
                printf("DSP WRITE");
                GAM_DEBUG_printf_HEX(1, buf, buflen);
            }
        }
#endif

#endif
        // LOG_I(" SQT_MEM write %d\n", buflen);
        pDspMemOps->write_lenth += buflen;
        return;
    }
    else if (sq->serial_fd_type == SQT_UART)
    {
#if UART_DATA_DEBUG
        {
            uint8_t *tbuf = (uint8_t *)buf;
            if (tbuf[2] != 0x28&&tbuf[2] != 41) // 0x28-get_clock
            {
                printf("UART WRITE");
                GAM_DEBUG_printf_HEX(1, buf, buflen);
            }
        }
#endif
        int ret = write(sq->serial_fd, buf, buflen);
        if (ret < 0)
        {
            LOG_E("SQT_UART  write error %d fd : %d\n", ret, sq->serial_fd);
        }
        return;
    }
}