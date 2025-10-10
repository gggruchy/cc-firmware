#ifndef DSP_MEMOPS_H
#define DSP_MEMOPS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
#define UART_DATA_DEBUG 0
#define R528_DSP_V812 1
    // #define pins_per_bank       32

#define SQT_UART 'u'
#define SQT_CAN 'c'
#define SQT_DEBUGFILE 'f'
#define SQT_MEM 'm'

#define RECEIVE_WINDOW_DSP528 192
#define MCU_TYPE_DSP528 1
#define CLOCK_FREQ_DSP528 200000000.0

#define SERIAL_BAUD_DSP528 0.00001
#define PWM_MAX_DSP528 2000000
#define ADC_MAX_DSP528 4095
#define STATS_SUMSQ_BASE_DSP528 256

    struct DspMemOps
    {
        uint64_t write_lenth;
        uint64_t read_lenth;
        int (*init)(void);
        int (*de_init)(void);
        int (*re_init)(void);
        int (*mem_read)(uint8_t *buf);
        int (*mem_write)(uint8_t *buf, int len);
        void (*set_read_pos)(uint32_t read_addr);
        int fd_write;
        int fd_read;
        int mem_actual_width;
        int mem_actual_height;
        char fd_write_type;
        int receive_window; // RECEIVE_WINDOW
        int mcu_type;
        double clock_freq;  // CLOCK_FREQ
        double serial_baud; // SERIAL_BAUD
        double pwm_max;
        double adc_max;
        double stats_sumsq_base;
        int reserve_pin[20]; // BUS_PINS_  RESERVE_PINS_
    };

    struct DspMemOps *GetDspsharespaceOps();
    struct DspMemOps *GetDspMemOps();
    int GAM_DEBUG_printf_time();
#ifdef __cplusplus
}
#endif

#endif
