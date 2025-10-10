#ifndef __DEBUG_H__
#define __DEBUG_H__
// #include "Debug.h"

#define uart_dbg(fmt, ...) // printf(fmt, ##__VA_ARGS__) //--G-G-2022-06-07-----
#define DisTraceMsg(enabel, ...)                   \
    // do                                             \
    // {                                              \
    //     if (enabel)                                \
    //     {                                          \
    //         printf("%s:%d: ", __func__, __LINE__); \
    //         printf(__VA_ARGS__);                   \
    //     }                                          \
    // } while (0)
#define DisErrorMsg()                                                          \
    // {                                                                          \
    //     GAM_DEBUG_printf_time();                                               \
    //     uart_dbg("###### Error!!!!!!File:%s Line:%d\r\n", __FILE__, __LINE__); \
    // }
#define time_debug(a, b)                                                                        \
    // {                                                                                           \
    //     GAM_DEBUG_printf_time();                                                                \
    //     printf("file :%s func :%s line :%d a :%d b :%s\n", __FILE__, __func__, __LINE__, a, b); \
    // }
#define share_space_debug(a, b)                                                                 \
    // {                                                                                           \
    //     GAM_DEBUG_printf_time();                                                                \
    //     printf("file :%s func :%s line :%d a :%d b :%s\n", __FILE__, __func__, __LINE__, a, b); \
    // }
#define GAM_DEBUG_printf(fmt, ...) // printf(fmt, ##__VA_ARGS__)
#define DEBUG_printf(fmt, ...) // printf(fmt, ##__VA_ARGS__)
#define ERRER_DEBUG_printf(fmt, ...) \
    // {                                \
    //     GAM_DEBUG_printf_time();     \
    //     printf(fmt, ##__VA_ARGS__);  \
    // }
#define read_write_pthread_debug(fmt, ...) \
    // {                                      \
    //     GAM_DEBUG_printf_time();           \
    //     printf(fmt, ##__VA_ARGS__);        \
    // }
#define GAM_ERR_printf(fmt, ...) // printf(fmt, ##__VA_ARGS__)//{GAM_DEBUG_printf_time();printf(fmt, ##__VA_ARGS__);}

#define GAM_DEBUG_config(fmt, ...)       // printf(fmt, ##__VA_ARGS__)
#define GAM_DEBUG_send(fmt, ...)         // printf(fmt, ##__VA_ARGS__)
#define GAM_DEBUG_receive(fmt, ...)      // printf(fmt, ##__VA_ARGS__)
#define GAM_DEBUG_send_MOVE(fmt, ...)    // printf(fmt, ##__VA_ARGS__)
#define GAM_DEBUG_send_UI(fmt, ...)      // printf(fmt, ##__VA_ARGS__)
#define GAM_DEBUG_send_UI_home(fmt, ...) // printf(fmt, ##__VA_ARGS__)
#define GAM_DEBUG_send_clock(fmt, ...)   // printf(fmt, ##__VA_ARGS__)
#endif
