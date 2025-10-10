#include "heaters.h"
#include "klippy.h"
#include "Define.h"
#include "my_string.h"

#include "simplebus.h"
#include "srv_state.h"

Heater::Heater(std::string section_name, TemperatureSensors *sensor)
{
    m_name = split(section_name, " ").back();
    // setup sensor
    m_min_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "min_temp", DBL_MIN, KELVIN_TO_CELSIUS);
    m_max_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_temp", DBL_MIN, DBL_MIN, DBL_MAX, m_min_temp);
    sensor->setup_minmax(m_min_temp, m_max_temp);
    sensor->setup_callback(std::bind(&Heater::temperature_callback, this, std::placeholders::_1, std::placeholders::_2));
    m_pwm_delay = ADC_TEMP_REPORT_TIME;
    // setup temperature checks
#if !ENABLE_MANUTEST
    m_min_extrude_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "min_extrude_temp", 170, m_min_temp, m_max_temp);
    last_min_extrude_temp = m_min_extrude_temp;
#endif
    bool is_fileoutput = IS_FILEOUTPUT;
    m_can_extrude = (m_min_extrude_temp <= 0. || is_fileoutput) ? true : false;
    m_can_extrude_switch = true;
    m_max_power = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_power", 1., 0., 1.);
    m_smooth_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "smooth_time", 1., DBL_MIN, DBL_MAX, 0.);
    m_inv_smooth_time = 1. / m_smooth_time;
    m_last_temp = m_smoothed_temp = m_target_temp = 0;
    m_last_temp_time = 0;
    // pwm caching
    m_next_pwm_time = 0;
    m_last_pwm_value = 0;
    // setup control algorithm sub-class
    std::string algo = Printer::GetInstance()->m_pconfig->GetString(section_name, "control", "");
    if (algo == "watermark")
    {
        m_control = new ControlBangBang(this, section_name);
    }
    else if (algo == "pid")
    {
        m_control = new ControlPID(this, section_name);
    }
    else
    {
        std::cout << algo << " is not a valid choice" << std::endl;
    }
    // setup output heater pin
    std::string heater_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "heater_pin");
    m_mcu_pwm = (MCU_pwm *)Printer::GetInstance()->m_ppins->setup_pin("pwm", heater_pin);
    double pwm_cycle_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pwm_cycle_time", 0.100, DBL_MIN, DBL_MAX, 0., m_pwm_delay);
    m_mcu_pwm->setup_cycle_time(pwm_cycle_time);
    m_mcu_pwm->setup_max_duration(MAX_HEAT_TIME);
    // load additional modules
    Printer::GetInstance()->load_object("verify_heater " + m_name);
    Printer::GetInstance()->load_object("pid_calibrate");
    Printer::GetInstance()->m_gcode->register_mux_command("SET_HEATER_TEMPERATURE", "HEATER", m_name, std::bind(&Heater::cmd_SET_HEATER_TEMPERATURE, this, std::placeholders::_1), cmd_SET_HEATER_TEMPERATURE_help);
}

Heater::~Heater()
{
    if (m_mcu_pwm != NULL)
    {
        delete m_mcu_pwm;
    }
}

void Heater::set_pwm(double read_time, double value)
{
    if (m_target_temp <= 0)
    {
        value = 0;
    }

    if (((read_time < m_next_pwm_time) || !m_last_pwm_value) && (fabs(value - m_last_pwm_value) < 0.05)) // 4.55S
    {
        return;
    }

    float pwm_time = read_time + m_pwm_delay;          // 0.800
    m_next_pwm_time = pwm_time + 0.75 * MAX_HEAT_TIME; // 3.75+0.8=4.55
    m_last_pwm_value = value;
    // printf("name %s set_pwm %f %lf\n", m_name.c_str(), pwm_time, value);
    m_mcu_pwm->set_pwm(pwm_time, value);
}

void Heater::temperature_callback(double read_time, double temp) //-----4-adc_callback-G-G-2022-08--------
{
    // printf("name %s temperature_callback\n", m_name.c_str());
    m_lock.lock();
    double time_diff = read_time - m_last_temp_time;
    m_last_temp = temp;
    m_last_temp_time = read_time;
    m_control->temperature_update(read_time, temp, m_target_temp);
    double temp_diff = temp - m_smoothed_temp;
    double adj_time = std::min(time_diff * m_inv_smooth_time, 1.0);
    m_smoothed_temp += temp_diff * adj_time;
    // 3℃以内均可通过      
    if( m_min_extrude_temp >= 3.0f )
        m_can_extrude = ( m_smoothed_temp >= (m_min_extrude_temp - 3) ) ? true : false;
    else
        m_can_extrude = (m_smoothed_temp >= m_min_extrude_temp);

    m_lock.unlock();
    // GAM_DEBUG_printf("--temperature_callback--%f--%f--%f--\n",temp,read_time,time_diff );

    // 上报温度数据
    srv_state_heater_msg_t heater_msg;
    if (m_name == "extruder")
        heater_msg.heater_id = HEATER_ID_EXTRUDER;
    else if (m_name == "heater_bed")
        heater_msg.heater_id = HEATER_ID_BED;
    heater_msg.current_temperature = m_smoothed_temp;
    heater_msg.target_temperature = m_target_temp;
    simple_bus_publish_async("heater", SRV_HEATER_MSG_ID_STATE, &heater_msg, sizeof(heater_msg));
}

double Heater::get_pwm_delay()
{
    return m_pwm_delay;
}

double Heater::get_max_power()
{
    return m_max_power;
}

double Heater::get_smooth_time()
{
    return m_smooth_time;
}

double Heater::set_temp(double degrees)
{
    if (degrees && (degrees < m_min_temp || degrees > m_max_temp))
    {
        printf("Requested temperature (%.1f) out of range (%.1f:%.1f)\n", degrees, m_min_temp, m_max_temp);
    }
    m_lock.lock();
    m_target_temp = degrees;
    m_lock.unlock();
}

std::vector<double> Heater::get_temp(double eventtime)
{
    double print_time = m_mcu_pwm->get_mcu()->estimated_print_time(eventtime) - 5;
    m_lock.lock();
    std::vector<double> ret;
    if (m_last_temp_time < print_time)
    {
        ret = {0., m_target_temp};
    }
    else
    {
        ret = {m_smoothed_temp, m_target_temp};
    }
    m_lock.unlock();
    return ret;
}

bool Heater::check_busy(double eventtime)
{
    m_lock.lock();
    bool ret = m_control->check_busy(eventtime, m_smoothed_temp, m_target_temp);
    m_lock.unlock();
    return ret;
}

Control *Heater::set_control(Control *control)
{
    m_lock.lock();
    Control *old_control = m_control;
    m_control = control;
    m_target_temp = 0.;
    m_lock.unlock();
    return old_control;
}

void Heater::alter_target(double target_temp)
{
    if (target_temp)
    {
        target_temp = std::max(m_min_temp, std::min(m_max_temp, target_temp));
    }
    m_target_temp = target_temp;
}

bool Heater::stats(double eventtime)
{
    m_lock.lock();
    double target_temp = m_target_temp;
    double last_temp = m_last_temp;
    double last_pwm_value = m_last_pwm_value;
    m_lock.unlock();
    bool is_active = (target_temp || last_temp > 50.0) ? true : false;
    printf("%s: target=%.0f temp=%.1f pwm=%.3f\n", m_name, target_temp, last_temp, last_pwm_value);
    return is_active;
}

struct temp_state Heater::get_status(double eventtime)
{
    struct temp_state state;
    m_lock.lock();
    state.target_temp = m_target_temp;
    state.smoothed_temp = m_smoothed_temp;
    state.last_pwm_value = m_last_pwm_value;
    m_lock.unlock();
    return state;
}

void Heater::cmd_SET_HEATER_TEMPERATURE(GCodeCommand &gcmd)
{
    double temp = gcmd.get_double("TARGET", 0.);
    Printer::GetInstance()->m_pheaters->set_temperature(this, temp);
}

ControlBangBang::ControlBangBang(Heater *heater, std::string section_name)
{
    m_heater = heater;
    m_heater_max_power = heater->get_max_power();
    m_max_delta = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_delta", 2.0, DBL_MIN, DBL_MAX, 0.);
    m_heating = false;
}

ControlBangBang::~ControlBangBang()
{
}

void ControlBangBang::temperature_update(double read_time, double temp, double target_temp)
{
    if (m_heating && temp >= target_temp + m_max_delta)
    {
        m_heating = false;
    }
    else if (!m_heating && temp <= target_temp - m_max_delta)
    {
        m_heating = true;
    }
    if (m_heating)
    {
        m_heater->set_pwm(read_time, m_heater_max_power);
    }
    else
    {
        m_heater->set_pwm(read_time, 0.0);
    }
}

bool ControlBangBang::check_busy(double eventtime, double smoothed_temp, double target_temp)
{
    return (smoothed_temp < target_temp - m_max_delta);
}

ControlPID::ControlPID(Heater *heater, std::string section_name)
{
    m_heater = heater;
    m_heater_max_power = heater->get_max_power();
    m_section_name = section_name;
    m_Kp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pid_Kp") / PID_PARAM_BASE;
    m_Ki = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pid_Ki") / PID_PARAM_BASE;
    m_Kd = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pid_Kd") / PID_PARAM_BASE;
    m_min_deriv_time = heater->get_smooth_time();
    m_temp_integ_max = 0.;
    if (m_Ki != DBL_MIN / PID_PARAM_BASE)
        m_temp_integ_max = m_heater_max_power / m_Ki;
    m_prev_temp = AMBIENT_TEMP;
    m_prev_temp_time = 0;
    m_prev_temp_deriv = 0;
    m_prev_temp_integ = 0;
}

ControlPID::~ControlPID()
{
}

void ControlPID::temperature_update(double read_time, double temp, double target_temp) //-----5-adc_callback-G-G-2022-08--------
{

    double time_diff = read_time - m_prev_temp_time;
    // Calculate change of temperature
    double temp_diff = temp - m_prev_temp;
    double temp_deriv = 0;
    if (time_diff >= m_min_deriv_time)
    {
        temp_deriv = temp_diff / time_diff;
    }
    else
    {
        temp_deriv = (m_prev_temp_deriv * (m_min_deriv_time - time_diff) + temp_diff) / m_min_deriv_time;
    }
    // Calculate accumulated temperature "error"
    double temp_err = target_temp - temp;
    double temp_integ = m_prev_temp_integ + temp_err * time_diff;
    temp_integ = std::max(0., std::min(m_temp_integ_max, temp_integ));
    // Calculate output
    double co = m_Kp * temp_err + m_Ki * temp_integ - m_Kd * temp_deriv;
    double bounded_co = std::max(0., std::min(m_heater_max_power, co));

    // printf("name %s temperature_update temp %lf target_temp %lf bounded_co %lf\n", m_heater->m_name.c_str(), temp, target_temp, bounded_co);

    m_heater->set_pwm(read_time, bounded_co);
    // Store state for next measurement
    m_prev_temp = temp;
    m_prev_temp_time = read_time;
    m_prev_temp_deriv = temp_deriv;
    if (co == bounded_co)
    {
        m_prev_temp_integ = temp_integ;
    }
}

bool ControlPID::check_busy(double eventtime, double smoothed_temp, double target_temp)
{
    double temp_diff = target_temp - smoothed_temp;
    // return (std::abs(temp_diff) > PID_SETTLE_DELTA || std::abs(m_prev_temp_deriv) > PID_SETTLE_SLOPE) ? true : false;
    return (std::fabs(temp_diff) > 2) ? true : false;
}

void ControlPID::reset_pid()
{
    m_Kp = Printer::GetInstance()->m_pconfig->GetDouble(m_section_name, "pid_Kp") / PID_PARAM_BASE;
    m_Ki = Printer::GetInstance()->m_pconfig->GetDouble(m_section_name, "pid_Ki") / PID_PARAM_BASE;
    m_Kd = Printer::GetInstance()->m_pconfig->GetDouble(m_section_name, "pid_Kd") / PID_PARAM_BASE;
}

void ControlPID::set_pid(double Kp, double Ki, double Kd)
{
    m_Kp = Kp / PID_PARAM_BASE;
    m_Ki = Ki / PID_PARAM_BASE;
    m_Kd = Kd / PID_PARAM_BASE;
}

PrinterHeaters::PrinterHeaters(std::string section_name)
{
    m_has_started = false;
    m_have_load_sensors = false;
    Printer::GetInstance()->register_event_handler("klippy:ready:PrinterHeaters", std::bind(&PrinterHeaters::handle_ready, this));
    Printer::GetInstance()->register_event_handler("gcode:request_restart:PrinterHeaters", std::bind(&PrinterHeaters::turn_off_all_heaters, this));
    std::string cmd_TURN_OFF_HEATERS_help = "Turn off all heaters";
    std::string cmd_TEMPERATURE_WAIT_help = "Wait for a temperature on a sensor";
    // register commands
    Printer::GetInstance()->m_gcode->register_command("TURN_OFF_HEATERS", std::bind(&PrinterHeaters::cmd_TURN_OFF_HEATERS, this, std::placeholders::_1), false, cmd_TURN_OFF_HEATERS_help);
    Printer::GetInstance()->m_gcode->register_command("M105", std::bind(&PrinterHeaters::cmd_M105, this, std::placeholders::_1), true);
    Printer::GetInstance()->m_gcode->register_command("TEMPERATURE_WAIT", std::bind(&PrinterHeaters::cmd_TEMPERATURE_WAIT, this, std::placeholders::_1), false, cmd_TEMPERATURE_WAIT_help);
    Printer::GetInstance()->m_gcode->register_command("M301", std::bind(&PrinterHeaters::cmd_M301, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M130", std::bind(&PrinterHeaters::cmd_M130, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M131", std::bind(&PrinterHeaters::cmd_M131, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M132", std::bind(&PrinterHeaters::cmd_M132, this, std::placeholders::_1));
}

PrinterHeaters::~PrinterHeaters()
{
}

void PrinterHeaters::add_sensor_factory(std::string sensor_type, TemperatureSensors *sensor_factory)
{
    m_sensor_factories[sensor_type] = sensor_factory;
}

Heater *PrinterHeaters::setup_heater(std::string section_name, std::string gcode_id)
{
    std::string heater_name = split(section_name, " ").front();
    if (m_heaters.find(heater_name) != m_heaters.end())
    {
        std::cout << "Heater " << heater_name << " already registered";
    }

    // setup sensor
    TemperatureSensors *sensor = setup_sensor(section_name);
    // create heater
    Heater *heater = new Heater(section_name, sensor);
    m_heaters[heater_name] = heater;
    register_sensor(section_name, heater, gcode_id);

    m_available_heaters.push_back(section_name);
    return heater;
}

std::vector<std::string> PrinterHeaters::get_all_heaters()
{
    return m_available_heaters;
}

Heater *PrinterHeaters::lookup_heater(std::string heater_name)
{
    if (m_heaters.find(heater_name) == m_heaters.end())
    {
        std::cout << "unknown heater " << heater_name << std::endl;
        return nullptr;
    }
    return m_heaters[heater_name];
}

// void PrinterHeaters::load_config()
// {
//     m_have_load_sensors = true;
//     // load default temperature sensors
//     std::string file_name = "temperature_sensors.cfg";
//     ConfigParser *dconfig = new ConfigParser(dir_name + file_name);
// }

TemperatureSensors *PrinterHeaters::setup_sensor(std::string section_name)
{
    std::vector<std::string> modules = {"thermistor", "adc_temperature", "spi_temperature",
                                        "bme280", "htu21d", "lm75", "temperature_host", "temperature_mcu", "ds18b20"};

    add_thermistor_sensors(section_name); //"thermistor" //添加加热传感器...

    std::string sensor_type = Printer::GetInstance()->m_pconfig->GetString(section_name, "sensor_type");
    if (m_sensor_factories.find(sensor_type) == m_sensor_factories.end())
        std::cout << "unknown temperature sensor " << sensor_type << std::endl;
    return m_sensor_factories[sensor_type];
}

void PrinterHeaters::register_sensor(std::string section_name, Heater *psensor, std::string gcode_id)
{
    m_available_sensors.push_back(section_name);
    if (gcode_id == "")
    {
        gcode_id = Printer::GetInstance()->m_pconfig->GetString(section_name, "gcode_id", "");
        if (gcode_id == "")
            return;
    }
    if (m_gcode_id_to_sensor.find(gcode_id) != m_gcode_id_to_sensor.end())
    {
        std::cout << "G-Code sensor id " << gcode_id << " already registered" << std::endl;
    }
    m_gcode_id_to_sensor[gcode_id] = psensor;
}

struct Available_status PrinterHeaters::get_status(double eventtime)
{
    struct Available_status status;
    status.available_heaters = m_available_heaters;
    status.available_sensors = m_available_sensors;
    return status;
}

void PrinterHeaters::turn_off_all_heaters()
{
    for (auto &t : m_heaters)
    {
        t.second->set_temp(0);
    }
}

void PrinterHeaters::cmd_TURN_OFF_HEATERS(GCodeCommand &gcmd)
{
    this->turn_off_all_heaters();
}

// G-Code M105 temperature reporting
void PrinterHeaters::handle_ready()
{
    m_has_started = true;
}

std::string PrinterHeaters::get_temp(double eventtime)
{
    std::vector<std::string> out;
    if (m_has_started)
    {
        for (auto t : m_gcode_id_to_sensor)
        {
            std::vector<double> temp = t.second->get_temp(eventtime);
            std::stringstream ss;
            ss << t.first << ":" << temp[0] << " /" << temp[1];
            out.push_back(ss.str());
        }
        if (out.size() == 0)
        {
            return "T:0";
        }
    }
    std::string ret;
    for (auto i : out)
    {
        ret += i;
        ret += " ";
    }
    return ret;
}

void PrinterHeaters::cmd_M105(GCodeCommand &gcmd)
{
    // Get PrinterExtruder Temperature
    //  reactor = self.printer.get_reactor()
    std::string msg = get_temp(get_monotonic());
    bool did_ack = gcmd.ack(msg);
    if (!did_ack)
    {
        // gcmd.m_respond_raw(msg); //---??---PrinterHeaters
    }
}

void PrinterHeaters::cmd_M301(GCodeCommand &gcmd)
{
    int H = gcmd.get_int("H", 1);
    Printer::GetInstance()->m_pconfig->SetDouble("exturder", "pid_Kp", gcmd.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("exturder", "pid_Kp")));
    Printer::GetInstance()->m_pconfig->SetDouble("exturder", "pid_Ki", gcmd.get_double("I", Printer::GetInstance()->m_pconfig->GetDouble("exturder", "pid_Ki")));
    Printer::GetInstance()->m_pconfig->SetDouble("exturder", "pid_Kd", gcmd.get_double("D", Printer::GetInstance()->m_pconfig->GetDouble("exturder", "pid_Kd")));
    Printer::GetInstance()->m_printer_extruder->get_heater()->m_control->reset_pid();
}

void PrinterHeaters::cmd_M130(GCodeCommand &gcmd)
{
    switch (gcmd.get_int("P", -1))
    {
    case 0:
        Printer::GetInstance()->m_pconfig->SetDouble("exturder", "pid_Kp", gcmd.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("exturder", "pid_Kp")));
        Printer::GetInstance()->m_printer_extruder->get_heater()->m_control->reset_pid();
        break;
    case 1:
        Printer::GetInstance()->m_pconfig->SetDouble("heater_bed", "pid_Kp", gcmd.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("heater_bed", "pid_Kp")));
        Printer::GetInstance()->m_printer_extruder->get_heater()->m_control->reset_pid();
        break;
    default:
        break;
    }
}

void PrinterHeaters::cmd_M131(GCodeCommand &gcmd)
{
    switch (gcmd.get_int("P", -1))
    {
    case 0:
        Printer::GetInstance()->m_pconfig->SetDouble("exturder", "pid_Ki", gcmd.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("exturder", "pid_Ki")));
        Printer::GetInstance()->m_printer_extruder->get_heater()->m_control->reset_pid();
        break;
    case 1:
        Printer::GetInstance()->m_pconfig->SetDouble("heater_bed", "pid_Ki", gcmd.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("heater_bed", "pid_Ki")));
        Printer::GetInstance()->m_printer_extruder->get_heater()->m_control->reset_pid();
        break;
    default:
        break;
    }
}

void PrinterHeaters::cmd_M132(GCodeCommand &gcmd)
{
    switch (gcmd.get_int("P", -1))
    {
    case 0:
        Printer::GetInstance()->m_pconfig->SetDouble("exturder", "pid_Kd", gcmd.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("exturder", "pid_Kd")));
        Printer::GetInstance()->m_printer_extruder->get_heater()->m_control->reset_pid();
        break;
    case 1:
        Printer::GetInstance()->m_pconfig->SetDouble("heater_bed", "pid_Kd", gcmd.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("heater_bed", "pid_Kd")));
        Printer::GetInstance()->m_printer_extruder->get_heater()->m_control->reset_pid();
        break;
    default:
        break;
    }
}

void PrinterHeaters::cmd_M573(GCodeCommand &gcmd)
{
    switch (gcmd.get_int("P", -1))
    {
    case 0:
        std::cout << "PWM value : " << Printer::GetInstance()->m_printer_extruder->m_heater->m_last_pwm_value << std::endl;
        break;
    case 1:
        std::cout << "PWM value : " << Printer::GetInstance()->m_pheaters->heater_bed->m_last_pwm_value << std::endl;
        break;

    default:
        break;
    }
}

void PrinterHeaters::cmd_M136(GCodeCommand &gcmd)
{
    std::vector<double> pid;
    switch (gcmd.get_int("P", -1))
    {
    case 0:
        pid = Printer::GetInstance()->m_printer_extruder->m_heater->m_control->get_pid();
        std::cout << "exturder pid kp: " << pid[0] << std::endl;
        std::cout << "exturder pid ki: " << pid[1] << std::endl;
        std::cout << "exturder pid kd: " << pid[2] << std::endl;
        break;
    case 1:
        if (Printer::GetInstance()->m_pconfig->GetString("heater_bed", "control", "") != "pid")
        {
            std::cout << "heater bed control is not pid" << std::endl;
        }
        else
        {
            pid = Printer::GetInstance()->m_pheaters->heater_bed->m_control->get_pid();
            std::cout << "exturder pid kp: " << pid[0] << std::endl;
            std::cout << "exturder pid ki: " << pid[1] << std::endl;
            std::cout << "exturder pid kd: " << pid[2] << std::endl;
        }
        break;
    default:
        break;
    }
}

void PrinterHeaters::wait_for_temperature(Heater *heater)
{
    // Helper to wait on heater.check_busy() and report M105 temperatures
    // if self.printer.get_start_args().get('debugoutput') is not None:
    //     return
    double eventtime = get_monotonic();
    while (!Printer::GetInstance()->is_shutdown() && heater->check_busy(eventtime))
    {
        if (Printer::GetInstance()->m_highest_priority_sq_require)
        {
            break;
        }
        // double poll_time = Printer::GetInstance()->m_reactor->_check_timers(get_monotonic(), true);
        double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
        // gcode.respond_raw(_get_temp(eventtime));  //---??---PrinterHeaters
        eventtime = Printer::GetInstance()->m_reactor->pause(get_monotonic());
        usleep(10000);
    }
}

void PrinterHeaters::set_temperature(Heater *heater, double temp, bool wait)
{
    Printer::GetInstance()->m_tool_head->register_lookahead_callback(nullptr);
    heater->set_temp(temp);
    if (wait && temp)
        wait_for_temperature(heater);
}

void PrinterHeaters::cmd_TEMPERATURE_WAIT(GCodeCommand &gcmd)
{
    std::string sensor_name = gcmd.get_string("SENSOR", "");
    if (std::find(m_available_sensors.begin(), m_available_sensors.end(), sensor_name) == m_available_sensors.end())
    {
        // gcmd.error("Unknown sensor "%s"" % (sensor_name,))  //---??---PrinterHeaters
    }
    double min_temp = gcmd.get_double("MINIMUM", DBL_MIN);
    double max_temp = gcmd.get_double("MAXIMUM", DBL_MAX, DBL_MIN, DBL_MAX, min_temp);
    if (min_temp == DBL_MIN && max_temp == DBL_MAX)
    {
        // raise gcmd.error("Error on "TEMPERATURE_WAIT": missing MINIMUM or MAXIMUM.")  //---??---PrinterHeaters
    }
    // if self.printer.get_start_args().get("debugoutput") is not None:  //---??---PrinterHeaters
    //     return
    Heater *sensor;
    if (m_heaters.find(sensor_name) != m_heaters.end())
    {
        sensor = m_heaters[sensor_name];
    }
    else
    {
        // sensor = self.printer.lookup_object(sensor_name);  //---??---PrinterHeaters
    }
    double eventtime = get_monotonic();
    while (!Printer::GetInstance()->is_shutdown())
    {
        double temp = sensor->get_temp(eventtime)[0];
        double target = sensor->get_temp(eventtime)[1];
        if (temp >= min_temp && temp <= max_temp)
            return;
        double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
        // gcmd.m_respond_raw(self._get_temp(eventtime));  //---??---PrinterHeaters
        // eventtime = reactor.pause(eventtime + 1.)
    }
}
