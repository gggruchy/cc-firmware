#include "neopixel.h"
#include "Define.h"
#include "klippy.h"
#include "debug.h"
#include "my_string.h"
#define LOG_TAG "neopixel"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define CHIP_NAME "clocksync"

#define BIT_MAX_TIME 0.000004f
#define RESET_MIN_TIME 0.000050f

#define MAX_MCU_SIZE 500 // Sanity check on LED chain length
NeoPixel::NeoPixel(std::string section_name)
{
    m_name = split(section_name, " ").back();
    PrinterPins *ppins = Printer::GetInstance()->m_ppins;
    std::string neopixel_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "pin", "");
    pinParams *pin_params = ppins->lookup_pin(neopixel_pin);
    m_mcu = (MCU *)pin_params->chip;
    m_pin = m_mcu->m_serial->m_msgparser->m_pinMap[pin_params->pin];
    m_oid = m_mcu->create_oid();
    m_neopixel_update_cmd = "";
    m_neopixel_send_cmd = "";
    /*todo 指定每一个RGB的颜色顺序待实现*/
    m_chain_count = Printer::GetInstance()->m_pconfig->GetInt(section_name, "chain_count", 1, 1);
    m_color_order.resize(m_chain_count);
    m_color_order[0] = Printer::GetInstance()->m_pconfig->GetString(section_name, "color_order", "GRB");
    // 把所有的颜色顺序都设置为第一个的颜色顺序
    for (int i = 1; i < m_chain_count; i++)
    {
        m_color_order[i] = m_color_order[0];
    }
    /*todo 指定每一个RGB的颜色顺序待实现*/
    std::vector<std::pair<int, int>> color_indexes;
    for (int lidx = 0; lidx < m_color_order.size(); ++lidx)
    {
        std::string co = m_color_order[lidx];
        std::sort(co.begin(), co.end());
        if (co != "BGR" && co != "BGRW")
        {
            LOG_E("Invalid m_color_order %s\n", co.c_str());
        }
        for (char c : m_color_order[lidx])
        {
            color_indexes.push_back(std::make_pair(lidx, std::string("RGBW").find(c)));
        }
    }
    for (int i = 0; i < color_indexes.size(); ++i)
    {
        m_color_map.push_back(std::make_tuple(i, color_indexes[i].first, color_indexes[i].second));
    }
    if (m_color_map.size() > MAX_MCU_SIZE)
    {
        LOG_E("Too many LEDs in chain %d\n", m_color_map.size());
    }
    // Initialize color data
    m_pled = new PrinterLED(section_name);
    m_led_helper = m_pled->setup_helper(section_name, std::bind(&NeoPixel::update_leds, this, std::placeholders::_1, std::placeholders::_2), m_chain_count);

    m_color_data.resize(m_color_map.size());
    update_color_data(m_led_helper->get_status(0.0));
    m_old_color_data.resize(m_color_map.size());
    for (int i = 0; i < m_color_data.size(); ++i)
    {
        m_old_color_data[i] = m_color_data[i] ^ 1;
    }
    m_mcu->register_config_callback(std::bind(&NeoPixel::build_config, this, std::placeholders::_1));
    Printer::GetInstance()->register_event_handler("klippy:connect" + section_name, std::bind(&NeoPixel::send_data, this));
}
void NeoPixel::build_config(int para)
{
    uint64_t bmt = m_mcu->seconds_to_clock(BIT_MAX_TIME);
    uint64_t rmt = m_mcu->seconds_to_clock(RESET_MIN_TIME);
    std::stringstream neo_pixel_config;
    neo_pixel_config << "config_neopixel oid=" << m_oid << " pin=" << m_pin << " data_size=" << m_color_data.size() << " bit_max_ticks=" << bmt << " reset_min_ticks=" << rmt;
    m_mcu->add_config_cmd(neo_pixel_config.str());
    m_cmd_queue = m_mcu->alloc_command_queue();
}
void NeoPixel::update_leds(std::vector<color_t> color, double print_time)
{
    update_color_data(color);
    send_data();
}
void NeoPixel::update_color_data(std::vector<color_t> led_state)
{
    for (int i = 0; i < m_color_map.size(); ++i)
    {
        int first = std::get<0>(m_color_map[i]);  // 获取第一个元素
        int second = std::get<1>(m_color_map[i]); // 获取第二个元素
        int third = std::get<2>(m_color_map[i]);  // 获取第三个元素
        m_color_data[i] = (int)(led_state[second].at(third) * 255.0f + 0.5f);
    }
}
void NeoPixel::send_data(void)
{
    std::vector<unsigned char> new_data = m_color_data;
    std::vector<unsigned char> old_data = m_old_color_data;
    if (new_data == old_data)
    {
        return;
    }
    std::vector<std::pair<int, int>> diffs;
    for (int i = 0; i < new_data.size(); ++i)
    {
        if (new_data[i] != old_data[i])
        {
            diffs.push_back(std::make_pair(i, 1));
        }
    }
    for (int i = diffs.size() - 2; i >= 0; --i)
    {
        int pos = diffs[i].first;
        int count = diffs[i].second;
        int nextpos = diffs[i + 1].first;
        int nextcount = diffs[i + 1].second;
        if (pos + 5 >= nextpos && nextcount < 16)
        {
            diffs[i].second = nextcount + (nextpos - pos);
            diffs.erase(diffs.begin() + i + 1);
        }
    }
    for (const auto &pair : diffs)
    {
        int pos = pair.first;
        int count = pair.second;
        std::string sub_data;
        for (int i = pos; i < pos + count; ++i)
        {
            sub_data += new_data[i];
        }
        std::stringstream neopixel_update;
        neopixel_update << "neopixel_update oid=" << m_oid << " pos=" << pos << " data=" << sub_data;
        m_mcu->m_serial->send(neopixel_update.str(), 0, BACKGROUND_PRIORITY_CLOCK, m_cmd_queue);
    }
    m_old_color_data = m_color_data;
    double eventtime = get_monotonic();
    double print_time_temp = m_mcu->estimated_print_time(eventtime) + PIN_MIN_TIME;
    uint64_t minclock = m_mcu->print_time_to_clock(print_time_temp);
    for (int i = 0; i < 8; ++i)
    {
        std::stringstream neopixel_send;
        neopixel_send << "neopixel_send oid=" << m_oid;
        ParseResult params = m_mcu->m_serial->send_with_response(neopixel_send.str(), "neopixel_result", m_cmd_queue, m_oid, 0, BACKGROUND_PRIORITY_CLOCK);
        if (params.PT_uint32_outs.at("success"))
            break;
        else
            LOG_E("Neopixel update did not succeed\n");
    }
}