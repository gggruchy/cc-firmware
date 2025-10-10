#include "clocksync.h"
#include "Define.h"
#include "klippy.h"
#include "debug.h"
#include <unordered_set>
#define LOG_TAG "clocksync"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define CHIP_NAME "clocksync"
#define HANDLE_CLOCK_DEBUG 0
/* 复位原因 */
#define CAUSE_LOW_POWER                 (31)
#define CAUSE_WINDOWS_WATCHDOG          (30)
#define CAUSE_INDEPENDENT_WATCHDOG      (29)
#define CAUSE_SOFTWARE                  (28)
#define CAUSE_POR_PDR                   (27)
#define CAUSE_NRST                      (26)
#define CAUSE_BOR                       (25)
#define CAUSE_UNKOWN                    (0)


#include <linux/watchdog.h>

static void keep_alive(void);
int watchdog_init(bool state);
int watchdog_fd;

static double sent_time1 = 0;    

/*
 * This function simply sends an IOCTL to the driver, which in turn ticks
 * the PC Watchdog card to reset its internal timer so it doesn't trigger
 * a computer reset.
 */
void keep_alive(void)
{
    int dummy;
    ioctl(watchdog_fd, WDIOC_KEEPALIVE, &dummy);
}
/*
 * The main program. Run the program with "-d" to disable the card,
 * or "-e" to enable the card.
 */
int watchdog_init(bool state)
{
    int flags;
    watchdog_fd = open("/dev/watchdog", O_WRONLY);
    if (watchdog_fd == -1)
    {
        fprintf(stderr, "Watchdog device not enabled.\n");
        fflush(stderr);
        exit(-1);
    }
    int time_out = 10;
    ioctl(watchdog_fd, WDIOC_SETTIMEOUT, &time_out);
}

ClockSyncMain::ClockSyncMain()
{
    // m_get_clock_timer = Printer::GetInstance()->m_reactor->register_timer(std::bind(&ClockSyncMain::get_clock_event, this, std::placeholders::_1));
    m_queries_pending = 0;
    m_mcu_freq = 1; //
    m_last_clock = 0;
    m_clock_est = {0, 0, 0};
    // Minimum round-trip-time tracking
    m_min_half_rtt = 999999999.9;
    m_min_rtt_time = 0;
    // Linear regression of mcu clock and system sent_time
    m_time_avg = 0;
    m_time_variance = 0;
    m_clock_avg = 0;
    m_clock_covariance = 0;
    m_prediction_variance = 0;
    m_last_prediction_time = 0;
    m_clock_sync_flag = 0;
}

ClockSyncMain::~ClockSyncMain()
{
}

void ClockSyncMain::connect(Serialhdl *serial) //-8-MCU-G-G-2023-03-25--
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;
    m_serial = serial;
    m_mcu_freq = m_serial->get_constant_wapper()->clock_freq;
    ParseResult response_msg = m_serial->send_with_response("get_uptime", "uptime", m_serial->m_command_queue);
    if(response_msg.msg_name == "")
    {
        return;
    }
    m_last_clock = response_msg.PT_uint32_outs.at("high") << 32 | response_msg.PT_uint32_outs.at("clock");
    m_clock_avg = m_last_clock;
    m_time_avg = response_msg.sent_time;
    m_clock_est.time_avg = m_time_avg;
    m_clock_est.clock_avg = m_clock_avg;
    m_clock_est.mcu_freq = m_mcu_freq;
    m_prediction_variance = pow(0.001 * m_mcu_freq, 2);
    m_clock_restart_flag = false;
    for (int i = 0; i < 8; i++)
    {
        Printer::GetInstance()->m_reactor->pause(get_monotonic());
        usleep(50000);
        m_last_prediction_time = -9999.0f;
        ParseResult response_clock_msg = m_serial->send_with_response("get_clock", "clock", m_serial->m_command_queue);
        handle_clock(response_clock_msg);
    }
    m_serial->register_response(std::bind(&ClockSyncMain::handle_clock, this, std::placeholders::_1), "clock");
    m_serial->register_response(std::bind(&ClockSyncMain::handle_pong, this, std::placeholders::_1), "pong");
    m_serial->send("get_clock", minclock, reqclock, m_serial->m_command_queue);
    // Printer::GetInstance()->m_reactor->update_timer(m_get_clock_timer, Printer::GetInstance()->m_reactor->m_NOW);
    // std::cout << "ClockSync_connect  '" << m_mcu_freq  << std::endl;
}

void ClockSyncMain::connect_file(bool pace)
{
    m_mcu_freq = m_serial->get_constant_wapper()->clock_freq;
    m_clock_est = {0., 0, m_mcu_freq};
    double freq = 1000000000000.;
    if (pace)
        freq = m_mcu_freq;
    // m_serial.set_clock_est(freq,m_reactor.monotonic(), 0, 0); //---??---
}

// MCU clock querying (_handle_clock is invoked from background thread)
double ClockSync::get_clock_event(double eventtime)
{
    std::string cmd1 = "get_clock";
    m_serial->send(cmd1, 0, 0, m_serial->m_command_queue); //-1-5task-G-G---get_clock---
    m_queries_pending += 1;
    if(!is_active())
    {
        Printer::GetInstance()->set_serial_data_error_state(m_serial->m_name);
        return Printer::GetInstance()->m_reactor->m_NEVER;
    }
    // Use an unusual time for the next event so clock messages
    // don't resonate with other periodic events.
    return eventtime + 0.9839;
}

void *clockPthreadCb(void *arg) //-5task-G-G-2022-08-11-get_clock---
{
    int minclock = 0;
    int reqclock = 0;
    // Serialhdl *m_serial = (Serialhdl *)arg;
    ClockSync *m_clocksync = (ClockSync *)arg;
    std::unordered_set<std::string> error_names; // 添加集合来跟踪已经处理过的name
    while (1)
    {
        if (Printer::GetInstance()->m_mcu->m_clocksync->m_clock_restart_flag)
            break;
        if (!Printer::GetInstance()->m_mcu->m_clocksync->m_clock_sync_flag)
        {
            std::string cmd1 = "get_clock";
            m_clocksync->m_serial->send(cmd1, minclock, reqclock, m_clocksync->m_serial->m_command_queue); //-1-5task-G-G---get_clock---
            m_clocksync->m_queries_pending++;
            if(!m_clocksync->is_active())
            {
                std::string name = m_clocksync->m_serial->m_name;
                if (error_names.find(name) == error_names.end())
                {
                    Printer::GetInstance()->set_serial_data_error_state(name);
                    error_names.insert(name); // 将name添加到集合中
                }
                // Printer::GetInstance()->m_mcu->m_clocksync->m_clock_sync_flag = 1;
            }
        }
        // std::cout << "m_serial m_name is " << m_clocksync->m_serial->m_name << "，m_clocksync m_queries_pending is " << m_clocksync->m_queries_pending << std::endl;
        usleep(983900);                                                      // 1s
        // if (m_clocksync->m_serial->m_name == "mcu")
        //     keep_alive();
    }
}

void ClockSync::creat_get_clock_pthread() //-5task-G-G-
{
    // if (m_serial->m_name == "mcu")
    //     watchdog_init(true);
    pthread_create(&clock_tid, NULL, clockPthreadCb, this); //-5task-G-G-2022-08-11--get_clock-
}
void ClockSync::handle_pong(ParseResult &result_msg)
{
    std::string response_str = result_msg.PT_string_outs["data"];
    uint32_t data_count = response_str.length();
    uint8_t *response_data = (uint8_t *)response_str.c_str();

    std::cout << "data_count: " << data_count << " data: " << std::endl;
    for (int i = 0; i < data_count; i++)
    {
        GAM_DEBUG_send("-%d", response_data[i]);
    }
    std::cout << ":data" << std::endl;
}

void ClockSync::handle_clock(ParseResult &result_clock)
{
    m_queries_pending = 0;
    // 扩展时钟到 64 位
    uint64_t last_clock = m_last_clock;
    int64_t temp = 0xffffffff;
    uint64_t clock = (last_clock & ~temp) | result_clock.PT_uint32_outs.at("clock");

    uint64_t temp_add = 0x100000000;
    if (clock < last_clock)
    {
        clock += temp_add;
    }
    m_last_clock = clock;
    // LOG_D("handle_clock last_clock : %llu\n", m_last_clock);
    // 检查这是否是迄今为止看到的最佳往返时间
    double sent_time = result_clock.sent_time;

    if (sent_time <= 1e-15) //--IS_DOUBLE_ZERO----------
    {
        LOG_E("sent_time is zero %s\n", m_serial->m_name.c_str());
        return;
    }

    double receive_time = result_clock.receive_time;
    double half_rtt = 0.5f * (receive_time - sent_time); // 半往返时间
    double aged_rtt = (sent_time - m_min_rtt_time) * RTT_AGE;
    // LOG_I("half_rtt %lf aged_rtt %lf\n", half_rtt, aged_rtt);
    // LOG_I("m_min_half_rtt %lf m_min_rtt_time %lf\n", m_min_half_rtt, m_min_rtt_time);
    if (half_rtt < m_min_half_rtt + aged_rtt)
    {
        m_min_half_rtt = half_rtt;
        m_min_rtt_time = sent_time;
        LOG_I("new minimum rtt %.3f: hrtt=%.6f freq=%lf %s\n",
              sent_time, half_rtt, m_clock_est.mcu_freq, m_serial->m_name.c_str());
    }

    // 过滤掉极端异常值的样本
    double exp_clock = ((sent_time - m_time_avg) * m_clock_est.mcu_freq + m_clock_avg);
    double clock_diff2 = pow((clock - exp_clock), 2);
    if (clock_diff2 > 10.0f * m_prediction_variance && clock_diff2 > pow((0.000100f * m_mcu_freq), 2))
    {
        if (clock > exp_clock && sent_time < m_last_prediction_time + 10.0f)
        {
            LOG_W("Ignoring clock sample sent_time:%.3f:"
                  " freq=%lf diff=%f stddev=%.3f %s\n",
                  sent_time, m_clock_est.mcu_freq, clock - exp_clock,
                  sqrt(m_prediction_variance), m_serial->m_name.c_str());
            return;
        }
        LOG_I("Resetting prediction variance %.3f:"
              " freq=%lf diff=%d stddev=%.3f %s\n",
              sent_time, m_clock_est.mcu_freq, clock - exp_clock,
              sqrt(m_prediction_variance), m_serial->m_name.c_str());
        m_prediction_variance = pow((0.001f * m_mcu_freq), 2);
    }
    else
    {
        m_last_prediction_time = sent_time;
        m_prediction_variance = ((1.0f - DECAY) * (m_prediction_variance + clock_diff2 * DECAY));
    }
    // 将时钟和 sent_time 添加到线性回归
    double diff_sent_time = sent_time - m_time_avg; // DECAY * diff_sent_time 趋近于发送时间间隔 s   diff_sent_time 趋近于29.5
    m_time_avg += DECAY * diff_sent_time;           // 发送时间-29.5
    m_time_variance = (1.0 - DECAY) * (m_time_variance + pow(diff_sent_time, 2) * DECAY);
    uint64_t diff_clock = clock - m_clock_avg; // DECAY * diff_clock 趋近于发送时间间隔   diff_clock 趋近于29.5*mcu_freq
    m_clock_avg += DECAY * diff_clock;
    m_clock_covariance = (1. - DECAY) * (m_clock_covariance + diff_sent_time * diff_clock * DECAY);
    // 从线性回归更新预测
    double new_freq = m_clock_covariance / m_time_variance;
    // LOG_I("new_freq %lf\n", new_freq);
    double pred_stddev = sqrt(m_prediction_variance);

    serialqueue_set_clock_est(m_serial->m_serialqueue, new_freq, m_time_avg + TRANSMIT_EXTRA, (int64_t)(m_clock_avg - 3. * pred_stddev), clock);

    m_clock_est.time_avg = m_time_avg + m_min_half_rtt; // 采样时间 s
    m_clock_est.clock_avg = m_clock_avg;                // 采样时间对应时钟 clk
    m_clock_est.mcu_freq = new_freq;                    // MCU校准后频率 clk
#if HANDLE_CLOCK_DEBUG
    // if (m_serial->mcu_type == 2)
    if(m_serial->m_name == "stm32" || m_serial->m_name == "strain_gauge_mcu")  //喷头板消息到来后开启led
    {
        static uint64_t recv_cnt = 0;
        LOG_D("%s %llu : UART 64 time:%lf clock raw : %u\n", m_serial->m_name.c_str(), recv_cnt, clock_to_print_time(clock),result_clock.PT_uint32_outs.at("clock")); 
        LOG_D(" estimated_print_time:%lf\n",estimated_print_time(get_monotonic())); 
        recv_cnt++;
    }
    else
    {
        static uint64_t recv_cnt = 0;
        LOG_D("DSP 64 time:%lf clock raw:%u\n",clock_to_print_time(clock),result_clock.PT_uint32_outs.at("clock"));  
        recv_cnt++;
    }
#endif
}

void ClockSync::_handle_extruder_bootup_info(ParseResult params)
{
    uint32_t oid = params.PT_uint32_outs.at("oid");
    m_stack_data.crash_flag = params.PT_uint32_outs.at("crash_flag");
    m_stack_data.rest_cause = params.PT_uint32_outs.at("rest_cause"); 
    m_stack_data.R0 = params.PT_uint32_outs.at("R0"); 
    m_stack_data.R1 = params.PT_uint32_outs.at("R1"); 
    m_stack_data.R2 = params.PT_uint32_outs.at("R2"); 
    m_stack_data.R3 = params.PT_uint32_outs.at("R3"); 
    m_stack_data.R12 = params.PT_uint32_outs.at("R12"); 
    m_stack_data.LR = params.PT_uint32_outs.at("LR"); 
    m_stack_data.PC = params.PT_uint32_outs.at("PC"); 
    m_stack_data.xPSR = params.PT_uint32_outs.at("xPSR");
    char *reset_msg = NULL;
    switch (m_stack_data.rest_cause)
    {
        case CAUSE_LOW_POWER:
            reset_msg = "Low-power";
            break;
        case CAUSE_WINDOWS_WATCHDOG:
            reset_msg = "Window watchdog";
            break;
        case CAUSE_INDEPENDENT_WATCHDOG:
            reset_msg = "Independent watchdog";
            break;
        case CAUSE_SOFTWARE:
            reset_msg = "Software";
            break;
        case CAUSE_POR_PDR:
            reset_msg = "POR/PDR";
            break;
        case CAUSE_NRST:
            reset_msg = "NRST";
            break; 
        default:
            reset_msg = "UNKOWN";
            break;
    }
    if (!(m_stack_data.crash_flag&0x01)) {
        LOG_E("reboot index:%d rest_cause: %s (%d) crash_flag:%d\n", (uint32_t)((m_stack_data.crash_flag&0xFE)>>1), reset_msg, (uint32_t)m_stack_data.rest_cause, (uint32_t)(m_stack_data.crash_flag&0x01));
        LOG_E("crash info\n R0=0x%08X\n R1=0x%08X\n R2=0x%08X\n R3=0x%08X\n R12=0x%08X\n LR=0x%08X\n PC=0x%08X\n xPSR=0x%08X\n\n", (uint32_t)m_stack_data.R0, (uint32_t)m_stack_data.R1, (uint32_t)m_stack_data.R2, (uint32_t)m_stack_data.R3, (uint32_t)m_stack_data.R12, (uint32_t)m_stack_data.LR, (uint32_t)m_stack_data.PC, (uint32_t)m_stack_data.xPSR);
    } else {
        LOG_D("reboot index:%d rest_cause: %s (%d) crash_flag:%d\n", (uint32_t)((m_stack_data.crash_flag&0xFE)>>1), reset_msg, (uint32_t)m_stack_data.rest_cause, (uint32_t)(m_stack_data.crash_flag&0x01));
        LOG_D("crash info\n R0=0x%08X\n R1=0x%08X\n R2=0x%08X\n R3=0x%08X\n R12=0x%08X\n LR=0x%08X\n PC=0x%08X\n xPSR=0x%08X\n\n", (uint32_t)m_stack_data.R0, (uint32_t)m_stack_data.R1, (uint32_t)m_stack_data.R2, (uint32_t)m_stack_data.R3, (uint32_t)m_stack_data.R12, (uint32_t)m_stack_data.LR, (uint32_t)m_stack_data.PC, (uint32_t)m_stack_data.xPSR);
    } 
}

uint64_t ClockSyncMain::print_time_to_clock(double print_time)
{
    uint64_t ret = (uint64_t)(print_time * m_mcu_freq);
    return ret;
}

double ClockSyncMain::clock_to_print_time(uint64_t clock) // MCU计数器计数值->MCU计数器计数时间 s
{
    double clock_to_print = clock / m_mcu_freq;
    // std::cout << "clock_to_print: " << clock_to_print  << "，clock:" << clock << std::endl;
    return clock_to_print;
}

// double ClockSyncMain::get_mcu_freq(void) // MCU计数器计数值->MCU计数器计数时间 s
// {
//     double clock_to_print = clock / m_mcu_freq;
//     // std::cout << "clock_to_print: " << clock_to_print  << "，clock:" << clock << std::endl;
//     return clock_to_print;
// }

uint64_t ClockSync::get_clock(double eventtime) // 计算预估MCU计数器当前计数值 单位clk
{
    double sample_time = m_clock_est.time_avg;                            // 采样时间 s
    uint64_t clock = m_clock_est.clock_avg;                               // 采样时间对应时钟 clk
    double freq = m_clock_est.mcu_freq;                                   // MCU校准后频率 clk
    uint64_t temp = (uint64_t)(clock + (eventtime - sample_time) * freq); //(clock-29.5*freq + (eventtime - (sent_time-29.5)) * freq) = (clock + (eventtime - (sent_time)) * freq)
    return temp;
}

double ClockSync::estimate_clock_systime(uint64_t reqclock)
{
    double sample_time = m_clock_est.time_avg;
    uint64_t clock = m_clock_est.clock_avg;
    double freq = m_clock_est.mcu_freq;
    return (double)((reqclock - clock) / freq) + sample_time;
}
/**
 * @brief 根据上一次发送时间和当前时间预估当前MCU的CLOCK，再根据MCU的频率计算当前MCU的打印时间
 *
 * @param eventtime
 * @return double
 */
double ClockSync::estimated_print_time(double eventtime) // 计算预估MCU计数器当前计数时间 s
{
    uint64_t clock = get_clock(eventtime);
    // printf("%s eventtime:%f clock:%ld\n", m_serial->m_name.c_str(),eventtime,clock);
    return clock_to_print_time(clock);
}

uint64_t ClockSync::clock32_to_clock64(uint32_t clock_32)
{
    uint64_t ret;
    uint64_t last_clock = m_last_clock;
    uint64_t clock_diff = (last_clock - (uint64_t)clock_32) & 0xffffffff;
    if (clock_diff & 0x80000000)
    {
        ret = last_clock + 0x100000000 - clock_diff;
        return ret;
    }
    ret = last_clock - clock_diff;
    return ret;
}

bool ClockSync::is_active()
{
    return m_queries_pending <= 10;
}

std::string ClockSyncMain::dump_debug()
{
    double sample_time = m_clock_est.time_avg;
    uint64_t clock = m_clock_est.clock_avg;
    double freq = m_clock_est.mcu_freq;
    std::stringstream ret;
    ret << "clocksync state: mcu_freq=" << m_mcu_freq << " last_clock=" << m_last_clock << " clock_est=" << sample_time << " " << clock << " " << freq << " min_half_rtt=" << m_min_half_rtt << " min_rtt_time=" << m_min_rtt_time << " time_avg=" << m_time_avg << m_time_variance << " clock_avg=" << m_clock_avg << m_clock_covariance << " pred_variance=" << m_prediction_variance;
    return ret.str();
}

std::string ClockSyncMain::stats(double eventtime)
{
    double sample_time = m_clock_est.time_avg;
    uint64_t clock = m_clock_est.clock_avg;
    double freq = m_clock_est.mcu_freq;
    std::stringstream ret;
    // ret << "freq=" << freq;
    return ret.str();
}

std::vector<double> ClockSyncMain::calibrate_clock(double print_time, double eventtime)
{
    std::vector<double> ret = {0., m_mcu_freq};
    return ret;
}

SecondarySync::SecondarySync(ClockSyncMain *main_sync) : ClockSync()
{
    // m_get_clock_timer = Printer::GetInstance()->m_reactor->register_timer(std::bind(&SecondarySync::get_clock_event, this, std::placeholders::_1));
    m_main_sync = main_sync;
    m_clock_adj = {0., 1.};
    m_last_sync_time = 0.;

    m_queries_pending = 0;
    m_mcu_freq = 1; //
    m_last_clock = 0;
    m_clock_est = {0, 0, 0};
    // Minimum round-trip-time tracking
    m_min_half_rtt = 999999999.9;
    m_min_rtt_time = 0;
    // Linear regression of mcu clock and system sent_time
    m_time_avg = 0;
    m_time_variance = 0;
    m_clock_avg = 0;
    m_clock_covariance = 0;
    m_prediction_variance = 0;
    m_last_prediction_time = 0;
    m_clock_sync_flag = 0;
}

SecondarySync::~SecondarySync()
{
}

void SecondarySync::connect_main(Serialhdl *serial) //-8-MCU-G-G-2023-03-25--
{
    uint64_t minclock = 0;
    uint64_t reqclock = 0;
    m_serial = serial;
    m_mcu_freq = m_serial->get_constant_wapper()->clock_freq;
    for (int i = 0; i < 8; i++) // 如果太快get_clock，handle_clock算出来的主频可能会有问题。原因尚未明确
    {
        Printer::GetInstance()->m_reactor->pause(get_monotonic());
        usleep(50000);
        ParseResult response_msg = m_serial->send_with_response("get_uptime", "uptime", m_serial->m_command_queue);
        if(response_msg.msg_name == "")
        {
            return;
        }
        m_last_clock = response_msg.PT_uint32_outs.at("high") << 32 | response_msg.PT_uint32_outs.at("clock");
        m_clock_avg = m_last_clock;
        m_time_avg = response_msg.sent_time;
    }
    m_clock_est.time_avg = m_time_avg;
    m_clock_est.clock_avg = m_clock_avg;
    m_clock_est.mcu_freq = m_mcu_freq;
    m_prediction_variance = pow(0.001 * m_mcu_freq, 2);
    m_serial->register_response(std::bind(&ClockSyncMain::_handle_extruder_bootup_info, this, std::placeholders::_1), "extruder_bootup_info");
    for (int i = 0; i < 8; i++)
    {
        Printer::GetInstance()->m_reactor->pause(get_monotonic());
        usleep(50000);
        m_last_prediction_time = -9999.0f;
        ParseResult response_clock_msg = m_serial->send_with_response("get_clock", "clock", m_serial->m_command_queue);
        // LOG_I("response_clock_msg.sent_time %lf\n", response_clock_msg.sent_time);
        // LOG_I("response_clock_msg.receive_time %lf\n", response_clock_msg.receive_time);
        // LOG_I("response_clock_msg.PT_uint32_outs.at(clock) %u\n", response_clock_msg.PT_uint32_outs.at("clock"));
        handle_clock(response_clock_msg);
    }
    m_serial->register_response(std::bind(&SecondarySync::handle_clock, this, std::placeholders::_1), "clock");
    m_serial->register_response(std::bind(&SecondarySync::handle_pong, this, std::placeholders::_1), "pong");
    m_serial->send("get_clock", minclock, reqclock, m_serial->m_command_queue);
    // Printer::GetInstance()->m_reactor->update_timer(m_get_clock_timer, Printer::GetInstance()->m_reactor->m_NOW);
}
void SecondarySync::connect(Serialhdl *serial) //-8-MCU-G-G-2023-03-25--
{
    connect_main(serial);

    m_clock_adj = {0., m_mcu_freq};
    double curtime = get_monotonic();
    double main_print_time = m_main_sync->estimated_print_time(curtime);
    double local_print_time = estimated_print_time(curtime);
    m_clock_adj = {main_print_time - local_print_time, m_mcu_freq};
    this->calibrate_clock(0., curtime);
}

void SecondarySync::connect_file_main(bool pace)
{
    m_mcu_freq = m_serial->get_constant_wapper()->clock_freq;
    m_clock_est = {0., 0, m_mcu_freq};
    double freq = 1000000000000.;
    if (pace)
        freq = m_mcu_freq;
    // m_serial.set_clock_est(freq,m_reactor.monotonic(), 0, 0); //---??---
}

void SecondarySync::connect_file(bool pace)
{
    connect_file_main(pace);
    m_clock_adj = {0., m_mcu_freq};
}

// clock frequency conversions
uint64_t SecondarySync::print_time_to_clock(double print_time) // 主MCU打印时间->本MCU打印时钟
{
    double adjusted_offset = m_clock_adj[0];
    double adjusted_freq = m_clock_adj[1];
    // LOG_I("print_time_to_clock: print_time=%lf adjusted_offset=%lf adjusted_freq=%lf\n", print_time, adjusted_offset, adjusted_freq);
    return (uint64_t)((print_time - adjusted_offset) * adjusted_freq);
}

double SecondarySync::clock_to_print_time(uint64_t clock)
{
    double adjusted_offset = m_clock_adj[0];
    double adjusted_freq = m_clock_adj[1];
    // LOG_I("clock_to_print_time: clock=%llu adjusted_offset=%lf adjusted_freq=%lf\n", clock, adjusted_offset, adjusted_freq);
    return clock / adjusted_freq + adjusted_offset;
}

std::string SecondarySync::dump_debug_main()
{
    double sample_time = m_clock_est.time_avg;
    uint64_t clock = m_clock_est.clock_avg;
    double freq = m_clock_est.mcu_freq;
    std::stringstream ret;
    ret << "clocksync state: mcu_freq=" << m_mcu_freq << " last_clock=" << m_last_clock << " clock_est=" << sample_time << " " << clock << " " << freq << " min_half_rtt=" << m_min_half_rtt << " min_rtt_time=" << m_min_rtt_time << " time_avg=" << m_time_avg << m_time_variance << " clock_avg=" << m_clock_avg << m_clock_covariance << " pred_variance=" << m_prediction_variance;
    return ret.str();
}

std::string SecondarySync::stats_main(double eventtime)
{
    double sample_time = m_clock_est.time_avg;
    uint64_t clock = m_clock_est.clock_avg;
    double freq = m_clock_est.mcu_freq;
    std::stringstream ret;
    // ret << "freq=" << freq;
    return ret.str();
}
// misc commands
std::string SecondarySync::dump_debug()
{
    double adjusted_offset = m_clock_adj[0];
    double adjusted_freq = m_clock_adj[1];
    std::string dump = dump_debug_main();
    std::stringstream ret;
    ret << dump << " clock_adj=" << adjusted_offset << " " << adjusted_freq;
    return ret.str();
}
std::string SecondarySync::stats(double eventtime)
{
    double adjusted_offset = m_clock_adj[0];
    double adjusted_freq = m_clock_adj[1];
    std::string state = stats_main(eventtime);
    std::stringstream ret;
    ret << state << " adj=" << adjusted_freq;
    return ret.str();
}
/**
 * @brief 利用发送时间等信息进行MCU时钟校准
 *
 * @param print_time
 * @param eventtime
 * @return std::vector<double>
 */
std::vector<double> SecondarySync::calibrate_clock(double print_time, double eventtime)
{
    // Calculate: est_print_time = main_sync.estimatated_print_time()
    double ser_time = m_main_sync->m_clock_est.time_avg; // 上一次主MCU发送时间，上位机系统时间
    uint64_t ser_clock = m_main_sync->m_clock_est.clock_avg;
    double ser_freq = m_main_sync->m_clock_est.mcu_freq;
    double main_mcu_freq = m_main_sync->m_mcu_freq;
    double est_main_clock = (eventtime - ser_time) * ser_freq + ser_clock; // 上位机当前时间-主MCU上一次发送时间*主MCU频率+主MCU上一次发送时钟->预测主MCU当前时钟
    double est_print_time = est_main_clock / main_mcu_freq;                // 预测主MCU当前时钟/主MCU频率->预测主MCU当前打印时间
    // Determine sync1_print_time and sync2_print_time
    double sync1_print_time = std::max(print_time, est_print_time); // 打印时间和预估时间的最大值
    double sync2_print_time = std::max(sync1_print_time + 4., std::max(m_last_sync_time, print_time + 2.5 * (print_time - est_print_time)));
    // Calc sync2_sys_time (inverse of main_sync.estimatated_print_time)
    double sync2_main_clock = sync2_print_time * main_mcu_freq;                   //
    double sync2_sys_time = ser_time + (sync2_main_clock - ser_clock) / ser_freq; //
    // Adjust freq so estimated print_time will match at sync2_print_time
    uint64_t sync1_clock = print_time_to_clock(sync1_print_time);
    uint64_t sync2_clock = get_clock(sync2_sys_time);                                             // 如果直接用 print_time_to_clock(sync2_print_time);求得clock，就没利用上副MCU的发送时间(系统时间)和发送时钟(MCU时钟)
    double adjusted_freq = ((sync2_clock - sync1_clock) / (sync2_print_time - sync1_print_time)); // 利用CLOCK和打印时间计算频率。
    double adjusted_offset = sync1_print_time - (sync1_clock / adjusted_freq);
    // Apply new values
    m_clock_adj = {adjusted_offset, adjusted_freq};
    m_last_sync_time = sync2_print_time; // 校准时主CPU对应的时间
    // LOG_I("adjusted_offset = %lf adjusted_freq = %lf\n", adjusted_offset, adjusted_freq);
    LOG_D("%s calibrate_clock: print_time=%lf eventtime=%lf ser_time=%lf ser_clock=%llu ser_freq=%lf main_mcu_freq=%lf est_main_clock=%lf est_print_time=%lf sync1_print_time=%lf sync2_print_time=%lf sync2_main_clock=%lf sync2_sys_time=%lf sync1_clock=%llu sync2_clock=%llu adjusted_freq=%lf adjusted_offset=%lf\n", 
    m_serial->m_name.c_str(),print_time, eventtime, ser_time, ser_clock, ser_freq, main_mcu_freq, est_main_clock, est_print_time, sync1_print_time, sync2_print_time, sync2_main_clock, sync2_sys_time, sync1_clock, sync2_clock, adjusted_freq, adjusted_offset);
    // std::cout << "adjusted_offset  '" << adjusted_offset << "' adjusted_freq=" << adjusted_freq << std::endl;
    return m_clock_adj;
}
