#include "mcu_io.h"
#include "klippy.h"
#include "Define.h"
#define LOG_TAG "mcu_io"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
MCU_trsync::MCU_trsync(MCU *mcu, trdispatch *trdispatch)
{
    m_mcu = mcu;
    m_trdispatch = trdispatch;
    // m_reactor = mcu.get_printer().get_reactor();  ---??---
    // m_trdispatch_mcu = nullptr;
    m_oid = mcu->create_oid();
    m_cmd_queue = m_mcu->alloc_command_queue();
    // m_trsync_start_cmd = nullptr;
    // m_trsync_set_timeout_cmd = nullptr;
    // m_trsync_trigger_cmd = nullptr;
    // m_trsync_query_cmd = nullptr;
    // m_stepper_stop_cmd = nullptr;
    // self._trigger_completion = None // ReactorCompletion ---??---
    m_home_end_clock = 0;
    m_mcu->register_config_callback(std::bind(&MCU_trsync::build_config, this, std::placeholders::_1));
    Printer::GetInstance()->register_event_handler("klippy:shutdown:MCU_trsync "+m_mcu->m_name, std::bind(&MCU_trsync::shutdown, this));    //暂时无意义
}

MCU_trsync::~MCU_trsync()
{
    if (m_cmd_queue != nullptr)
    {
        serialqueue_free_commandqueue(m_cmd_queue);
    }
    if (m_trdispatch_mcu != nullptr)
    {
        free(m_trdispatch_mcu);
    }
}

MCU *MCU_trsync::get_mcu()
{
    return m_mcu;
}

int MCU_trsync::get_oid()
{
    return m_oid;
}

command_queue *MCU_trsync::get_command_queue()
{
    return m_cmd_queue;
}

void MCU_trsync::add_stepper(MCU_stepper *stepper)
{
    // if stepper in self._steppers: ---??---
    //         return
    m_steppers.push_back(stepper);
}

std::vector<MCU_stepper *> MCU_trsync::get_steppers()
{
    return m_steppers;
}

void MCU_trsync::build_config(int para)
{
    if (para & 1)
    {
        std::stringstream config_trsync;
        config_trsync << "config_trsync oid=" << m_oid;
        m_mcu->add_config_cmd(config_trsync.str());
        int set_timeout_tag = m_mcu->m_serial->m_msgparser->m_format_to_id.at("trsync_set_timeout oid=%c clock=%u");
        int trigger_tag = m_mcu->m_serial->m_msgparser->m_format_to_id.at("trsync_trigger oid=%c reason=%c");
        int state_tag = m_mcu->m_serial->m_msgparser->m_format_to_id.at("trsync_state oid=%c can_trigger=%c trigger_reason=%c clock=%u");
        m_trdispatch_mcu = trdispatch_mcu_alloc(m_trdispatch, m_mcu->m_serial->m_serialqueue, m_cmd_queue, m_oid, set_timeout_tag, trigger_tag, state_tag);
    }
    if (para & 4)
    {
        std::stringstream trsync_start;
        trsync_start << "trsync_start oid=" << m_oid << " report_clock=0 report_ticks=0 expire_reason=0";
        m_mcu->add_config_cmd(trsync_start.str(), false, true);
    }
}

void MCU_trsync::shutdown()
{
    // m_trigger_completion;  ---??---MCU_trsync
    // tc = self._trigger_completion
    // if tc is not None:
    //     self._trigger_completion = None
    //     tc.complete(False)
}

void MCU_trsync::handle_trsync_state(ParseResult params) // trsync_state
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;

    if (!params.PT_uint32_outs.at("can_trigger"))
    {
        // m_trigger_mtx.lock();

        // m_trigger_mtx.unlock();
        int reason = params.PT_uint32_outs.at("trigger_reason");
        bool is_failure = (reason == REASON_COMMS_TIMEOUT);
        // if(!is_failure)
        {
            Printer::GetInstance()->m_tool_head->is_trigger = true; //-----G-G-2023-05-10---------
        }

        // reason = params['trigger_reason']
        // is_failure = (reason == self.REASON_COMMS_TIMEOUT)
        // self._reactor.async_complete(tc, is_failure)  //---??---
    }
    else if (m_home_end_clock != 0)
    {
        uint32_t clock = params.PT_uint32_outs.at("clock");
        uint64_t clock64 = m_mcu->m_clocksync->clock32_to_clock64(clock);
        if (clock64 >= m_home_end_clock)
        {
            m_home_end_clock = 0;
            std::stringstream trsync_trigger_str;
            trsync_trigger_str << "trsync_trigger oid=" << m_oid << " reason=" << REASON_PAST_END_TIME;
            m_mcu->m_serial->send(trsync_trigger_str.str(), minclock, reqclock, m_cmd_queue); // m_mcu->m_serial->m_command_queue
        }
    }
}

void MCU_trsync::start(double print_time, double expire_timeout)
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;
    m_home_end_clock = 0;
    uint64_t clock = m_mcu->print_time_to_clock(print_time);
    uint64_t expire_ticks = m_mcu->seconds_to_clock(expire_timeout);
    uint64_t expire_clock = clock + expire_ticks;
    uint64_t report_ticks = m_mcu->seconds_to_clock(expire_timeout * 0.4);
    uint64_t min_extend_ticks = m_mcu->seconds_to_clock(expire_timeout * 0.4 * 0.8);
    trdispatch_mcu_setup(m_trdispatch_mcu, clock, expire_clock, expire_ticks, min_extend_ticks);
    m_mcu->m_serial->register_response(std::bind(&MCU_trsync::handle_trsync_state, this, std::placeholders::_1), "trsync_state", m_oid);
    reqclock = clock;
    std::stringstream trsync_start;
    trsync_start << "trsync_start oid=" << m_oid << " report_clock=" << (uint32_t)clock << " report_ticks=" << (uint32_t)report_ticks << " expire_reason=" << REASON_COMMS_TIMEOUT;
    m_mcu->m_serial->send(trsync_start.str(), minclock, reqclock, m_cmd_queue);
    for (int i = 0; i < m_steppers.size(); i++)
    {
        std::stringstream stepper_stop_on_trigger;
        stepper_stop_on_trigger << "stepper_stop_on_trigger oid=" << m_steppers[i]->m_oid << " trsync_oid=" << m_oid;
        m_mcu->m_serial->send(stepper_stop_on_trigger.str(), minclock, reqclock, m_cmd_queue);
    }
    std::stringstream trsync_set_timeout;
    trsync_set_timeout << "trsync_set_timeout oid=" << m_oid << " clock=" << (uint32_t)expire_clock;
    reqclock = expire_clock;
    m_mcu->m_serial->send(trsync_set_timeout.str(), minclock, reqclock, m_cmd_queue);
}

void MCU_trsync::start_z(double print_time, double expire_timeout)
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;
    m_home_end_clock = 0;
    uint64_t clock = m_mcu->print_time_to_clock(print_time);
    uint64_t expire_ticks = m_mcu->seconds_to_clock(expire_timeout);
    uint64_t expire_clock = clock + expire_ticks;
    uint64_t report_ticks = m_mcu->seconds_to_clock(expire_timeout * 0.4);
    uint64_t min_extend_ticks = m_mcu->seconds_to_clock(expire_timeout * 0.4 * 0.8);
    trdispatch_mcu_setup(m_trdispatch_mcu, clock, expire_clock, expire_ticks, min_extend_ticks);
    m_mcu->m_serial->register_response(std::bind(&MCU_trsync::handle_trsync_state, this, std::placeholders::_1), "trsync_state", m_oid);
    reqclock = clock;
    std::stringstream trsync_start;
    trsync_start << "trsync_start oid=" << m_oid << " report_clock=" << (uint32_t)clock << " report_ticks=" << (uint32_t)report_ticks << " expire_reason=" << REASON_COMMS_TIMEOUT;
    m_mcu->m_serial->send(trsync_start.str(), minclock, reqclock, m_cmd_queue);
    for (int i = 0; i < m_steppers.size(); i++)
    {
        std::stringstream stepper_stop_on_trigger;
        stepper_stop_on_trigger << "stepper_stop_on_trigger oid=" << m_steppers[i]->m_oid << " trsync_oid=" << m_oid;
        m_mcu->m_serial->send(stepper_stop_on_trigger.str(), minclock, reqclock, m_cmd_queue);
    }
    // std::stringstream trsync_set_timeout;
    // trsync_set_timeout << "trsync_set_timeout oid=" << m_oid << " clock=" << (uint32_t)expire_clock;
    // reqclock = expire_clock;
    // m_mcu->m_serial->send(trsync_set_timeout.str(), minclock, reqclock, m_cmd_queue);
}

void MCU_trsync::set_home_end_time(double home_end_time)
{
    m_home_end_clock = m_mcu->print_time_to_clock(home_end_time);
}

uint32_t MCU_trsync::stop()
{
    uint64_t minclock;
    uint64_t reqclock;
    std::stringstream trsync_trigger_str;
    trsync_trigger_str << "trsync_trigger oid=" << m_oid << " reason=" << REASON_HOST_REQUEST;
    if (m_mcu->m_serial->m_response_callbacks.find("trsync_state" + std::to_string(m_oid)) != m_mcu->m_serial->m_response_callbacks.end())
    {
        m_mcu->m_serial->m_response_callbacks.erase("trsync_state" + std::to_string(m_oid));
    }
    ParseResult params = m_mcu->m_serial->send_with_response(trsync_trigger_str.str(), "trsync_state", m_cmd_queue, m_oid);
    for (int i = 0; i < m_steppers.size(); i++)
    {
        m_steppers[i]->note_homing_end(true);
    }
    return params.PT_uint32_outs.at("trigger_reason");
}

uint32_t MCU_trsync::stop_z()
{
    uint64_t minclock;
    uint64_t reqclock;
    std::stringstream trsync_trigger_str;
    trsync_trigger_str << "trsync_trigger oid=" << m_oid << " reason=" << REASON_ENDSTOP_HIT;
    if (m_mcu->m_serial->m_response_callbacks.find("trsync_state" + std::to_string(m_oid)) != m_mcu->m_serial->m_response_callbacks.end())
    {
        m_mcu->m_serial->m_response_callbacks.erase("trsync_state" + std::to_string(m_oid));
    }
    ParseResult params = m_mcu->m_serial->send_with_response(trsync_trigger_str.str(), "trsync_state", m_cmd_queue, m_oid);
    Printer::GetInstance()->m_tool_head->wait_moves();
    for (int i = 0; i < m_steppers.size(); i++)
    {
        m_steppers[i]->note_homing_end(true);
    }
    return 1;
}

MCU_endstop::MCU_endstop(MCU *mcu, pinParams *pin_params)
{
    // m_trigger_completion;
    m_mcu = mcu;
    m_pin = m_mcu->m_serial->m_msgparser->m_pinMap[pin_params->pin]; // m_pin = pin_params->pin;
    m_pullup = pin_params->pullup;
    m_invert = pin_params->invert;
    m_oid = m_mcu->create_oid();
    m_trdispatch = trdispatch_alloc();
    m_mcu->register_config_callback(std::bind(&MCU_endstop::build_config, this, std::placeholders::_1));
    m_trsync = new MCU_trsync(mcu, m_trdispatch);
    m_cmd_queue = m_trsync->get_command_queue();
    m_rest_ticks = 0;
    m_trsyncs.push_back(m_trsync);
}

MCU_endstop::~MCU_endstop()
{
    if (m_trsync != nullptr)
    {
        delete m_trsync;
    }
    if (m_trdispatch != nullptr)
    {
        delete m_trdispatch;
    }
}

MCU *MCU_endstop::get_mcu()
{
    return m_mcu;
}

void MCU_endstop::add_stepper(MCU_stepper *stepper)
{
    m_trsync->add_stepper(stepper);
    // std::map<std::string, MCU_trsync*> trsyncs;
    // for(auto trsync : m_trsyncs)
    // {
    //     trsyncs[trsync->get_mcu()->get_name()] = trsync;
    // }
    // MCU_trsync* trsync = trsyncs[stepper->get_mcu()->get_name()];
    // if(trsync == nullptr)
    // {
    //     trsync = MCU_trsync(stepper->get_mcu(), m_trdispatch);
    //     m_trsyncs.push_back(trsync);
    // }
    // m_trsync->add_stepper(stepper);
    // // Check for unsupported multi-mcu shared stepper rails
    // std::string sname = stepper->get_name();
    // if(startswith(sname, "stepper_"))
    // {
    //     for(auto ot : m_trsyncs)
    //     {
    //         for(auto s : ot->get_steppers())
    //         {
    //             //---??---
    //             // if ot is not trsync and s.get_name().startswith(sname[:9]):
    //             //         cerror = self._mcu.get_printer().config_error
    //             //         raise cerror("Multi-mcu homing not supported on"
    //             //                      " multi-mcu shared axis")
    //         }
    //     }
    // }
}

std::vector<MCU_stepper *> MCU_endstop::get_steppers()
{
    return m_trsync->get_steppers();
    // std::vector<std::vector<MCU_stepper*>> ret;
    // for(auto trsync : m_trsyncs)
    // {
    //     ret.push_back(trsync->get_steppers());
    // }
    // return ret;
}

void MCU_endstop::build_config(int para)
{
    if (para & 1)
    {
        std::stringstream config_endstop;
        config_endstop << "config_endstop oid=" << m_oid << " pin=" << m_pin << " pull_up=" << m_pullup;
        m_mcu->add_config_cmd(config_endstop.str());
    }
    if (para & 4)
    {
        std::stringstream endstop_home;
        endstop_home << "endstop_home oid=" << m_oid << " clock=0 sample_ticks=0 sample_count=0"
                     << " rest_ticks=0 pin_value=0 trsync_oid=0 trigger_reason=0";
        m_mcu->add_config_cmd(endstop_home.str(), false, true);
    }
}

void MCU_endstop::home_start(double print_time, double sample_time, int sample_count, double rest_time, bool triggered)
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;
    uint64_t clock = m_mcu->print_time_to_clock(print_time);
    uint64_t rest_ticks = m_mcu->print_time_to_clock(print_time + rest_time) - clock;
    uint64_t sample_ticks = m_mcu->seconds_to_clock(sample_time);
    int pin_value = triggered ^ m_invert;
    int trsync_oid = m_trsync->m_oid;
    m_trsync->start(print_time, 0.250);
    trdispatch_start(m_trdispatch, REASON_HOST_REQUEST);
    std::stringstream endstop_home;
    endstop_home << "endstop_home oid=" << m_oid << " clock=" << (uint32_t)clock << " sample_ticks=" << (uint32_t)sample_ticks << " sample_count=" << sample_count << " rest_ticks=" << (uint32_t)rest_ticks << " pin_value=" << pin_value << " trsync_oid=" << trsync_oid << " trigger_reason=" << REASON_ENDSTOP_HIT;

    m_mcu->m_serial->send(endstop_home.str(), minclock, reqclock, m_trsync->m_cmd_queue);
}

void MCU_endstop::home_start_z(double print_time, double sample_time, int sample_count, double rest_time, bool triggered)
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;
    uint64_t clock = m_mcu->print_time_to_clock(print_time);
    uint64_t rest_ticks = m_mcu->print_time_to_clock(print_time + rest_time) - clock;
    uint64_t sample_ticks = m_mcu->seconds_to_clock(sample_time);
    int pin_value = triggered ^ m_invert;
    int trsync_oid = m_trsync->m_oid;
    m_trsync->start(print_time, 0.250);
    trdispatch_start(m_trdispatch, REASON_HOST_REQUEST);
}

int MCU_endstop::home_wait(double home_end_time)
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;
    m_trsync->set_home_end_time(home_end_time);
    // while(Printer::GetInstance()->m_tool_head->is_trigger == false)
    //     ;
    std::stringstream endstop_home;
    endstop_home << "endstop_home oid=" << m_oid << " clock=" << 0 << " sample_ticks=" << 0 << " sample_count=" << 0 << " rest_ticks=" << 0 << " pin_value=" << 0 << " trsync_oid=" << 0 << " trigger_reason=" << 0;
    m_mcu->m_serial->send(endstop_home.str(), minclock, reqclock, m_trsync->m_cmd_queue);

    trdispatch_stop(m_trdispatch);

    int ret = m_trsync->stop();
    Printer::GetInstance()->m_tool_head->is_trigger = false; //-----G-G-2023-05-10---------
    // usleep(500 * 1000);
    // std::cout << "ret 122 " << ret << std::endl;
    return ret == REASON_ENDSTOP_HIT;
}

int MCU_endstop::home_wait_z(double home_end_time)
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;
    m_trsync->set_home_end_time(home_end_time);
    // while(Printer::GetInstance()->m_tool_head->is_trigger == false)
    //     ;
    trdispatch_stop(m_trdispatch);

    int ret = m_trsync->stop_z();
    Printer::GetInstance()->m_tool_head->is_trigger = false; //-----G-G-2023-05-10---------
    // usleep(500 * 1000);
    // std::cout << "ret 122 " << ret << std::endl;
    // return ret == REASON_ENDSTOP_HIT;
    return 1;
}

int MCU_endstop::query_endstop(double print_time)
{
    m_mcu->print_time_to_clock(print_time);
    if (m_mcu->is_fileoutput())
    {
        return 0;
    }
    // m_query_cmd->send()
    // params = self._query_cmd.send([self._oid], minclock=clock)
    //     return params['pin_value'] ^ self._invert  //---??---MCU_endstop
}

MCU_digital_out::MCU_digital_out(MCU *mcu, pinParams *params)
{
    m_mcu = mcu;
    m_oid = 0;
    m_pin = m_mcu->m_serial->m_msgparser->m_pinMap[params->pin]; // m_pin = params->pin;
    m_invert = params->invert;
    m_start_value = m_invert;
    m_shutdown_value = m_invert;
    m_is_static = false;
    m_max_duration = 2.0;
    m_last_clock = 0.0;
    m_mcu->register_config_callback(std::bind(&MCU_digital_out::build_config, this, std::placeholders::_1)); // --2022-8.31-- 打开此行会导致ui无法显示，原因还没找到
    m_cmd_queue = mcu->alloc_command_queue();
}

MCU_digital_out::~MCU_digital_out()
{
}

MCU *MCU_digital_out::get_mcu()
{
    return m_mcu;
}

void MCU_digital_out::setup_max_duration(double max_duration)
{
    LOG_D("setup_max_duration %lf\n", max_duration);
    m_max_duration = max_duration;
}

void MCU_digital_out::setup_start_value(int start_value, int shutdown_value, bool is_static)
{
    if (is_static && start_value != shutdown_value)
    {
        std::cout << "Static pin can not have shutdown value" << std::endl;
    }
    m_start_value = (!!start_value) ^ m_invert;
    m_shutdown_value = (!!shutdown_value) ^ m_invert;
    m_is_static = is_static;
}

void MCU_digital_out::build_config(int para)
{
    if (m_is_static)
    {
        if (para & 1)
        {
            std::stringstream set_digital_out;
            set_digital_out << "set_digital_out pin=" << m_pin << " value=" << m_start_value;
            m_mcu->add_config_cmd(set_digital_out.str());
        }
        return;
    }
    if (para & 1)
    {
        m_mcu->request_move_queue_slot();
        m_oid = m_mcu->create_oid();
        std::stringstream config_digital_out;
        config_digital_out << "config_digital_out oid=" << m_oid
                           << " pin=" << m_pin
                           << " value=" << m_start_value
                           << " default_value=" << m_shutdown_value
                           << " max_duration=" << (uint32_t)(m_mcu->seconds_to_clock(m_max_duration));
        LOG_D("max_duration %lf\n", m_max_duration);
        LOG_D("freq : %lf\n", m_mcu->m_clocksync->m_mcu_freq);

        // printf("PWM %d %s\n", m_oid, config_digital_out.str().c_str());
        m_mcu->add_config_cmd(config_digital_out.str());
    }
    if (para & 4)
    {
        std::stringstream update_digital_out;
        update_digital_out << "update_digital_out oid=" << m_oid << " value=" << m_start_value;
        m_mcu->add_config_cmd(update_digital_out.str(), false, true);
    }
}

void MCU_digital_out::set_digital(double print_time, int value)
{
    uint64_t clock = m_mcu->print_time_to_clock(print_time); // 系统时钟转化为对应的MCU时钟
    uint64_t reqclock = clock;
    uint64_t minclock = m_last_clock;
    uint64_t on_ticks = !(!value) ^ m_invert;
    std::stringstream queue_digital_out;
    queue_digital_out << "queue_digital_out oid=" << m_oid << " clock=" << (uint32_t)clock << " on_ticks=" << on_ticks;
    m_mcu->m_serial->send(queue_digital_out.str(), minclock, reqclock, m_cmd_queue);
    m_last_clock = clock;
}

MCU_pwm::MCU_pwm(MCU *mcu, pinParams *params)
{
    m_mcu = mcu;
    m_hardware_pwm = false;
    m_cycle_time = 0.100;
    m_max_duration = 2.0;
    m_oid = 0;
    m_pin = m_mcu->m_serial->m_msgparser->m_pinMap[params->pin]; // m_pin = params->pin;
    m_invert = params->invert;
    m_start_value = m_invert;
    m_shutdown_value = m_invert;
    m_is_static = false;
    m_last_clock = m_last_cycle_ticks = 0;
    m_pwm_max = 0.0;
    m_mcu->register_config_callback(std::bind(&MCU_pwm::build_config, this, std::placeholders::_1));
    m_cmd_queue = m_mcu->alloc_command_queue();

    // printf("MCU_pwm m_pin %d params->pin %s\n", m_pin, params->pin.c_str());
}

MCU_pwm::~MCU_pwm()
{
}

MCU *MCU_pwm::get_mcu()
{
    return m_mcu;
}

void MCU_pwm::setup_max_duration(double max_duration)
{
    LOG_D("setup_max_duration %lf\n", max_duration);
    m_max_duration = max_duration;
}

void MCU_pwm::setup_cycle_time(double cycle_time, bool hardware_pwm)
{
    m_cycle_time = cycle_time;
    m_hardware_pwm = hardware_pwm;
}

void MCU_pwm::setup_start_value(double start_value, double shutdown_value, bool is_static)
{
    if (is_static && start_value != shutdown_value)
    {
        std::cout << "Static pin can not have shutdown value" << std::endl;
    }
    if (m_invert)
    {
        start_value = 1. - start_value;
        shutdown_value = 1. - shutdown_value;
    }
    m_start_value = std::max(0., std::min(1., start_value));
    m_shutdown_value = std::max(0., std::min(1., shutdown_value));
    m_is_static = is_static;
}

void MCU_pwm::build_config(int para)
{
    double curtime = get_monotonic();
    double printtime = m_mcu->estimated_print_time(curtime);
    m_last_clock = m_mcu->print_time_to_clock(printtime + 0.200);
    uint32_t cycle_ticks = m_mcu->seconds_to_clock(m_cycle_time);
#if 1
    if (m_hardware_pwm)
    {
        m_pwm_max = m_mcu->m_serial->get_constant_wapper()->pwm_max; // PWM_MAX
        if (m_is_static)
        {
            if (para & 1)
            {
                std::stringstream set_pwm_out;
                set_pwm_out << "set_pwm_out pin=" << m_pin << "cycle_ticks=" << cycle_ticks << "value=" << (uint32_t)(m_start_value * m_pwm_max);
                m_mcu->add_config_cmd(set_pwm_out.str());
            }
            return;
        }
        if (para & 1)
        {
            m_mcu->request_move_queue_slot();
            m_oid = m_mcu->create_oid();
            std::stringstream config_pwm_out;
            config_pwm_out << "config_pwm_out oid=" << m_oid << " pin=" << m_pin << " cycle_ticks=" << cycle_ticks << " value=" << (uint32_t)(m_start_value * m_pwm_max) << " default_value=" << (uint32_t)(m_shutdown_value * m_pwm_max) << " max_duration=" << (uint32_t)m_mcu->seconds_to_clock(m_max_duration);
            LOG_D("config_pwm_out %s\n", config_pwm_out.str().c_str());
            LOG_D("max_duration %lf\n", m_max_duration);
            m_mcu->add_config_cmd(config_pwm_out.str());
        }
        if (para & 2)
        {
            uint32_t svalue = (uint32_t)(m_start_value * m_pwm_max + 0.5);
            std::stringstream queue_pwm_out;
            queue_pwm_out << "queue_pwm_out oid=" << m_oid
                          << " clock=" << m_last_clock
                          << " value=" << svalue;
            m_mcu->add_config_cmd(queue_pwm_out.str(), true);
        }
        return;
    }
#endif

    if (m_is_static)
    {
        std::stringstream set_digital_out;
        set_digital_out << "set_digital_out pin=" << m_pin
                        << " value=" << m_start_value;
        return;
    }
    if (para & 1)
    {
        m_mcu->request_move_queue_slot();
        m_oid = m_mcu->create_oid();
        std::stringstream config_digital_out_config;
        config_digital_out_config << "config_digital_out oid=" << m_oid
                                  << " pin=" << m_pin
                                  << " value=" << m_start_value
                                  << " default_value=" << m_shutdown_value
                                  << " max_duration=" << (uint32_t)(m_mcu->seconds_to_clock(m_max_duration));
        m_mcu->add_config_cmd(config_digital_out_config.str());

        LOG_D("config_digital_out_config %s\n", config_digital_out_config.str().c_str());
        LOG_D("max_duration %lf\n", m_max_duration);

        std::stringstream set_digital_out_pwm_cycle;
        set_digital_out_pwm_cycle << "set_digital_out_pwm_cycle oid=" << m_oid
                                  << " clock_ticks=" << (uint32_t)cycle_ticks;
        m_mcu->add_config_cmd(set_digital_out_pwm_cycle.str());

        // printf("MCU_pwm::build_config %d para %d m_hardware_pwm %d m_is_static %d\n", m_oid, para, m_hardware_pwm, m_is_static);
    }
    if (para & 2)
    {
        m_last_cycle_ticks = cycle_ticks;
        uint32_t svalue = (uint32_t)(m_start_value * cycle_ticks + 0.5);
        std::stringstream queue_digital_out;
        queue_digital_out << "queue_digital_out oid=" << m_oid
                          << " clock=" << (uint32_t)m_last_clock
                          << " on_ticks=" << svalue;
        m_mcu->add_config_cmd(queue_digital_out.str(), true);
    }
}

void MCU_pwm::set_pwm(double print_time, double value, double cycle_time)
{
    uint64_t clock = m_mcu->print_time_to_clock(print_time);
    double t = (m_mcu->m_clocksync->estimated_print_time(get_monotonic()));
    uint64_t minclock = m_last_clock;
    m_last_clock = clock;
    if (m_invert)
    {
        value = 1.0 - value;
    }

    // 硬件PWM
    if (m_hardware_pwm)
    {
        uint32_t v = (uint32_t)(std::max(0.0, std::min(1.0, value)) * m_pwm_max + 0.5);
        std::stringstream queue_pwm_out;
        queue_pwm_out << "queue_pwm_out oid=" << m_oid << " clock=" << (uint32_t)clock << " value=" << v;
        m_mcu->m_serial->send(queue_pwm_out.str(), minclock, clock, m_cmd_queue);
        return;
    }

    if (cycle_time <= 1e-15)       //--IS_DOUBLE_ZERO----------
        cycle_time = m_cycle_time; // 0.1

    uint32_t cycle_ticks = (uint32_t)(m_mcu->seconds_to_clock(cycle_time));
    if (cycle_ticks != m_last_cycle_ticks)
    {
        std::stringstream set_digital_out_pwm_cycle;
        set_digital_out_pwm_cycle << "set_digital_out_pwm_cycle oid=" << m_oid << " clock=" << clock << " cycle_ticks=" << cycle_ticks;
        m_mcu->m_serial->send(set_digital_out_pwm_cycle.str(), minclock, clock, m_cmd_queue);
        m_last_cycle_ticks = cycle_ticks;
    }
    uint32_t on_ticks = (uint32_t)(std::max(0.0, std::min(1.0, value)) * (cycle_ticks) + 0.5);
    std::stringstream queue_digital_out;
    queue_digital_out << "queue_digital_out oid=" << m_oid << " clock=" << (uint32_t)clock << " on_ticks=" << on_ticks;

    // printf("set_pwm: %s\n", queue_digital_out.str().c_str());
    m_mcu->m_serial->send(queue_digital_out.str(), minclock, clock, m_cmd_queue);
    if (callback_function != nullptr)
    {
        callback_function(m_oid);
    }
}

MCU_adc::MCU_adc(MCU *mcu, pinParams *pin_params)
{
    m_mcu = mcu;
    m_pin = m_mcu->m_serial->m_msgparser->m_pinMap[pin_params->pin]; // m_pin = pin_params->pin;
    m_min_sample = m_max_sample = 0;
    m_sample_time = m_report_time = 0;
    m_sample_count = SAMPLE_TIME;
    m_range_check_count = 0;
    m_report_clock = 0;
    m_last_state.first = 0;
    m_last_state.second = 0;
    m_oid = 0;

    m_mcu->register_config_callback(std::bind(&MCU_adc::build_config, this, std::placeholders::_1));
}

MCU_adc::~MCU_adc()
{
}

MCU *MCU_adc::get_mcu()
{
    return m_mcu;
}

void MCU_adc::setup_minmax(double sample_time, int sample_count, double minval, double maxval, int range_check_count)
{
    m_sample_time = sample_time;
    m_sample_count = sample_count;
    m_min_sample = minval;
    m_max_sample = maxval;
    m_range_check_count = range_check_count;
}

void MCU_adc::setup_adc_callback(double report_time, std::function<void(double, double)> callback)
{
    m_report_time = report_time; // 0.8S
    m_callback = callback;
}

std::pair<double, double> MCU_adc::get_last_value()
{
    return m_last_state;
}

void MCU_adc::build_config(int para)
{
    std::cout.setf(std::ios::fixed, std::ios::floatfield); // 十进制计数法，不是科学计数法
    std::cout.precision(20);                               // 保留20位小数
    if (m_sample_count == 0)                               //--IS_DOUBLE_ZERO----------
        return;
    double mcu_adc_max = m_mcu->m_serial->get_constant_wapper()->adc_max; // ADC_MAX;
    double max_adc = m_sample_count * mcu_adc_max;
    if (para & 1)
    {
        m_oid = m_mcu->create_oid();
        std::stringstream query_analog_in_config;
        query_analog_in_config << "config_analog_in oid=" << m_oid << " pin=" << m_pin;
        m_mcu->add_config_cmd(query_analog_in_config.str());
        m_inv_max_adc = 1.0 / max_adc;
        m_report_clock = m_mcu->seconds_to_clock(m_report_time); // 0.8S
        m_mcu->m_serial->register_response(std::bind(&MCU_adc::handle_analog_in_state, this, std::placeholders::_1), "analog_in_state", m_oid);
    }

    if (para & 2)
    {
        uint64_t clock = m_mcu->get_query_slot(m_oid);
        uint32_t sample_ticks = m_mcu->seconds_to_clock(m_sample_time);
        uint16_t min_sample = (uint16_t)(std::max(0, std::min(0xffff, (int)(m_min_sample * max_adc))));
        uint16_t max_sample = (uint16_t)std::max(0, std::min(0xffff, (int)ceil(m_max_sample * max_adc)));

        std::stringstream query_analog_in;
        query_analog_in << "query_analog_in oid=" << m_oid << " clock=" << (uint32_t)clock << " sample_ticks=" << sample_ticks << " sample_count=" << m_sample_count << " rest_ticks=" << (uint32_t)m_report_clock
                        << " min_value=" << min_sample << " max_value=" << max_sample << " range_check_count=" << m_range_check_count;
        m_mcu->add_config_cmd(query_analog_in.str(), true);
    }
}

void MCU_adc::handle_analog_in_state(ParseResult &response_params) //-----2-adc_callback-G-G-2022-08--------
{
    double last_value = response_params.PT_uint32_outs.at("value") * m_inv_max_adc;
    uint32_t clock_32 = response_params.PT_uint32_outs.at("next_clock");
    uint64_t next_clock = m_mcu->clock32_to_clock64(response_params.PT_uint32_outs.at("next_clock")); //
    uint64_t last_read_clock = next_clock - m_report_clock;
    // LOG_D("m_mcu_freq %lf\n", m_mcu->m_clocksync->m_mcu_freq);
    // LOG_D("m_last_clock %llu\n", m_mcu->m_clocksync->m_last_clock);
    // double adjusted_offset = m_clock_adj[0];
    // double adjusted_freq = m_clock_adj[1];
    double last_read_time = m_mcu->clock_to_print_time(last_read_clock);
    m_last_state = {last_value, last_read_time};
    if (m_callback != nullptr)
        m_callback(last_read_time, last_value);
}