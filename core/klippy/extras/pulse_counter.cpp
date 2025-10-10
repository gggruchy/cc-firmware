#include "pulse_counter.h"
#include "pins.h"
#include "klippy.h"

MCU_counter::MCU_counter(std::string pin, double sample_time, double poll_time)
{
    PrinterPins *ppins = Printer::GetInstance()->m_ppins;
    pinParams *pin_params = ppins->lookup_pin(pin, false, true);
    m_mcu =  (MCU *)pin_params->chip;
    m_oid = m_mcu->create_oid();
    m_pin = pin_params->pin;
    m_pullup = pin_params->pullup;
    m_poll_time = poll_time;
    m_poll_ticks = 0;
    m_sample_time = sample_time;
    m_last_count = 0;
    m_mcu->register_config_callback(std::bind(&MCU_counter::build_config, this, std::placeholders::_1));
}

MCU_counter::~MCU_counter()
{
}

void MCU_counter::build_config(int para)
{
    m_mcu->add_config_cmd("config_counter oid=" + std::to_string(m_oid) + " pin=" + m_pin + " pull_up=" + std::to_string(m_pullup));
    uint64_t clock = m_mcu->get_query_slot(m_oid);
    m_poll_ticks = m_mcu->seconds_to_clock(m_poll_time);
    uint64_t sample_ticks = m_mcu->seconds_to_clock(m_sample_time);
    m_mcu->add_config_cmd("query_counter oid=" + std::to_string(m_oid) + " clock=" + std::to_string(clock) + " poll_ticks=" + std::to_string(m_poll_ticks) + " sample_ticks=" + std::to_string(sample_ticks), true);
    m_mcu->register_response(std::bind(&MCU_counter::handle_counter_state, this, std::placeholders::_1), "counter_state", m_oid);
}

void MCU_counter::setup_callback(std::function<void(double, uint32_t, double)> cb)
{
    m_callback = cb;
}

void MCU_counter::handle_counter_state(ParseResult &params)
{
    double next_clock = m_mcu->clock32_to_clock64(params.PT_uint32_outs["next_clock"]);
    double time = m_mcu->clock_to_print_time(next_clock - m_poll_ticks);

    double count_clock = m_mcu->clock32_to_clock64(params.PT_uint32_outs["count_clock"]);
    double count_time = m_mcu->clock_to_print_time(count_clock);

    uint32_t last_count = m_last_count;

    uint32_t delta_count = (params.PT_uint32_outs["count"] - last_count) & 0xffffffff;
    uint32_t count = last_count + delta_count;
    m_last_count = count;
    if(m_callback != nullptr)
    {
        m_callback(time, count, count_time);
    }
}


FrequencyCounter::FrequencyCounter(std::string pin, double sample_time, double poll_time)
{
    m_last_time = 0;
    m_last_count = 0;
    m_freq = 0;
    m_counter = new MCU_counter(pin, sample_time, poll_time);
    m_counter->setup_callback(std::bind(&FrequencyCounter::counter_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

FrequencyCounter::~FrequencyCounter()
{
    delete m_counter;
}

void FrequencyCounter::counter_callback(double time, uint32_t count, double count_time)
{
    if(!m_last_time)
    {
        m_last_time = time;
    }
    else
    {
        double delta_time = count_time - m_last_time;
        if(delta_time > 0)
        {
            m_last_time = count_time;
            uint32_t delta_count = count - m_last_count;
            m_freq = delta_count / delta_time;
        }
        else
        {
            m_last_time = time;
            m_freq = 0;
        }
        if(m_callback != nullptr)
        {
            m_callback(time, m_freq);
        }
    }
    m_last_count = count;
}

double FrequencyCounter::get_frequency()
{
    return m_freq;
}