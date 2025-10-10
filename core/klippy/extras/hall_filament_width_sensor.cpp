#include "hall_filament_width_sensor.h"
#include "klippy.h"

const static double ADC_REPORT_TIME = 0.500;
const static double ADC_SAMPLE_TIME = 0.03;
const static int ADC_SAMPLE_COUNT = 15;

HallFilamentWidthSensor::HallFilamentWidthSensor(std::string section_name)
{
    m_pin1 = Printer::GetInstance()->m_pconfig->GetString(section_name, "adc1");
    m_pin2 = Printer::GetInstance()->m_pconfig->GetString(section_name, "adc2");
    m_dia1 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "Cal_dia1", 1.5);
    m_dia2 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "Cal_dia2", 2.0);
    m_rawdia1 = Printer::GetInstance()->m_pconfig->GetInt(section_name, "Raw_dia1", 9500);
    m_rawdia2 = Printer::GetInstance()->m_pconfig->GetInt(section_name, "Raw_dia2", 10500);
    m_MEASUREMENT_INTERVAL_MM = Printer::GetInstance()->m_pconfig->GetInt(section_name, "measurement_delay", 10);
    m_nominal_filament_dia = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "default_nominal_filament_diameter", DBL_MIN, DBL_MIN, DBL_MAX, 1);
    m_measurement_delay = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "measurement_delay", DBL_MIN, DBL_MIN, DBL_MAX, 0);
    m_measurement_max_difference = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_difference", 0.2);
    m_max_diameter = m_nominal_filament_dia + m_measurement_max_difference;
    m_min_diameter = m_nominal_filament_dia - m_measurement_max_difference;
    m_diameter = m_nominal_filament_dia;
    m_is_active = Printer::GetInstance()->m_pconfig->GetBool(section_name, "enable", false);
    m_runout_dia = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "min_diameter", 1.0);
    m_is_log = Printer::GetInstance()->m_pconfig->GetBool(section_name, "logging", false);
    // Use the current diameter instead of nominal while the first
    // measurement isn't in place
    m_use_current_dia_while_delay = Printer::GetInstance()->m_pconfig->GetBool(section_name, "use_current_dia_while_delay", false);
    m_lastFilamentWidthReading = 0;
    m_lastFilamentWidthReading2 = 0;
    m_firstExtruderUpdatePosition = 0;
    m_filament_width = m_nominal_filament_dia;

    // printer objects
    // self.toolhead = self.ppins = self.mcu_adc = None;
    Printer::GetInstance()->register_event_handler("klippy:ready:HallFilamentWidthSensor", std::bind(&HallFilamentWidthSensor::handle_ready, this));
    // Start adc
    m_mcu_adc = (MCU_adc*)Printer::GetInstance()->m_ppins->setup_pin("adc", m_pin1);
    m_mcu_adc->setup_minmax(ADC_SAMPLE_TIME, ADC_SAMPLE_COUNT);
    m_mcu_adc->setup_adc_callback(ADC_REPORT_TIME, std::bind(&HallFilamentWidthSensor::adc_callback, this, std::placeholders::_1, std::placeholders::_2));
    m_mcu_adc2 =  (MCU_adc*)Printer::GetInstance()->m_ppins->setup_pin("adc", m_pin2);
    m_mcu_adc2->setup_minmax(ADC_SAMPLE_TIME, ADC_SAMPLE_COUNT);
    m_mcu_adc2->setup_adc_callback(ADC_REPORT_TIME, std::bind(&HallFilamentWidthSensor::adc2_callbcak, this, std::placeholders::_1, std::placeholders::_2));
    // extrude factor updating
    m_extrude_factor_update_timer = Printer::GetInstance()->m_reactor->register_timer("extrude_factor_update_timer", std::bind(&HallFilamentWidthSensor::extrude_factor_update_event, this, std::placeholders::_1));
    // Register commands
    Printer::GetInstance()->m_gcode->register_command("QUERY_FILAMENT_WIDTH", 
                                std::bind(&HallFilamentWidthSensor::cmd_M407, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("RESET_FILAMENT_WIDTH_SENSOR",
                                std::bind(&HallFilamentWidthSensor::cmd_ClearFilamentArray, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("DISABLE_FILAMENT_WIDTH_SENSOR",
                                std::bind(&HallFilamentWidthSensor::cmd_M406, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("ENABLE_FILAMENT_WIDTH_SENSOR",
                                std::bind(&HallFilamentWidthSensor::cmd_M405, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("QUERY_RAW_FILAMENT_WIDTH",
                                std::bind(&HallFilamentWidthSensor::cmd_Get_Raw_Values, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("ENABLE_FILAMENT_WIDTH_LOG",
                                std::bind(&HallFilamentWidthSensor::cmd_log_enable, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("DISABLE_FILAMENT_WIDTH_LOG",
                                std::bind(&HallFilamentWidthSensor::cmd_log_disable, this, std::placeholders::_1));

    m_runout_helper = new RunoutHelper(section_name);

}

HallFilamentWidthSensor::~HallFilamentWidthSensor()
{
}

void HallFilamentWidthSensor::handle_ready()
{
    // Load printer objects
    // self.toolhead = self.printer.lookup_object('toolhead')

    // Start extrude factor update timer
    Printer::GetInstance()->m_reactor->update_timer(m_extrude_factor_update_timer,
                                Printer::GetInstance()->m_reactor->m_NOW); 
}

void HallFilamentWidthSensor::adc_callback(double read_time, double read_value)
{
    // read sensor value
    m_lastFilamentWidthReading = round(read_value * 10000);
}

void HallFilamentWidthSensor::adc2_callbcak(double read_time, double read_value)
{
    // read sensor value
    m_lastFilamentWidthReading2 = round(read_value * 10000);
    // calculate diameter
    double diameter_new = round(((m_dia2 - m_dia1)/
        (m_rawdia2 - m_rawdia1) *
        ((m_lastFilamentWidthReading + m_lastFilamentWidthReading2)
        - m_rawdia1) + m_dia1) * 100) / 100;
    m_diameter = (5.0 * m_diameter + diameter_new) / 6;
}

void HallFilamentWidthSensor::update_filament_array(double last_epos)
{
    // Fill array
    if (m_filament_array.size() > 0)
    {
        // Get last reading position in array & calculate next
        // reading position
        double next_reading_position = m_filament_array.back().first + m_MEASUREMENT_INTERVAL_MM;
        if (next_reading_position <= (last_epos + m_measurement_delay))
        {
            m_filament_array.push(make_pair(last_epos + m_measurement_delay, m_diameter));
        }
        
        if (m_is_log)
            Printer::GetInstance()->m_gcode->respond_info("Filament width:" + to_string(m_diameter), true);
    }
    else
    {
        // add first item to array
        m_filament_array.push(make_pair(m_measurement_delay + last_epos, m_diameter));
        m_firstExtruderUpdatePosition = (m_measurement_delay + last_epos);
    }
        
}

double HallFilamentWidthSensor::extrude_factor_update_event(double eventtime)
{
    // Update extrude factor
    std::vector<double> pos = Printer::GetInstance()->m_tool_head->get_position();
    double last_epos = pos[3];
    // Update filament array for lastFilamentWidthReading
    update_filament_array(last_epos);
    // Check runout
    m_runout_helper->note_filament_present(m_diameter > m_runout_dia);
    // Does filament exists
    if (m_diameter > 0.5)
    {
        if (m_filament_array.size() > 0)
        {
            // Get first position in filament array
            double pending_position = m_filament_array.front().first;
            if (pending_position <= last_epos)
            {
                // Get first item in filament_array queue
                std::pair<double, double>item = m_filament_array.front();
                m_filament_array.pop();
                m_filament_width = item.second;
            }
            else
            {
                if ((m_use_current_dia_while_delay) && (m_firstExtruderUpdatePosition == pending_position))
                {
                    m_filament_width = m_diameter;
                }
                else if(m_firstExtruderUpdatePosition == pending_position)
                {
                    m_filament_width = m_nominal_filament_dia;
                }
            }
            if ((m_filament_width <= m_max_diameter) && (m_filament_width >= m_min_diameter))
            {
                int percentage = round(m_nominal_filament_dia * m_nominal_filament_dia / m_filament_width * m_filament_width * 100);
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
    else
    {
        std::string cmd = "M221 S100";
        Printer::GetInstance()->m_gcode->run_script(cmd);
        std::queue<std::pair<double, double>>().swap(m_filament_array);
    }
    if (m_is_active)
        return eventtime + 1;
    else
        return Printer::GetInstance()->m_reactor->m_NEVER;
}

void HallFilamentWidthSensor::cmd_M407(GCodeCommand &gcmd)
{
    std::string response = "";
    if (m_diameter > 0)
    {
        response += ("Filament dia (measured mm): " + to_string(m_diameter));
    }
    else
    {
        response += "Filament NOT present";
    }
    gcmd.m_respond_info(response, true);
}

void HallFilamentWidthSensor::cmd_ClearFilamentArray(GCodeCommand &gcmd)
{
    std::queue<std::pair<double, double>>().swap(m_filament_array);
    gcmd.m_respond_info("Filament width measurements cleared!", true);
    // Set extrude multiplier to 100%
    std::string cmd = "M221 S100";
    Printer::GetInstance()->m_gcode->run_script(cmd);
}

void HallFilamentWidthSensor::cmd_M405(GCodeCommand &gcmd)
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

void HallFilamentWidthSensor::cmd_M406(GCodeCommand &gcmd)
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

void HallFilamentWidthSensor::cmd_Get_Raw_Values(GCodeCommand &gcmd)
{
    std::string response = "ADC1=";
    response +=  (" " + to_string(m_lastFilamentWidthReading));
    response +=  (" ADC2=" + to_string(m_lastFilamentWidthReading2));
    response +=  (" RAW=" + to_string(m_lastFilamentWidthReading + m_lastFilamentWidthReading2));
    gcmd.m_respond_info(response, true);
}

struct hall_status HallFilamentWidthSensor::get_status()
{
    struct hall_status ret;
    ret.Diameter = m_diameter;
    ret.Raw = m_lastFilamentWidthReading + m_lastFilamentWidthReading2;
    ret.is_active = m_is_active;
    return ret;
}

void HallFilamentWidthSensor::cmd_log_enable(GCodeCommand &gcmd)
{
    m_is_log = true;
    gcmd.m_respond_info("Filament width logging Turned On", true);
}

void HallFilamentWidthSensor::cmd_log_disable(GCodeCommand &gcmd)
{
    m_is_log = false;
    gcmd.m_respond_info("Filament width logging Turned Off", true);
}