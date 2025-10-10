#include "tsl1401cl_filament_width_sensor.h"
#include "klippy.h"

const static double ADC_REPORT_TIME = 0.500;
const static double ADC_SAMPLE_TIME = 0.001;
const static int ADC_SAMPLE_COUNT = 8;
const static int MEASUREMENT_INTERVAL_MM = 10;

FilamentWidthSensor::FilamentWidthSensor(std::string section_name)
{
    m_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "pin");
    m_nominal_filament_dia = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "default_nominal_filament_diameter", DBL_MIN, DBL_MIN, DBL_MAX, 1.0);
    m_measurement_delay = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "measurement_delay", DBL_MIN, DBL_MIN, DBL_MAX, 0);
    m_measurement_max_difference = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_difference", DBL_MIN, DBL_MIN, DBL_MAX, 0);
    m_max_diameter = (m_nominal_filament_dia + m_measurement_max_difference);
    m_min_diameter = (m_nominal_filament_dia - m_measurement_max_difference);
    m_is_active = true;
    m_lastFilamentWidthReading = 0;

    Printer::GetInstance()->register_event_handler("klippy:ready:FilamentWidthSensor", std::bind(&FilamentWidthSensor::handle_ready, this));
    // Start adc
    m_mcu_adc = (MCU_adc*)Printer::GetInstance()->m_ppins->setup_pin("adc", m_pin);
    m_mcu_adc->setup_minmax(ADC_SAMPLE_TIME, ADC_SAMPLE_COUNT);
    m_mcu_adc->setup_adc_callback(ADC_REPORT_TIME, std::bind(&FilamentWidthSensor::adc_callback, this, std::placeholders::_1, std::placeholders::_2));
    // extrude factor updating
    m_extrude_factor_update_timer = Printer::GetInstance()->m_reactor->register_timer("extrude_factor_update_timer", std::bind(&FilamentWidthSensor::extrude_factor_update_event, this, std::placeholders::_1));
    // Register commands
    Printer::GetInstance()->m_gcode->register_command("QUERY_FILAMENT_WIDTH", std::bind(&FilamentWidthSensor::cmd_M407, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("RESET_FILAMENT_WIDTH_SENSOR",
                                std::bind(&FilamentWidthSensor::cmd_ClearFilamentArray, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("DISABLE_FILAMENT_WIDTH_SENSOR",
                                std::bind(&FilamentWidthSensor::cmd_M406, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("ENABLE_FILAMENT_WIDTH_SENSOR",
                                std::bind(&FilamentWidthSensor::cmd_M405, this, std::placeholders::_1));
}

FilamentWidthSensor::~FilamentWidthSensor()
{
}

void FilamentWidthSensor::handle_ready()
{
    // # Load printer objects
    //     self.toolhead = self.printer.lookup_object('toolhead')

    // # Start extrude factor update timer
    Printer::GetInstance()->m_reactor->update_timer(m_extrude_factor_update_timer,
                                Printer::GetInstance()->m_reactor->m_NOW); 

}

void FilamentWidthSensor::adc_callback(double readtime, double read_value)
{
    // read sensor value
    m_lastFilamentWidthReading = round(read_value * 5 * 100) / 100;
}

void FilamentWidthSensor::update_filament_array(double last_epos)
{
    // Fill array
    if(m_filament_array.size() > 0)
    {
        // Get last reading position in array & calculate next
        // reading position
        double next_reading_position = (m_filament_array.back().first + MEASUREMENT_INTERVAL_MM);
        if(next_reading_position <= (last_epos + m_measurement_delay))
        {
            m_filament_array.push(make_pair(last_epos + m_measurement_delay, m_lastFilamentWidthReading));
        }
    }
    else
    {
        // add first item to array
        m_filament_array.push(make_pair(m_measurement_delay + last_epos, m_lastFilamentWidthReading));
    }
}

double FilamentWidthSensor::extrude_factor_update_event(double eventtime)
{
    // Update extrude factor
    std::vector<double> pos = Printer::GetInstance()->m_tool_head->get_position();
    double last_epos = pos[3];
    // Update filament array for lastFilamentWidthReading
    update_filament_array(last_epos);
    // Does filament exists
    if(m_lastFilamentWidthReading > 0.5)
    {
        if(m_filament_array.size() > 0)
        {
            //Get first position in filament array
            double pending_position = m_filament_array.front().first;
            if(pending_position <= last_epos)
            {
                // Get first item in filament_array queue
                std::pair<double, double> item = m_filament_array.front();
                m_filament_array.pop();
                double filament_width = item.second;
                if((filament_width <= m_max_diameter) && (filament_width >= m_min_diameter))
                {
                    int percentage = round(m_nominal_filament_dia * m_nominal_filament_dia / filament_width * filament_width * 100);
                    std::string cmd = "M221 S" + to_string(percentage);
                    Printer::GetInstance()->m_gcode->run_script(cmd);
                }
                else
                {
                    std::string cmd = "M221 S100";
                    Printer::GetInstance()->m_gcode->run_script(cmd);
                }
            }
        }
    }
    else
    {
        std::string cmd = "M221 S100";
        Printer::GetInstance()->m_gcode->run_script(cmd);
        std::queue<std::pair<double, double>>().swap(m_filament_array);
    }
    if(m_is_active)
    {
        return eventtime + 1;
    }
    else
    {
        return Printer::GetInstance()->m_reactor->m_NEVER;
    }

}

void FilamentWidthSensor::cmd_M407(GCodeCommand &gcmd)
{
    std::string response = "";
    if(m_lastFilamentWidthReading > 0)
    {
        response += ("Filament dia (measured mm): " + to_string(m_lastFilamentWidthReading));
    }
    else
    {
        response += "Filament NOT present";
    }
    gcmd.m_respond_info(response, true);
}

void FilamentWidthSensor::cmd_ClearFilamentArray(GCodeCommand &gcmd)
{
    std::queue<std::pair<double, double>>().swap(m_filament_array);
    gcmd.m_respond_info("Filament width measurements cleared!", true);
    // Set extrude multiplier to 100%
    std::string cmd = "M221 S100";
    Printer::GetInstance()->m_gcode->run_script(cmd);
}

void FilamentWidthSensor::cmd_M405(GCodeCommand &gcmd)
{
    std::string response = "Filament width sensor Turned On";
    if (m_is_active)
    {
        response = "Filament width sensor is already On";
    }
    else
    {
        m_is_active = true;
        // Start extrude factor update timer
        Printer::GetInstance()->m_reactor->update_timer(m_extrude_factor_update_timer,
                                Printer::GetInstance()->m_reactor->m_NOW); 
    }
    gcmd.m_respond_info(response, true);
}

void FilamentWidthSensor::cmd_M406(GCodeCommand &gcmd)
{
    std::string response = "Filament width sensor Turned Off";
    if (!m_is_active)
    {
        response = "Filament width sensor is already Off";
    }
    else
    {

        m_is_active = false;
        // Stop extrude factor update timer
        Printer::GetInstance()->m_reactor->update_timer(m_extrude_factor_update_timer,
                                Printer::GetInstance()->m_reactor->m_NEVER); 
        // Clear filament array
        std::queue<std::pair<double, double>>().swap(m_filament_array);
        // Set extrude multiplier to 100%
        std::string cmd = "M221 S100";
        Printer::GetInstance()->m_gcode->run_script(cmd);
    }
    gcmd.m_respond_info(response, true);
}

