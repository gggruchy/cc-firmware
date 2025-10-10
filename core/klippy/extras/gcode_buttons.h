#ifndef GCODE_BUTTONS_H
#define GCODE_BUTTONS_H
#include <string>
#include "gcode.h"

typedef struct GCodeButton_status_tag
{
    std::string state;
}GCodeButton_status_t;


class GCodeButton
{
private:
    
public:
    std::string m_name;
    std::string m_pin;
    bool m_last_state;
    std::function<void(double, bool)> m_button_callback;
public:
    GCodeButton(std::string section_name);
    ~GCodeButton();
    void cmd_QUERY_BUTTON(GCodeCommand &gcmd);
    void button_callback(double eventtime, bool state);
    GCodeButton_status_t get_status(double eventtime = 0);
};



#endif