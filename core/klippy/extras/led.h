#ifndef __LED_H__
#define __LED_H__

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
#include "gcode.h"
#include "mcu_io.h"
class color_t
{
public:
    double r, g, b, w;

    double &at(int index)
    {
        switch (index)
        {
        case 0:
            return r;
        case 1:
            return g;
        case 2:
            return b;
        case 3:
            return w;
        default:
            throw std::out_of_range("Index out of range for color_t");
        }
    }
};
class LEDHelper
{
public:
    LEDHelper(std::string section_name, std::function<void(std::vector<color_t>, double)> update_func, int led_count = 1);
    ~LEDHelper();
    int get_led_count();
    int m_led_count;
    bool m_need_transmit;
    std::vector<color_t> m_led_state;
    void set_color(int index, color_t color);
    void check_transmit(double print_time);
    std::function<void(std::vector<color_t>, double)> m_update_func;
    void cmd_SET_LED(GCodeCommand &gcode);
    void lookahead_bgfunc(double print_time, double temp);
    std::vector<color_t> get_status(double eventtime);
};
class PrinterLED
{
public:
    PrinterLED(std::string section_name);
    ~PrinterLED();
    LEDHelper *setup_helper(std::string section_name, std::function<void(std::vector<color_t>, double)> update_func, int led_count = 1);
    void _activate_timer();
    std::map<std::string, LEDHelper *> m_led_helpers;
};
class PrinterPWMLED
{
public:
    PrinterPWMLED(std::string section_name);
    ~PrinterPWMLED();
    std::vector<MCU_pwm *> m_mcu_led;
    double m_last_print_time;
    PrinterLED *m_led;
    LEDHelper *m_led_helper;
    void update_leds(std::vector<color_t> color, double print_time);
    std::vector<color_t> m_prev_color;
    struct pins_s
    {
        int index;
        MCU_pwm *mcu_pin;
    };
    std::vector<pins_s> m_pins;
};

class SysfsLEDControl
{
private:
    std::string pin_name;
    int gpio_num;
    int gpio_level;
public:
    SysfsLEDControl(std::string section_name);
    ~SysfsLEDControl();
    int get_lednum();
    void control_light(int gpio_level);
};

#endif