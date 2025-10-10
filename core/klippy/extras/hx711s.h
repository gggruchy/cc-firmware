/**
 * @file hx711.h
 * @author
 * @brief
 * @version 0.1
 * @date 2023-12-05
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef _HX711S_H
#define _HX711S_H

#include <stdint.h>
#include <vector>
#include "filter.h"
#include "gcode.h"
#include "serialqueue.h"
#include "msgproto.h"
#include "mcu.h"
#include "strain_gauge_kalman_filter.h"

#ifdef __cplusplus
extern "C"
{
#endif
    #define STRAIN_GAUGE_DEQUE_SIZE 10
    enum
    {
        HX711_LOWER_LEFT,
        HX711_LOWER_RIGHT,
        HX711_UPPER_RIGHT,
        HX711_UPPER_LEFT,
        HX711_LIST
    };
    void usb_uart_init(void);
    int32_t *get_hx711_val(void);

    typedef struct strain_gauge_params_t
    {
        uint32_t now_inter_ms;
        uint32_t now_tick;
        int32_t hx711s_outvals[HX711_LIST];
        double time;
    } strain_gauge_params;

    class HX711S
    {
    private:
    public:
        HX711S(std::string section_name);
        ~HX711S();

        void _build_config(int para);
        void _handle_mcu_identify();
        void _handle_debug_hx711s(ParseResult &params);
        void _handle_shutdown();
        void _handle_disconnect();
        void _handle_sg_resp(ParseResult &params);
        std::vector<ParseResult> get_params();
        std::vector<std::vector<double>> get_vals();
        std::vector<double> get_vals_new();
        void delay_s(double delay_s);
        void query_start(int pi_count, int cycle_count, bool del_dirty = false, bool show_msg = false, bool is_ck_con = false);
        void ProbeCheckTriggerStart(int x, int y, int z, bool ack = true);
        void ProbeCheckTriggerStop(bool ack = true);        
        bool CalibrationStart(int times_read, bool is_check_err = false);
        std::vector<double> read_base(int cnt, double max_hold, bool reset_zero = true);
        void cmd_READ_HX711(GCodeCommand &gcmd);

        int m_s_count;
        int m_enable_test;
        uint32_t m_rest_ticks;
        int m_install_dir;
        int m_enable_hpf;
        int m_enable_shake_filter;
// bit7-0:       4            3           2               1                   0
//                                     k补偿计算方式     回退点的方式         应用k补偿
//             0:         0:          0:线性拟合          0:线性             0:不应用
//             1:         1:          1:固定规律          1:阈值             1:应用
        int m_find_index_mode;

        uint32_t m_enable_channels;
        uint32_t m_sg_mode;
        int m_th_k;
        int m_k_slope;
        int m_bias_slope;
        int m_kalman_q;
        int m_kalman_r;
        int m_max_th;
        int m_min_th;

        MCU *m_mcu;
        command_queue *m_queue;
        std::vector<std::string> m_s_clk_pin;
        std::vector<std::string> m_s_sdo_pin;
        std::vector<double> m_base_avgs;
        bool m_del_dirty;
        int m_index_dirty;
        uint64_t m_start_tick;
        uint64_t m_trigger_tick;
        uint64_t m_time_tick;
        double m_trigger_timestamp;
        double m_time_timestamp;
        double m_zaxis_timestamp;
        double m_delta_zaxis_sg_timestamp;
        bool m_need_wait;
        // std::vector<double> m_s_clk_pin;
        // std::vector<double> m_s_sdo_pin;
        std::vector<ParseResult> m_all_params;
        std::vector<std::vector<double>> m_all_vals;
        std::deque<ParseResult> m_fusion_params;
        std::deque<double> m_fusion_vals;
        int32_t choosed_sensor;
        int m_pi_count;
        bool m_show_msg;
        Filter *m_filter; 
        // std::shared_ptr<StrainGaugeKalmanFilter> strain_gauge_kalman_filter_ = nullptr;
        // m_query_cmd = None
        int32_t m_mcu_freq;
        double m_last_send_heart;
        bool m_is_shutdown;
        bool m_is_timeout;
        std::atomic_uchar m_is_calibration;
        std::atomic_uchar m_is_trigger;            // 触发标志
        std::atomic_uchar m_probe_cmd_response;         // probe cmd回复标志
        std::atomic_uchar m_probe_calibration_response;  // probe calibration回复标志
        int m_oid;
        int m_cycle_count;
        std::atomic_int m_times_read;
        std::mutex m_mutex;
        // int m_times_read;
    };

#ifdef __cplusplus
}
#endif

#endif /*_HX711_H*/
