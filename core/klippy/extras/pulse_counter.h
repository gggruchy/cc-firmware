#ifndef PULSE_COUNTER_H
#define PULSE_COUNTER_H
#include "mcu.h"
#include "functional"
#include "msgproto.h"

class MCU_counter
{
private:
    MCU *m_mcu;
    int m_oid;
    std::string m_pin;
    int m_pullup;
    double m_poll_time;
    uint64_t m_poll_ticks = 0;
    double m_sample_time;
    std::function<void(double, uint32_t, double)> m_callback;
    uint32_t m_last_count = 0;


public:
    MCU_counter(std::string pin, double sample_time, double poll_time);
    ~MCU_counter();
    void build_config(int para);
    void setup_callback(std::function<void(double, uint32_t, double)> cb);
    void handle_counter_state(ParseResult &params);
};

class FrequencyCounter
{
private:
    double m_last_time;
    uint32_t m_last_count;
    double m_freq;
    MCU_counter *m_counter;
    std::function<void(double, double)> m_callback;
public:
    FrequencyCounter(std::string pin, double sample_time, double poll_time);
    ~FrequencyCounter();
    void counter_callback(double time, uint32_t count, double count_time);
    double get_frequency();
};


#endif // !__PULSE_COUNTER__H__
