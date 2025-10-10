#ifndef __CLOCKSYNC_H__
#define __CLOCKSYNC_H__

extern "C"
{
    #include "serialqueue.h"
}
#include <vector>
#include <iostream>
#include <math.h>
#include <functional>
#include <unistd.h>
#include <mutex>
#include <pthread.h>
#include <iostream>

#include "../allwinner/Define.h"
#include "msgproto.h"
#include "serialhdl.h"
#include "reactor.h"
#include <atomic>

struct ClockEst{
    double time_avg;                //采样时间
    uint64_t clock_avg;         //采样时MCU计数器计数时钟
    double mcu_freq;            //MCU计数器时钟频率
};

typedef struct
{
	uint32_t R0;
	uint32_t R1;
	uint32_t R2;
	uint32_t R3;
	uint32_t R12;
	uint32_t LR;
	uint32_t PC;
	uint32_t xPSR;
	uint8_t crash_flag;
	uint8_t rest_cause; 
}stack_data_type_t;

class ClockSync{
public:
    ClockSync(){};
    ~ClockSync(){};

    ReactorTimerPtr m_get_clock_timer;
    int uptime_flag;
    int clock_flag;
    int m_clock_sync_flag;
    ParseResult uptime_params;
    ParseResult clock_params;
    std::mutex uptime_mux;
    std::mutex clock_mux;
    pthread_t clock_tid;
    bool m_clock_restart_flag;
    std::atomic<int> m_queries_pending;
    double m_mcu_freq;              //200MHZ 200 000 000  MCU TIMER FREQ
    uint64_t m_last_clock;
    //  最小往返时间跟踪
    double m_min_half_rtt;
    double m_min_rtt_time;
    //  mcu时钟与系统sent_time的线性回归
    double m_time_avg;
    double m_time_variance;
    double m_clock_avg;
    double m_clock_covariance;
    double m_prediction_variance;
    double m_last_prediction_time;

    stack_data_type_t m_stack_data;

    struct serialqueue *sq;
    ClockEst m_clock_est;
    Serialhdl *m_serial;


    double get_clock_event(double eventtime);
    void handle_pong(ParseResult &result_msg);
    void handle_clock(ParseResult &result_clock);
    void _handle_extruder_bootup_info(ParseResult params);
    uint64_t get_clock(double eventtime);
    double estimate_clock_systime(uint64_t reqclock);
    double estimated_print_time(double eventtime);
    uint64_t clock32_to_clock64(uint32_t clock);
    bool is_active(); 
   void creat_get_clock_pthread();

    virtual void connect(Serialhdl* serial)=0;
    virtual void connect_file(bool pace=false)=0;
    virtual std::string dump_debug()=0;
    virtual std::string stats(double eventtime)=0;

    virtual uint64_t print_time_to_clock(double print_time)=0; 
    virtual double clock_to_print_time(uint64_t clock)=0;
    virtual std::vector<double> calibrate_clock(double print_time, double eventtime)=0;



 
private:
 
};









class ClockSyncMain : public ClockSync{
public:
    ClockSyncMain();
    ~ClockSyncMain();

    void connect(Serialhdl* serial);
    void connect_file(bool pace=false);
    std::string dump_debug();
    std::string stats(double eventtime);

    uint64_t print_time_to_clock(double print_time); 
    double clock_to_print_time(uint64_t clock);
    std::vector<double> calibrate_clock(double print_time, double eventtime);
private:

};

class SecondarySync : public ClockSync{
    private:

    public:
    SecondarySync(ClockSyncMain* ClockSync);
    ~SecondarySync();

    ClockSyncMain* m_main_sync;
    std::vector<double> m_clock_adj;
    double m_last_sync_time;


    void connect_main(Serialhdl* serial);
    void connect_file_main(bool pace=false);
    std::string dump_debug_main();
    std::string stats_main(double eventtime);


    void connect(Serialhdl* serial);
    void connect_file(bool pace=false);;
    uint64_t print_time_to_clock(double print_time); 
    double clock_to_print_time(uint64_t clock);
    std::string dump_debug();
    std::string stats(double eventtime);
    std::vector<double> calibrate_clock(double print_time, double eventtime);

};

#endif
