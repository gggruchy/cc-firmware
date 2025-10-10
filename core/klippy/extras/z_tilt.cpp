#include "z_tilt.h"
#include "klippy.h"
#include "mathutil.h"
#include "my_string.h"

ZAdjustHelper::ZAdjustHelper(std::string section_name , int z_count)
{
    m_name = section_name;
    m_z_count = z_count;
    Printer::GetInstance()->register_event_handler("klippy:connect::ZAdjustHelper", std::bind(&ZAdjustHelper::handle_connect, this));
}

ZAdjustHelper::~ZAdjustHelper()
{

}
        
void ZAdjustHelper::handle_connect()
{
    Kinematics *kin = Printer::GetInstance()->m_tool_head->get_kinematics();
    std::vector<std::vector<MCU_stepper*>> steppers = kin->get_steppers();
    std::vector<MCU_stepper*> z_steppers;
    for (int i = 0; i < steppers.size(); i++)
    {
        for (int j = 0; j < steppers[i].size(); j++)
        {
            if (steppers[i][j]->is_active_axis('z'))
                z_steppers.push_back(steppers[i][j]);
        }
    }
    if (z_steppers.size() != m_z_count)
    {
        // raise self.printer.config_error( "%s z_positions needs exactly %d items" % ( self.name, len(z_steppers)))
    }
    if (z_steppers.size() < 2)
    {
        // raise self.printer.config_error("%s requires multiple z steppers" % (self.name,));
    }
    m_z_steppers = z_steppers;
}
        
void ZAdjustHelper::adjust_steppers(std::vector<double> adjustments, double speed)
{
    std::vector<double> curpos = Printer::GetInstance()->m_tool_head->get_position();
    // Report on movements
    // stepstrs = ["%s = %.6f" % (s.get_name(), a) for s, a in zip(self.z_steppers, adjustments)]
    // msg = "Making the following Z adjustments:\n%s" % ("\n".join(stepstrs),)
    // gcode.respond_info(msg)
    // Disable Z stepper movements
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    for (auto stepper : m_z_steppers)
    {
        stepper->set_trapq(nullptr);
    }
    // Move each z stepper (sorted from lowest to highest) until they match
    std::map<double, MCU_stepper*> positions;
    for (int i = 0; i < adjustments.size(); i++)
    {
        positions[-adjustments[i]] = m_z_steppers[i];
    }
    std::map<double, MCU_stepper*>::iterator it = positions.begin();
    double first_stepper_offset = it->first;
    MCU_stepper* first_stepper = it->second;
    double z_low = curpos[2] - first_stepper_offset;
    for (std::map<double, MCU_stepper*>::iterator iter = positions.begin(); iter != positions.end(); iter++)
    {
        double stepper_offset = iter->first;
        MCU_stepper* stepper = iter->second;
        iter++;
        double next_stepper_offset = iter->first;
        MCU_stepper* next_stepper = iter->second;
        iter--;
        Printer::GetInstance()->m_tool_head->flush_step_generation();
        stepper->set_trapq(Printer::GetInstance()->m_tool_head->get_trapq());
        curpos[2] = z_low + next_stepper_offset;
        Printer::GetInstance()->m_tool_head->move(curpos, speed);
        Printer::GetInstance()->m_tool_head->set_position(curpos);
    }
    // Z should now be level - do final cleanup
    it = positions.end();
    it--;
    double last_stepper_offset = it->first;
    MCU_stepper* last_stepper = it->second;
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    last_stepper->set_trapq(Printer::GetInstance()->m_tool_head->get_trapq());
    curpos[2] += first_stepper_offset;
    Printer::GetInstance()->m_tool_head->set_position(curpos);
}
        

ZAdjustStatus::ZAdjustStatus()
{
    bool m_applied = false;
    Printer::GetInstance()->register_event_double_handler("stepper_enable:motor_off::ZAdjustStatus", std::bind(&ZAdjustStatus::_motor_off, this, std::placeholders::_1));
}

ZAdjustStatus::~ZAdjustStatus()
{

}

std::string ZAdjustStatus::check_retry_result(std::string retry_result)
{
    if (retry_result == "done")
        m_applied = true;
    return retry_result;
}
        
std::string ZAdjustStatus::reset()
{
    m_applied = false;
}
        
std::string ZAdjustStatus::get_status(double eventtime)
{
    // return {'applied': self.applied}
}
        
void ZAdjustStatus::_motor_off(double print_time)
{
    reset();
}

RetryHelper::RetryHelper(std::string section_name, std::string error_msg_extra)
{
    m_section_name = section_name;
    m_default_max_retries = Printer::GetInstance()->m_pconfig->GetInt(section_name, "retries", 0, 0);
    m_default_retry_tolerance = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "retry_tolerance", 0., DBL_MIN, DBL_MAX, 0.);
    m_value_label = "Probed points range";
    m_error_msg_extra = error_msg_extra;
}

RetryHelper::~RetryHelper()
{

}
        
void RetryHelper::start(GCodeCommand& gcmd)
{
    m_max_retries = Printer::GetInstance()->m_pconfig->GetInt(m_section_name, "RETRIES", m_default_max_retries, 0, 30);
    m_retry_tolerance = Printer::GetInstance()->m_pconfig->GetDouble(m_section_name, "RETRY_TOLERANCE", m_default_retry_tolerance, 0.0, 1.0);
    m_current_retry = 0;
    m_previous = 0.;
    m_increasing = 0;
}
        
bool RetryHelper::check_increase(double error)
{
    if (m_previous && error > m_previous + 0.0000001)
        m_increasing += 1;
    else if (m_increasing > 0)
        m_increasing -= 1;
    m_previous = error;
    return m_increasing > 1;
}
        
std::string RetryHelper::check_retry(std::vector<double> z_positions)
{
    if (m_max_retries == 0)
        return "";
    double error = round(*max_element(z_positions.begin(), z_positions.end()) - *min_element(z_positions.begin(), z_positions.end()));
    // self.gcode.respond_info(
    //     "Retries: %d/%d %s: %0.6f tolerance: %0.6f" % (
    //         self.current_retry, self.max_retries, self.value_label,
    //         error, self.retry_tolerance))
    if (check_increase(error))
    {
        // raise self.gcode.error("Retries aborting: %s is increasing. %s"
        //                         % (self.value_label, self.error_msg_extra))
    }
    if (error <= m_retry_tolerance)
        return "done";
    m_current_retry += 1;
    if (m_current_retry > m_max_retries)
    {
        // raise self.gcode.error("Too many retries")
    }
    return "retry";
}
        
ZTilt::ZTilt(std::string section_name)
{
    std::vector<std::string> z_positions = split(Printer::GetInstance()->m_pconfig->GetString(section_name, "z_positions"), "\n");
    for (auto pos_str : z_positions)
    {
        std::vector<std::string> z_pos_str = split(pos_str, ",");
        std::vector<double> z_pos = {stod(z_pos_str[0]), stod(z_pos_str[1])};
        m_z_positions.push_back(z_pos);
    }
    m_retry_helper = new RetryHelper(section_name);
    m_probe_helper = new ProbePointsHelper(section_name, std::bind(&ZTilt::probe_finalize, this, std::placeholders::_1, std::placeholders::_2));
    m_probe_helper->minimum_points(2);
    m_z_status = new ZAdjustStatus();
    m_z_helper = new ZAdjustHelper(section_name, m_z_positions.size());
    // Register Z_TILT_ADJUST command
    m_cmd_Z_TILT_ADJUST_help = "Adjust the Z tilt";
    Printer::GetInstance()->m_gcode->register_command("Z_TILT_ADJUST", std::bind(&ZTilt::cmd_Z_TILT_ADJUST, this, std::placeholders::_1), false, m_cmd_Z_TILT_ADJUST_help);
}

ZTilt::~ZTilt()
{

}

void ZTilt::cmd_Z_TILT_ADJUST(GCodeCommand& gcmd)
{
    m_z_status->reset();
    m_retry_helper->start(gcmd);
    m_probe_helper->start_probe(gcmd);
}
        
std::string ZTilt::probe_finalize(std::vector<double> offsets, std::vector<std::vector<double>> positions)
{
    // Setup for coordinate descent analysis
    double z_offset = offsets[2];
    // logging.info("Calculating bed tilt with: %s", positions)
    std::map<std::string, double> params = {{"x_adjust", 0.}, {"y_adjust", 0.}, {"z_adjust", z_offset}};

    std::map<std::string, double> new_params = coordinate_descent(params, positions);
    // Apply results
    double speed = m_probe_helper->get_lift_speed();
    // logging.info("Calculated bed tilt parameters: %s", new_params)
    double x_adjust = new_params["x_adjust"];
    double y_adjust = new_params["y_adjust"];
    double z_adjust = (new_params["z_adjust"] - z_offset - x_adjust * offsets[0] - y_adjust * offsets[1]);
    std::vector<double> adjustments;
    for (auto pos: m_z_positions)
    {
        adjustments.push_back(pos[0]*x_adjust + pos[1]*y_adjust + z_adjust);
    }
    m_z_helper->adjust_steppers(adjustments, speed);
    std::vector<double> postions_z;
    for (auto pos : positions)
    {
        postions_z.push_back(pos[2]);
    }
    return m_z_status->check_retry_result(m_retry_helper->check_retry(postions_z));
}
        
std::string ZTilt::get_status(double eventtime)
{
    return m_z_status->get_status(eventtime);
}
            
