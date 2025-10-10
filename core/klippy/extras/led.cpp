#include "led.h"
#include "Define.h"
#include "klippy.h"
#include "my_string.h"
#include "gpio.h"
#define LOG_TAG "led"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define RENDER_TIME 0.5f
#define PIN_MIN_TIME 0.100
#define MAX_SCHEDULE_TIME 5.0
#define PRINT_TIME_NONE -1.0f
LEDHelper::LEDHelper(std::string section_name, std::function<void(std::vector<color_t>, double)> update_func, int led_count)
{
    m_update_func = update_func;
    m_led_count = led_count;
    m_need_transmit = false;
    m_led_state.resize(m_led_count);
    double red = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "initial_RED", 0.0f, 0.0f, 1.0f);
    double green = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "initial_GREEN", 0.0f, 0.0f, 1.0f);
    double blue = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "initial_BLUE", 0.0f, 0.0f, 1.0f);
    double white = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "initial_WHITE", 0.0f, 0.0f, 1.0f);
    for (int i = 0; i < m_led_count; i++)
    {
        m_led_state[i] = {red, green, blue, white};
    }
    string register_cmd_name = string("SET_LED") + "_" + split(section_name, " ").back();
    // LOG_I("register_cmd_name %s\n", register_cmd_name.c_str());
    /*todo 复用命令注册接口未移植*/
    // string name = split(section_name, " ").back();
    // Printer::GetInstance()->m_gcode->register_mux_command("SET_LED", "LED", name, std::bind(&LEDHelper::cmd_SET_LED, this, std::placeholders::_1), "Set the color of an LED");
    Printer::GetInstance()->m_gcode->register_command(register_cmd_name, std::bind(&LEDHelper::cmd_SET_LED, this, std::placeholders::_1));
}

LEDHelper::~LEDHelper()
{
}

int LEDHelper::get_led_count()
{
    return m_led_count;
}

void LEDHelper::set_color(int index, color_t color)
{
    if (index >= m_led_count)
    {
        LOG_E("Invalid LED index %d\n", index);
        return;
    }
    m_led_state[index] = color;
    m_need_transmit = true;
}

void LEDHelper::check_transmit(double print_time)
{
    if (!m_need_transmit)
        return;
    m_need_transmit = false;
    try
    {
        m_update_func(m_led_state, print_time);
    }
    catch (const std::exception &e)
    {
        LOG_E("Error updating LED: %s\n", e.what());
    }
}
// void LEDHelper::lookahead_bgfunc(double print_time, double temp)
// {
//     set_color(index, color);
//     if (transmit)
//         check_transmit(print_time);
// }
void LEDHelper::cmd_SET_LED(GCodeCommand &gcode)
{
    double red = gcode.get_double("RED", 0.0f, 0.0f, 1.0f);
    double green = gcode.get_double("GREEN", 0.0f, 0.0f, 1.0f);
    double blue = gcode.get_double("BLUE", 0.0f, 0.0f, 1.0f);
    double white = gcode.get_double("WHITE", 0.0f, 0.0f, 1.0f);
    int index = gcode.get_int("INDEX", 0, 0);
    int transmit = gcode.get_int("TRANSMIT", 0, 0);
    // int sync = gcode.get_int("SYNC", 0, 0);//暂不支持同步
    color_t color = {red, green, blue, white};
    // if (sync)//todo
    // {
    //     Printer::GetInstance()->m_tool_head->register_lookahead_callback(std::bind(&LEDHelper::lookahead_bgfunc, this, std::placeholders::_1, std::placeholders::_2));
    // }
    // else
    // {
    // lookahead_bgfunc(0.0f);
    // }
    set_color(index, color);
    if (transmit){
        // LOG_I("check_transmit\n");
        check_transmit(PRINT_TIME_NONE);
    }

}
std::vector<color_t> LEDHelper::get_status(double eventtime)
{
    return m_led_state;
}

PrinterLED::PrinterLED(std::string section_name)
{
}
LEDHelper *PrinterLED::setup_helper(std::string section_name, std::function<void(std::vector<color_t>, double)> update_func, int led_count)
{
    LEDHelper *helper = new LEDHelper(section_name, update_func, led_count);
    if (split(section_name, " ").size() > 1)
        m_led_helpers[split(section_name, " ").back()] = helper;
    else
        m_led_helpers[section_name] = helper;
    return helper;
}
/**
 * @brief //按照源码意思，PrinterPWMLED，一个模块实例只有一个led。可以有rgbw四个通道。(理解不一定对)
 *
 * @param section_name
 */
PrinterPWMLED::PrinterPWMLED(std::string section_name)
{
    std::vector<std::string> pin_names;
    std::vector<color_t> color;
    // read config
    double cycle_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "cycle_time", 0.010, DBL_MIN, MAX_SCHEDULE_TIME, 0.);
    bool hardware_pwm = Printer::GetInstance()->m_pconfig->GetBool(section_name, "hardware_pwm", false);

    std::vector<std::string> names = {"red", "green", "blue", "white"};
    for (int i = 0; i < names.size(); i++)
    {
        std::string pin_name = Printer::GetInstance()->m_pconfig->GetString(section_name, names[i] + "_pin");
        if (pin_name.empty())
        {
            continue;
        }
        m_mcu_led.push_back((MCU_pwm *)Printer::GetInstance()->m_ppins->setup_pin("pwm", pin_name));
        m_mcu_led[i]->setup_max_duration(0.);
        m_mcu_led[i]->setup_cycle_time(cycle_time, hardware_pwm);
        pins_s pin_temp;
        pin_temp.index = i;
        pin_temp.mcu_pin = m_mcu_led[i];
        m_pins.push_back(pin_temp);
    }
    if (m_mcu_led.size() == 0)
    {
        LOG_E("No pins configured for LED %s \n", section_name);
    }
    m_last_print_time = 0.0f;
    m_led = new PrinterLED(section_name);
    m_led_helper = m_led->setup_helper(section_name, std::bind(&PrinterPWMLED::update_leds, this, std::placeholders::_1, std::placeholders::_2), 1);
    m_prev_color = color = m_led_helper->get_status(0.0);
    for (int i = 0; i < m_mcu_led.size(); i++)
    {
        m_mcu_led[i]->setup_start_value(0.0f, 0.0f);
    }
}
void PrinterPWMLED::update_leds(std::vector<color_t> led_state, double print_time)
{
    double print_time_temp = print_time;
    color_t color;
    if (print_time_temp <= PRINT_TIME_NONE)
    {
        double eventtime = get_monotonic();
        print_time_temp = m_mcu_led[0]->m_mcu->estimated_print_time(eventtime) + PIN_MIN_TIME;
    }
    print_time_temp = std::max(print_time_temp, m_last_print_time + PIN_MIN_TIME);
    color = led_state[0]; // 按照源码意思，PrinterPWMLED，一个模块实例只有一个led。可以有rgbw四个通道。(理解不一定对)
    for (int i = 0; i < m_pins.size(); i++)
    {
        if (m_prev_color[0].at(m_pins[i].index) != color.at(m_pins[i].index))
        {
            m_pins[i].mcu_pin->set_pwm(print_time_temp, color.at(m_pins[i].index));
            // LOG_I("set_pwm %f %f\n", print_time_temp, color.at(m_pins[i].index));
            m_last_print_time = print_time_temp;
        }
    }
    m_prev_color = led_state;
}

SysfsLEDControl::SysfsLEDControl(std::string section_name)
{
    pin_name = Printer::GetInstance()->m_pconfig->GetString(section_name, "pin", "PC2");
    gpio_level = Printer::GetInstance()->m_pconfig->GetInt(section_name, "level", 1);
    gpio_num = (pin_name[1] - 'A') * R528_PB_BASE + std::stoi(pin_name.substr(2));
    if (gpio_is_init(gpio_num) < 0)
	{
		gpio_init(gpio_num); // 断料检测IO初始化
		gpio_set_direction(gpio_num, GPIO_OUTPUT);
        gpio_set_value(gpio_num, (gpio_state_t)gpio_level);
	}
}

int SysfsLEDControl::get_lednum(void)
{
    return gpio_num;
}

void SysfsLEDControl::control_light(int gpio_level)
{
    gpio_set_value(gpio_num, (gpio_state_t)gpio_level);
}

