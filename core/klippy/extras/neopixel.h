#ifndef __NEOPIXEL_H__
#define __NEOPIXEL_H__

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
#include "led.h"
class NeoPixel
{
public:
    NeoPixel(std::string section_name);
    ~NeoPixel();
    void build_config(int para);
    void update_leds(std::vector<color_t> color, double print_time);
    void update_color_data(std::vector<color_t> led_state);
    void send_data(void);
    MCU *m_mcu;
    int m_oid;
    int m_pin;
    int m_pullup;
    int m_invert;
    struct trdispatch * m_trdispatch;
    command_queue * m_cmd_queue;
    std::string m_name;
    std::string m_neopixel_update_cmd;
    std::string m_neopixel_send_cmd;
    int m_chain_count;
    std::vector<std::string> m_color_order;
    std::vector<std::string> color_indexes;
    PrinterLED *m_pled;
    LEDHelper *m_led_helper;
    std::vector<unsigned char> m_color_data;
    std::vector<unsigned char> m_old_color_data;
    std::vector<std::tuple<int, int, int>> m_color_map;
};
#endif