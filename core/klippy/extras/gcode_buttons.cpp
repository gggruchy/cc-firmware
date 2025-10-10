#include "gcode_buttons.h"
#include "klippy.h"
#include "my_string.h"

static std::string cmd_QUERY_BUTTON_help = "Report on the state of a button";

GCodeButton::GCodeButton(std::string section_name)
{
    m_name = split(section_name, " ").back();
    m_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "pin");
    m_last_state = 0;
    Printer::GetInstance()->load_object("buttons");
    std::string analog_range = Printer::GetInstance()->m_pconfig->GetString(section_name, "analog_range", "");
    if (analog_range == "")
        Printer::GetInstance()->m_buttons->register_buttons({m_pin}, m_button_callback);
    else
    {
        double amin = stod(split(analog_range, ",")[0]);
        double amax = stod(split(analog_range, ",")[1]);
        int pullup = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "analog_pullup_resistor", 4700., DBL_MIN, DBL_MAX, 0.);
        Printer::GetInstance()->m_buttons->register_adc_button(m_pin, amin, amax, pullup,
                                    std::bind(&GCodeButton::button_callback, this, std::placeholders::_1, std::placeholders::_2));
    }
    // self.press_template = gcode_macro.load_template(config, 'press_gcode') //----????-----
    // self.release_template = gcode_macro.load_template(config,
    //                                                     'release_gcode', '')
 
    Printer::GetInstance()->m_gcode->register_mux_command("QUERY_BUTTON", "BUTTON", m_name,
                                    std::bind(&GCodeButton::cmd_QUERY_BUTTON, this, std::placeholders::_1),
                                    cmd_QUERY_BUTTON_help);
}

GCodeButton::~GCodeButton()
{
}


void GCodeButton::cmd_QUERY_BUTTON(GCodeCommand &gcmd)
{
    gcmd.m_respond_info(m_name + ": " + get_status().state, true);
}

void GCodeButton::button_callback(double eventtime, bool state)
{
    m_last_state = state;
    // std::string template = m_press_template;  //-----????----
    // if (! state)
    //     template = m_release_template;
    // Printer::GetInstance()->m_gcode->run_script(template.render());
}

GCodeButton_status_t GCodeButton::get_status(double eventtime)
{
    if (m_last_state)
    {
        GCodeButton_status_t status{
            .state = "PRESSED"
        };
        return status;
    }
    GCodeButton_status_t status{
            .state = "RELEASED"
    };
    return status;
}
