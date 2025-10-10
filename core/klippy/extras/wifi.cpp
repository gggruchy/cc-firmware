#include "wifi.h"
#include "klippy.h"

WiFi::WiFi()
{
    Printer::GetInstance()->m_gcode->register_command("M587", std::bind(&WiFi::cmd_M587, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M588", std::bind(&WiFi::cmd_M588, this, std::placeholders::_1));
}

WiFi::~WiFi()
{

}

void WiFi::cmd_M587(GCodeCommand& gcmd)
{
    // wifi_get_scan_results();
}

void WiFi::cmd_M588(GCodeCommand& gcmd)
{
    std::string ssid = gcmd.get_string("S", ""); //wifi名

    // wifi_remove_networks(ssid.c_str());
}