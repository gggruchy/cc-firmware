#include "filament_motion_sensor.h"
#include "klippy.h"

static double CHECK_RUNOUT_TIMEOUT = .250;

EncoderSensor::EncoderSensor(std::string section_name)
{
    // Read config
    std::string switch_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "switch_pin");
    m_extruder_name = Printer::GetInstance()->m_pconfig->GetString(section_name, "extruder");
    m_detection_length = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "detection_length", 7., DBL_MIN, DBL_MAX, 0.);
    // Configure pins
    // buttons = Printer::GetInstance()->load_object("buttons");
    // Printer::GetInstance()->m_buttons->register_buttons([switch_pin], self.encoder_event); //----????----
    // Get printer objects
    m_reactor = Printer::GetInstance()->get_reactor();
    m_runout_helper = new RunoutHelper(section_name);
    m_get_status = std::bind(&RunoutHelper::get_status, m_runout_helper);
    m_extruder = nullptr;
    m_estimated_print_time = nullptr;
    // Initialise internal state
    m_filament_runout_pos = 0;
    // Register commands and event handlers
    Printer::GetInstance()->register_event_handler("klippy:ready:EncoderSensor",
            std::bind(&EncoderSensor::handle_ready, this));
    Printer::GetInstance()->register_event_double_handler("idle_timeout:printing:EncoderSensor", std::bind(&EncoderSensor::handle_printing, this, std::placeholders::_1));
    Printer::GetInstance()->register_event_double_handler("idle_timeout:ready:EncoderSensor", std::bind(&EncoderSensor::handle_not_printing, this, std::placeholders::_1));
    Printer::GetInstance()->register_event_double_handler("idle_timeout:idle:EncoderSensor", std::bind(&EncoderSensor::handle_not_printing, this, std::placeholders::_1));
}

EncoderSensor::~EncoderSensor()
{
}

void EncoderSensor::update_filament_runout_pos(double eventtime)
{
    if(!eventtime)
        eventtime = get_monotonic();
    m_filament_runout_pos = get_extruder_pos(eventtime) + m_detection_length;
}

void EncoderSensor::handle_ready()
{
    // self.extruder = Printer::GetInstance()->lookup_object(m_extruder_name); //----????----
    m_estimated_print_time = std::bind(&MCU::estimated_print_time, Printer::GetInstance()->m_mcu, std::placeholders::_1);
    update_filament_runout_pos();
    m_extruder_pos_update_timer = Printer::GetInstance()->m_reactor->register_timer("extruder_pos_update_timer", std::bind(&EncoderSensor::extruder_pos_update_event, this, std::placeholders::_1));
}

void EncoderSensor::handle_printing(double print_time)
{
    Printer::GetInstance()->m_reactor->update_timer(m_extruder_pos_update_timer, Printer::GetInstance()->m_reactor->m_NOW);
}

void EncoderSensor::handle_not_printing(double print_time)
{
    Printer::GetInstance()->m_reactor->update_timer(m_extruder_pos_update_timer, Printer::GetInstance()->m_reactor->m_NEVER);
}

double EncoderSensor::get_extruder_pos(double eventtime)
{
    if (!eventtime)
        eventtime = get_monotonic();
    double print_time = m_estimated_print_time(eventtime);
    return m_extruder->find_past_position(print_time);
}
 
double EncoderSensor::extruder_pos_update_event(double eventtime)
{
    double extruder_pos = get_extruder_pos(eventtime);
    // Check for filament runout
    m_runout_helper->note_filament_present(extruder_pos < m_filament_runout_pos);
    return eventtime + CHECK_RUNOUT_TIMEOUT;
}

void EncoderSensor::encoder_event(double eventtime, bool state)
{
    if (m_extruder != nullptr)
    {
        update_filament_runout_pos(eventtime);
        // Check for filament insertion
        // Filament is always assumed to be present on an encoder event
        m_runout_helper->note_filament_present(true);
    }
}
        