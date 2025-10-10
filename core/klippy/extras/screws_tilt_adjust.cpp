#include "screws_tilt_adjust.h"
#include "klippy.h"
#include "my_string.h"

ScrewsTiltAdjust::ScrewsTiltAdjust(std::string section_name)
{
    m_max_diff = 0;
    // Read config
    for (int i = 0; i < 99; i++)
    {
        std::string prefix = "screw" + std::to_string(i + 1);
        std::string screw_coord_str = Printer::GetInstance()->m_pconfig->GetString(section_name, prefix);
        if (screw_coord_str == "")
        {
            break;
        }
        std::vector<std::string> screw_coord_xy = split(screw_coord_str, ",");
        std::vector<double> screw_coord = {stod(screw_coord_xy[0]), stod(screw_coord_xy[1])};
        std::string screw_name = Printer::GetInstance()->m_pconfig->GetString(section_name, prefix + "_name");
        m_screws_coord.push_back(screw_coord);
        m_screws_name.push_back(screw_name);
    }
    if (m_screws_coord.size() < 3)
    {
        // raise config.error("screws_tilt_adjust: Must have "
        //                     "at least three screws")
    }
    m_threads = {{"CW-M3", 0}, {"CCW-M3", 1}, {"CW-M4", 2}, {"CCW-M4", 3}, {"CW-M5", 4}, {"CCW-M5", 5}};
    m_thread = m_threads[Printer::GetInstance()->m_pconfig->GetString(section_name, "screw_thread", "CW-M3")];
    // Initialize ProbePointsHelper
    m_probe_helper = new ProbePointsHelper(section_name, std::bind(&ScrewsTiltAdjust::probe_finalize, this, std::placeholders::_1, std::placeholders::_2), m_screws_coord);
    m_probe_helper->minimum_points(3);
    // Register command
    m_cmd_SCREWS_TILT_CALCULATE_help = "Tool to help adjust bed leveling screws by calculating the number of turns to level it.";
    Printer::GetInstance()->m_gcode->register_command("SCREWS_TILT_CALCULATE", std::bind(&ScrewsTiltAdjust::cmd_SCREWS_TILT_CALCULATE, this, std::placeholders::_1), false, m_cmd_SCREWS_TILT_CALCULATE_help);
}

ScrewsTiltAdjust::~ScrewsTiltAdjust()
{
    
}

void ScrewsTiltAdjust::cmd_SCREWS_TILT_CALCULATE(GCodeCommand& gcmd)
{
    m_max_diff = gcmd.get_double("MAX_DEVIATION", DBL_MIN);
    // Option to force all turns to be in the given direction (CW or CCW)
    std::string direction = gcmd.get_string("DIRECTION", "");
    if (direction != "")
    {
        if (direction == "CW" || direction == "CCW")
        {
            // raise gcmd.error(
            //     "Error on "%s": DIRECTION must be either CW or CCW" % (
            //         gcmd.get_commandline(),))
        }
    }
    m_direction = direction;
    m_probe_helper->start_probe(gcmd);
}
        

std::string ScrewsTiltAdjust::probe_finalize(std::vector<double> offsets, std::vector<std::vector<double>> positions)
{
    // Factors used for CW-M3, CCW-M3, CW-M4, CCW-M4, CW-M5 and CCW-M5
    std::map<int, double> threads_factor = {{0, 0.5}, {1, 0.5}, {2, 0.7}, {3, 0.7}, {4, 0.8}, {5, 0.8}};
    bool is_clockwise_thread = ((m_thread & 1) == 0);
    std::vector<double> screw_diff;
    // Process the read Z values
    int i_base = 0;
    double z_base = 0.;
    if (m_direction != "")
    {
        // Lowest or highest screw is the base position used for comparison
        bool use_max = ((is_clockwise_thread && m_direction == "CW") || (!is_clockwise_thread && m_direction == "CCW"));
        std::vector<double> z_positions;
        for (auto position : positions)
        {
            z_positions.push_back(position[2]);
        }
        if (use_max)
        {
            z_base = *max_element(z_positions.begin(), z_positions.end());
            i_base = std::find(z_positions.begin(), z_positions.end(), z_base) - z_positions.begin();
        }
        else
        {
            z_base = *min_element(z_positions.begin(), z_positions.end());
            i_base = std::find(z_positions.begin(), z_positions.end(), z_base) - z_positions.begin();
        }
    }
    else
    {
        // First screw is the base position used for comparison
        i_base = 0;
        z_base = positions[0][2];
    }
        
    // Provide the user some information on how to read the results
    // self.gcode.respond_info("01:20 means 1 full turn and 20 minutes, "
    //                         "CW=clockwise, CCW=counter-clockwise")
    double z = 0.;
    for (int i = 0; i < m_screws_coord.size(); i++)
    {
        z = positions[i][2];
        std::vector<double> coord = m_screws_coord[i];  
        std::string name = m_screws_name[i];
        if (i == i_base)
        {
            // Show the results
            // self.gcode.respond_info(
            //     "%s : x=%.1f, y=%.1f, z=%.5f" %
            //     (name + " (base)", coord[0], coord[1], z))
        }
        else
        {
            // Calculate how knob must be adjusted for other positions
            double diff = z_base - z;
            double adjust = 0.;
            std::string sign = "";
            screw_diff.push_back(fabs(diff));
            if (fabs(diff) < 0.001)
                adjust = 0;
            else
                adjust = diff / threads_factor[m_thread];
            if (is_clockwise_thread)
            {
                if (adjust >= 0)
                    sign = "CW";
                else
                    sign = "CCW";
            }
            else
            {
                if (adjust >= 0)
                    sign = "CCW";
                else
                    sign = "CW";
            }
            adjust = fabs(adjust);
            double full_turns = std::trunc(adjust);
            double decimal_part = adjust - full_turns;
            double minutes = round(decimal_part * 60);
            // Show the results
            // self.gcode.respond_info(
            //     "%s : x=%.1f, y=%.1f, z=%.5f : adjust %s %02d:%02d" %
            //     (name, coord[0], coord[1], z, sign, full_turns, minutes))
        }
    }
    for (auto d : screw_diff)
    {
        if (m_max_diff && d > m_max_diff)
        {
            // aise m_gcode.error(
            // "bed level exceeds configured limits ({}mm)! " 
            // "Adjust screws and restart print.".format(self.max_diff))
        }
    }
}
        