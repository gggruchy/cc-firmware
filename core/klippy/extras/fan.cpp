#include "fan.h"
#include "klippy.h"
#include "Define.h"
#include "my_string.h"

#include "simplebus.h"
#include "srv_state.h"
#define LOG_TAG "FAN"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

Fan::Fan(std::string section_name, double default_shutdown_speed)
{
    m_last_fan_value = 0.;
    m_last_fan_time = 0.;
    m_name = split(section_name, " ").back();
    if (m_name == "model")
        m_id = FAN_ID_MODEL;
    else if (m_name == "model_helper_fan")
        m_id = FAN_ID_MODEL_HELPER;
    else if (m_name == "box_fan")
        m_id = FAN_ID_BOX;
    // read config
    m_max_power = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_power", 1, 0., 1.);
    m_kick_start_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "kick_start_time", 0.1, 0.);
    m_off_below = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "off_below", 0., 0., 1.);
    double cycle_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "cycle_time", 0.010, DBL_MIN, DBL_MAX, 0.); // 100HZ,周期时间
    bool hardware_pwm = Printer::GetInstance()->m_pconfig->GetBool(section_name, "hardware_pwm", false);
    double shutdown_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "shutdown_speed", default_shutdown_speed, 0., 1.);
    m_startup_voltage = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "startup_voltage", 0., 0., 1.);

    // setup pwm object
    // 创建PWM
    // printf("[0]fan and pin %s %s\n", m_name.c_str(), Printer::GetInstance()->m_pconfig->GetString(section_name, "pin").c_str());
    m_mcu_fan = (MCU_pwm *)Printer::GetInstance()->m_ppins->setup_pin("pwm", Printer::GetInstance()->m_pconfig->GetString(section_name, "pin"));
    // printf("-----------------------------------\n");
    m_mcu_fan->setup_max_duration(0.);
    m_mcu_fan->setup_cycle_time(cycle_time, hardware_pwm);
    double shutdown_power = std::max(0., std::min(m_max_power, shutdown_speed));
    m_mcu_fan->setup_start_value(0., shutdown_power);
    m_mcu_fan->callback_function = std::bind(&Fan::fan_pwm_value_setting, this, std::placeholders::_1);

    // 转速测量
    //---??---
    // Setup tachometer
    m_tachometer = new FanTachometer(section_name);
    if (Printer::GetInstance()->m_pconfig->GetString(section_name, "tachometer_pin", "") != "")
    {
#if !ENABLE_MANUTEST
        Printer::GetInstance()->m_reactor->register_timer("fan_check_timer", std::bind(&Fan::check_event, this, std::placeholders::_1), get_monotonic());
#endif
    }
    m_current_speed = 0.;
    m_fan_min_time = 0.1;
    m_retry_times = 0;
    // Register callbacks ---??---
    Printer::GetInstance()->register_event_double_handler("gcode:request_restart:Fan" + section_name, std::bind(&Fan::handle_request_restart, this, std::placeholders::_1));
}

Fan::~Fan()
{
    if (m_mcu_fan != nullptr)
    {
        delete m_mcu_fan;
    }
}

MCU *Fan::get_mcu()
{
    return m_mcu_fan->get_mcu();
}

void Fan::fan_pwm_value_setting(int oid)
{
    if(oid == m_mcu_fan->m_oid)
    {
        m_last_fan_value = fan_value;
        // LOG_I("m_last_fan_value oid is %d, time is %1.lf\n", oid, get_monotonic());
    }
}

void Fan::set_speed(double print_time, double value)
{
    value = m_current_speed;
    if (fabs(m_startup_voltage) > 1e-15)
    {
        if (fabs(value) < 1e-15)
        {
            value = 0.;
        }
        else
        {
            value = (1 - m_startup_voltage) * value + m_startup_voltage;
        }
    }
    if (value < m_off_below)
        value = 0;

    value = std::max(0.0, std::min(m_max_power, value * m_max_power));
    fan_value = value;
    if (value == m_last_fan_value)
        return;

    print_time = std::max(m_last_fan_time + m_fan_min_time, print_time);

    if (value && value < m_max_power && m_kick_start_time &&
        (!m_last_fan_value || (value - m_last_fan_value > 0.5)))
    {
        m_mcu_fan->set_pwm(print_time, m_max_power);
        print_time += m_kick_start_time;
    }

    // printf("[2] fan_name %s fan_id %d value %lf  pwm_oid %d\n", m_name.c_str(), m_id, value, m_mcu_fan->m_oid);
    m_mcu_fan->set_pwm(print_time, value);
    m_last_fan_time = print_time;
    // m_last_fan_value = value;
    return;
}

void Fan::set_speed_from_command(double value)
{
    // double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    m_current_speed = value;

    // 上报数据
    srv_state_fan_msg_t fan_msg;
    fan_msg.fan_id = m_id;
    fan_msg.value = value;
    simple_bus_publish_async("fan", SRV_FAN_MSG_ID_STATE, &fan_msg, sizeof(fan_msg));
    // printf("[1] fan_name %s fan_id %d value %lf\n", m_name.c_str(), m_id, value);
    Printer::GetInstance()->m_tool_head->register_lookahead_callback(std::bind(&Fan::set_speed, this, std::placeholders::_1, std::placeholders::_2));
}

void Fan::handle_request_restart(double print_time)
{
    m_current_speed = 0.;
    set_speed(print_time, 0.);
    return;
}

struct fan_state Fan::get_status(double eventtime)
{
    double rpm = m_tachometer->get_status(eventtime);
    struct fan_state status;
    status.speed = m_last_fan_value;
    status.rpm = rpm;
    return status;
}

double Fan::check_event(double eventtime)
{
    fan_state state = get_status(eventtime);
    // std::cout << "m_name : " << m_name << " rpm : " << state.rpm << " speed : " << state.speed << std::endl;
    if (fabs(state.rpm) < 1e-15 && state.speed > 0.)
    {
        m_retry_times++;
        if (m_retry_times >= 10)
        {
            if (strcmp(m_name.c_str(), "model") == 0){
                fan_state_callback_call(FAN_STATE_MODEL_ERROR);
                LOG_I("error_fan oid is %d, time is %1.lf\n", m_mcu_fan->m_oid, get_monotonic());
            }
            else if (strcmp(m_name.c_str(), "board_cooling_fan") == 0){
                fan_state_callback_call(FAN_STATE_BOARD_COOLING_FAN_ERROR);
            }
            else if (strcmp(m_name.c_str(), "hotend_cooling_fan") == 0){
                fan_state_callback_call(FAN_STATE_HOTEND_COOLING_FAN_ERROR);
            }
            LOG_E("Fan %s is not working\n", m_name.c_str());
            if(is_fan_state_arrayempty() != false)
                return Printer::GetInstance()->m_reactor->m_NEVER;
        }
    }
    else
    {
        m_retry_times = 0;
    }
    return eventtime + 2.;
}

FanTachometer::FanTachometer(std::string section_name)
{
    std::string pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "tachometer_pin", "");
    if (pin != "")
    {
        m_ppr = Printer::GetInstance()->m_pconfig->GetInt(section_name, "tachometer_ppr", 2, 1);
        double poll_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "tachometer_poll_interval", 0.0015, DBL_MIN, DBL_MAX, 0.);
        double sample_time = 1.;
        m_freq_counter = new FrequencyCounter(pin, sample_time, poll_time);
    }
}

FanTachometer::~FanTachometer()
{
}

double FanTachometer::get_status(double eventtime)
{
    double rpm = -1;
    if (m_freq_counter)
    {
        rpm = m_freq_counter->get_frequency() * 30. / m_ppr;
    }
    return rpm;
}

PrinterFan::PrinterFan(std::string section_name)
{
    m_fan = new Fan(section_name);
    if (m_fan->m_name == "model")
    {
        Printer::GetInstance()->m_gcode->register_command("M106", std::bind(&PrinterFan::cmd_M106, this, std::placeholders::_1));
        Printer::GetInstance()->m_gcode->register_command("M107", std::bind(&PrinterFan::cmd_M107, this, std::placeholders::_1));
        Printer::GetInstance()->m_gcode->register_command("SET_FAN_SPEED", std::bind(&PrinterFan::cmd_SET_FAN_SPEED, this, std::placeholders::_1));
    }
    else if (m_fan->m_name == "model_helper_fan")
    {
        fan_silence_speed = Printer::GetInstance()->m_pconfig->GetInt(section_name, "silence", 50);
        fan_normal_speed = Printer::GetInstance()->m_pconfig->GetInt(section_name, "normal", 80);
        fan_crazy_speed = Printer::GetInstance()->m_pconfig->GetInt(section_name, "crazy", 100);
    }
}

PrinterFan::~PrinterFan()
{
    if (m_fan != nullptr)
    {
        delete m_fan;
        m_fan = nullptr;
    }
}

struct fan_state PrinterFan::get_status(double eventtime)
{
    return m_fan->get_status(eventtime);
}

double Fan::get_startup_voltage(void)
{
    return m_startup_voltage;
}

// M106与M107仅用于模型风扇
void PrinterFan::cmd_M106(GCodeCommand &gcmd)
{
    int fan_id = gcmd.get_int("P", 1, 0, 100);
    double value = gcmd.get_double("S", 255., 0., 255.) / 255.;
    fan_id -= 1;
    if (Printer::GetInstance()->m_printer_fans[fan_id] != nullptr)
    {
        Printer::GetInstance()->m_printer_fans[fan_id]->m_fan->set_speed_from_command(value);
    }

}

void PrinterFan::cmd_M107(GCodeCommand &gcmd)
{
    if (m_fan->m_name != "model")
        return;
    m_fan->set_speed_from_command(0.);
}

void PrinterFan::cmd_SET_FAN_SPEED(GCodeCommand &gcmd)
{
    int fan_id = gcmd.get_int("I", 0, 0, 100);
    double value = gcmd.get_double("S", 0., 0., 1.);
    if (Printer::GetInstance()->m_printer_fans[fan_id] != nullptr)
    {
        Printer::GetInstance()->m_printer_fans[fan_id]->m_fan->set_speed_from_command(value);
    }
}

int PrinterFan::get_fan_speed_mode(int mode_id)
{
    int speed = 0;
    switch (mode_id)
    {
    case FAN_SILENCE_MODE:
        speed = fan_silence_speed;
        break;
    case FAN_NORMAL_MODE:
        speed = fan_normal_speed;
        break;
    case FAN_CRAZY_MODE:
       speed = fan_crazy_speed;
        break;
    default:
        speed = fan_normal_speed;
        break;
    }
    return speed;
}

#if ENABLE_MANUTEST
double PrinterFan::get_speed()
{
    struct fan_state fan_s;
    double curtime = get_monotonic();
    fan_s =  m_fan->get_status(curtime);
    printf("[PrinterFan] get speed: %f\n", fan_s.rpm);
    return fan_s.rpm;
}
#endif

#define FAN_STATE_CALLBACK_SIZE 16
static fan_state_callback_t fan_state_callback[FAN_STATE_CALLBACK_SIZE];

int fan_register_state_callback(fan_state_callback_t state_callback)
{
    for (int i = 0; i < FAN_STATE_CALLBACK_SIZE; i++)
    {
        if (fan_state_callback[i] == NULL)
        {
            fan_state_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}

int fan_state_callback_call(int state)
{
    // printf("homing_state_callback_call state:%d\n", state);
    for (int i = 0; i < FAN_STATE_CALLBACK_SIZE; i++)
    {
        if (fan_state_callback[i] != NULL)
        {
            fan_state_callback[i](state);
        }
    }
    return 0;
}

bool is_fan_state_arrayempty(void)
{
    // printf("homing_state_callback_call state:%d\n", state);
    for (int i = 0; i < FAN_STATE_CALLBACK_SIZE; i++)
    {
        if (fan_state_callback[i] != NULL)
        {
            return true;
        }
    }
    return false;
}

void check_fan_init(hl_queue_t *queue, fan_state_callback_t state_callback)
{
    hl_queue_create(queue, sizeof(ui_event_fan_state_id_t), 8);
    fan_register_state_callback(state_callback);
}
