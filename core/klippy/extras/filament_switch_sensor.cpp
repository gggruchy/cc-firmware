#include "filament_switch_sensor.h"
#include "my_string.h"
#include "klippy.h"

static std::string cmd_QUERY_FILAMENT_SENSOR_help = "Query the status of the Filament Sensor";
static std::string cmd_SET_FILAMENT_SENSOR_help = "Sets the filament sensor on/off";

RunoutHelper::RunoutHelper(std::string section_name)
{
    m_name = section_name;
    // self.printer = config.get_printer()
    // self.reactor = self.printer.get_reactor()
    // self.gcode = self.printer.lookup_object('gcode')
    // Read config
    m_runout_pause = Printer::GetInstance()->m_pconfig->GetBool(section_name, "pause_on_runout", true);
    if(m_runout_pause)
    {
        Printer::GetInstance()->load_object("pause_resume"); //----????----
    }
    m_runout_gcode = "";
    m_insert_gcode = "";
    Printer::GetInstance()->load_object("gcode_macro");
    if (m_runout_pause || Printer::GetInstance()->m_pconfig->GetString(section_name, "runout_gcode", "") != "")
    {
        // m_runout_gcode = gcode_macro->load_template(
        //     config, 'runout_gcode', ''); //----????----
    }
    if (Printer::GetInstance()->m_pconfig->GetString(section_name, "insert_gcode", "") != "")
    {
        // self.insert_gcode = gcode_macro.load_template(
        //     config, 'insert_gcode');    //----????----
    }
    m_pause_delay = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_delay", .5, DBL_MIN, DBL_MAX, .0);
    m_event_delay = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "event_delay", 3., DBL_MIN, DBL_MAX, 0.);
    // Internal state
    m_min_event_systime = Printer::GetInstance()->m_reactor->m_NOW;
    m_filament_present = false;
    m_sensor_enabled = true;
    // Register commands and event handlers
    Printer::GetInstance()->register_event_handler("klippy:ready:RunoutHelper", std::bind(&RunoutHelper::handle_ready, this));
    Printer::GetInstance()->m_gcode->register_mux_command(
        "QUERY_FILAMENT_SENSOR", "SENSOR", m_name,
        std::bind(&RunoutHelper::cmd_QUERY_FILAMENT_SENSOR, this, std::placeholders::_1),
        cmd_QUERY_FILAMENT_SENSOR_help);
    Printer::GetInstance()->m_gcode->register_mux_command(
        "SET_FILAMENT_SENSOR", "SENSOR", m_name,
        std::bind(&RunoutHelper::cmd_SET_FILAMENT_SENSOR, this, std::placeholders::_1),
        cmd_SET_FILAMENT_SENSOR_help);
}

RunoutHelper::~RunoutHelper()
{
}

void RunoutHelper::handle_ready()
{
    m_min_event_systime = get_monotonic() + 2.;
}

double RunoutHelper::runout_event_handler(double eventtime)
{
    // Pausing from inside an event requires that the pause portion
    // of pause_resume execute immediately.
    std::string pause_prefix = "";
    if (m_runout_pause)
    {
        Printer::GetInstance()->lookup_object("pause_resume");
        Printer::GetInstance()->m_pause_resume->send_pause_command();
        pause_prefix = "PAUSE\n";
        Printer::GetInstance()->get_reactor()->pause(eventtime + m_pause_delay);
    }
    exec_gcode(pause_prefix, m_runout_gcode);
}

double RunoutHelper::insert_event_handler(double eventtime)
{
    exec_gcode("", m_insert_gcode);
}

void RunoutHelper::exec_gcode(std::string prefix, std::string gcode)
{
    // Printer::GetInstance()->m_gcode->run_script(prefix + gcode.render() + "\nM400");//----????----
    m_min_event_systime = get_monotonic() + m_event_delay;
}

void RunoutHelper::note_filament_present(bool is_filament_present)
{
    if (is_filament_present == m_filament_present)
        return;
    m_filament_present = is_filament_present;
    double eventtime = get_monotonic();
    if (eventtime < m_min_event_systime || !m_sensor_enabled)
    {
        // do not process during the initialization time, duplicates,
        // during the event delay time, while an event is running, or
        // when the sensor is disabled
        return;
    }
    // Determine "printing" status
    bool is_printing ;//= Printer::GetInstance()->m_idle_timeout->get_status(eventtime).state == "Printing";//----????----
    // Perform filament action associated with status change (if any)
    if (is_filament_present)
    {
        if (! is_printing && m_insert_gcode != "")
        {
            // insert detected
            m_min_event_systime = Printer::GetInstance()->m_reactor->m_NEVER;
            std::cout << "Filament Sensor " + m_name + ": insert event detected, Time " + to_string(eventtime) << std::endl;
            Printer::GetInstance()->m_reactor->register_callback(std::bind(&RunoutHelper::insert_event_handler, this, std::placeholders::_1));
        }
    }
    else if (is_printing && m_runout_gcode != "")
    {
        // runout detected
        m_min_event_systime = Printer::GetInstance()->m_reactor->m_NEVER;
        std::cout << "Filament Sensor " + m_name + ": runout event detected, Time " + to_string(eventtime) << std::endl;
        Printer::GetInstance()->m_reactor->register_callback(std::bind(&RunoutHelper::runout_event_handler, this, std::placeholders::_1));
    }
}

RunoutHelper_status_t RunoutHelper::get_status()
{
    RunoutHelper_status_t status{
        .filament_detected = m_filament_present,
        .enabled = m_sensor_enabled
    };
    return  status;
}


void RunoutHelper::cmd_QUERY_FILAMENT_SENSOR(GCodeCommand &gcmd)
{
    std::string msg = "";
    if (m_filament_present)
        msg = "Filament Sensor " + m_name + ": filament detected";
    else
        msg = "Filament Sensor " + m_name + ": filament not detected";
    gcmd.m_respond_info(msg, true);
}


void RunoutHelper::cmd_SET_FILAMENT_SENSOR(GCodeCommand &gcmd)
{
    m_sensor_enabled = gcmd.get_int("ENABLE", 1);
}

SwitchSensor::SwitchSensor(std::string section_name)
{
    Printer::GetInstance()->load_object("buttons");
    std::string switch_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "switch_pin");
    Printer::GetInstance()->m_buttons->register_buttons({switch_pin}, std::bind(&SwitchSensor::button_handler, this, std::placeholders::_1, std::placeholders::_2));
    m_runout_helper = new RunoutHelper(section_name);
    m_get_status = std::bind(&RunoutHelper::get_status, m_runout_helper);
}

SwitchSensor::~SwitchSensor()
{
    
}

void SwitchSensor::button_handler(double eventtime, bool state)
{
    m_runout_helper->note_filament_present(state);
}   