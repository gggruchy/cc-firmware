#include "bed_tilt.h"
#include "klippy.h"
#include "Define.h"
#include "mathutil.h"

BedTilt::BedTilt(std::string section_name)
{
    Printer::GetInstance()->register_event_handler("klippy:connect::BedTilt", std::bind(&BedTilt::handle_connect, this));
    m_x_adjust = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "x_adjust", 0.);
    m_y_adjust = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "y_adjust", 0.);
    m_z_adjust = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_adjust", 0.);
    if ((Printer::GetInstance()->m_pconfig->GetString(section_name, "points", "") != ""))
    {
        BedTiltCalibrate(section_name, this);
    }
    // Register move transform with g-code class
    Printer::GetInstance()->m_gcode_move->set_move_transform(std::bind(&BedTilt::move, this, std::placeholders::_1, std::placeholders::_2));
    Printer::GetInstance()->m_gcode_move->set_get_position_transform(std::bind(&BedTilt::get_position, this));
}

BedTilt::~BedTilt()
{

}
        
void BedTilt::handle_connect()
{
    m_toolhead = Printer::GetInstance()->m_tool_head;
}
        
std::vector<double> BedTilt::get_position()
{
    std::vector<double> pos = m_toolhead->get_position();
    std::vector<double> ret = {pos[0], pos[1], pos[2] - pos[0] * m_x_adjust - pos[1] * m_y_adjust - m_z_adjust, pos[3]};
    return ret;
}
        
bool BedTilt::move(std::vector<double>& newpos, double speed)
{
    double pos[4] = {newpos[0], newpos[1], newpos[2] + newpos[0] * m_x_adjust + newpos[1] * m_y_adjust + m_z_adjust, newpos[3]};
    return m_toolhead->move1(pos, speed);
}
        
void BedTilt::update_adjust(double x_adjust, double y_adjust, double z_adjust)
{
    m_x_adjust = x_adjust;
    m_y_adjust = y_adjust;
    m_z_adjust = z_adjust;
    // gcode_move = self.printer.lookup_object('gcode_move')
    Printer::GetInstance()->m_gcode_move->reset_last_position();
    Printer::GetInstance()->m_pconfig->SetDouble("bed_tilt", "x_adjust", x_adjust);
    Printer::GetInstance()->m_pconfig->SetDouble("bed_tilt", "y_adjust", y_adjust);
    Printer::GetInstance()->m_pconfig->SetDouble("bed_tilt", "z_adjust", z_adjust);
}
        
// Helper script to calibrate the bed tilt
BedTiltCalibrate::BedTiltCalibrate(std::string section_name, BedTilt* bedtilt)
{
    m_bedtilt = bedtilt;
    m_probe_helper = new ProbePointsHelper(section_name, std::bind(&BedTiltCalibrate::probe_finalize, this, std::placeholders::_1, std::placeholders::_2));
    m_probe_helper->minimum_points(3);
    // Register BED_TILT_CALIBRATE command
    m_cmd_BED_TILT_CALIBRATE_help = "Bed tilt calibration script";
    Printer::GetInstance()->m_gcode->register_command("BED_TILT_CALIBRATE", std::bind(&BedTiltCalibrate::cmd_BED_TILT_CALIBRATE, this, std::placeholders::_1), false,  m_cmd_BED_TILT_CALIBRATE_help);
}

BedTiltCalibrate::~BedTiltCalibrate()
{

}
        
    
void BedTiltCalibrate::cmd_BED_TILT_CALIBRATE(GCodeCommand& gcmd)
{
    m_probe_helper->start_probe(gcmd);
}

std::string BedTiltCalibrate::probe_finalize(std::vector<double> offsets, std::vector<std::vector<double>> positions)
{
    // Setup for coordinate descent analysis
    double z_offset = offsets[2];
    // logging.info("Calculating bed_tilt with: %s", positions)
    std::map<std::string, double> params;
    params["x_adjust"] = m_bedtilt->m_x_adjust;
    params["y_adjust"] = m_bedtilt->m_y_adjust;
    params["z_adjust"] = z_offset;
    // logging.info("Initial bed_tilt parameters: %s", params)
    std::map<std::string, double> new_params = coordinate_descent(params, positions);
    // Update current bed_tilt calculations
    double x_adjust = new_params["x_adjust"];
    double y_adjust = new_params["y_adjust"];
    double z_adjust = (new_params["z_adjust"] - z_offset - x_adjust * offsets[0] - y_adjust * offsets[1]);
    m_bedtilt->update_adjust(x_adjust, y_adjust, z_adjust);
    // Log and report results
    // logging.info("Calculated bed_tilt parameters: %s", new_params)
    for (auto pos : positions)
    {
        // logging.info("orig: %s new: %s", adjusted_height(pos, params), adjusted_height(pos, new_params))
    }
    std::string msg = "x_adjust: " + std::to_string(x_adjust) + "y_adjust: " + std::to_string(y_adjust) + "z_adjust: " + std::to_string(z_adjust);
    // self.printer.set_rollover_info("bed_tilt", "bed_tilt: %s" % (msg,))
    // self.gcode.respond_info(
    //     "%s\nThe above parameters have been applied to the current\n"
    //     "session. The SAVE_CONFIG command will update the printer\n"
    //     "config file and restart the printer." % (msg,))
}