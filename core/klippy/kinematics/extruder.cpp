#include "extruder.h"
#include "Define.h"
#include "klippy.h"
#include "my_math.h"

#include "config.h"

#define LOG_TAG "extruder"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

PrinterExtruder::PrinterExtruder(std::string section_name, int extruder_num)
{
    m_wait = false;
    m_name = section_name;
    std::string shared_heater = Printer::GetInstance()->m_pconfig->GetString("shared_heater", "");
    Printer::GetInstance()->load_object("heaters");
    std::string gcode_id = "T" + std::to_string(extruder_num);

    if (shared_heater == "")
        m_heater = Printer::GetInstance()->m_pheaters->setup_heater(section_name, gcode_id);
    else
        m_heater = Printer::GetInstance()->m_pheaters->lookup_heater(shared_heater);

    m_stepper = PrinterStepper(section_name);

    double filament_diameter = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "filament_diameter", DBL_MIN, m_nozzle_diameter);
    m_nozzle_diameter = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "nozzle_diameter", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    m_filament_area = M_PI * pow(filament_diameter * .5, 2);// 3.14 * (1.75 / 2)^2 = 2.4
    double def_max_cross_section = 4. * m_nozzle_diameter * m_nozzle_diameter;// 4 * 0.4 * 0.4 = 0.64
    double def_max_extrude_ratio = def_max_cross_section / m_filament_area; // 0.64 / 2.4 = 0.27
    double max_cross_section = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_extrude_cross_section", def_max_cross_section, DBL_MIN, DBL_MAX, 0.);
    m_max_extrude_ratio = max_cross_section / m_filament_area; // 0.64 / 2.4 = 0.27
    // logging.info("PrinterExtruder max_extrude_ratio=%.6f", m_max_extrude_ratio) //---??--- PrinterExtruder
    double max_velocity = Printer::GetInstance()->m_tool_head->get_max_velocity()[0];
    double max_accel = Printer::GetInstance()->m_tool_head->get_max_velocity()[1];
    m_max_e_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_extrude_only_velocity", max_velocity * def_max_extrude_ratio, DBL_MIN, DBL_MAX, 0.);
    // m_max_e_velocity = max_velocity * 0.27 = 270
    m_max_e_accel = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_extrude_only_accel", max_accel * def_max_extrude_ratio, DBL_MIN, DBL_MAX, 0.);
    // m_max_e_accel = max_accel * 0.27 = 5400
    m_max_e_dist = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_extrude_only_distance", 60.0f, 0.);
    m_instant_corner_v = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "instantaneous_corner_velocity", 1., 0.);
    m_pressure_advance = 0.;
    m_pressure_advance_smooth_time = 0.;
    double pressure_advance = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pressure_advance", 0., 0.);
    double smooth_time = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pressure_advance_smooth_time", 0.040, 0.0200, DBL_MAX, 0.);
    // Setup iterative solver

    m_trapq = trapq_alloc();
    m_sk_extruder = extruder_stepper_alloc();
    m_stepper->set_stepper_kinematics(m_sk_extruder);

    m_stepper->set_trapq(m_trapq);
    Printer::GetInstance()->m_tool_head->register_step_generator(std::bind(&MCU_stepper::generate_steps, m_stepper, std::placeholders::_1));
    // Register commands
    m_cmd_SET_PRESSURE_ADVANCE_help = "Set pressure advance parameters";
    m_cmd_SET_E_STEP_DISTANCE_help = "Set extruder step distance";
    m_cmd_ACTIVATE_EXTRUDER_help = "Change the active extruder";
    if (m_name == "extruder")
    {
        Printer::GetInstance()->m_tool_head->set_extruder(this, 0.);
        Printer::GetInstance()->m_gcode->register_command("M104", std::bind(&PrinterExtruder::cmd_M104, this, std::placeholders::_1));
        Printer::GetInstance()->m_gcode->register_command("M109", std::bind(&PrinterExtruder::cmd_M109, this, std::placeholders::_1));
        Printer::GetInstance()->m_gcode->register_mux_command("SET_PRESSURE_ADVANCE", "EXTRUDER", "", std::bind(&PrinterExtruder::cmd_default_SET_PRESSURE_ADVANCE, this, std::placeholders::_1), m_cmd_SET_PRESSURE_ADVANCE_help);
        Printer::GetInstance()->m_gcode->register_command("RESET_EXTRUDER", std::bind(&PrinterExtruder::cmd_RESET_EXTRUDER, this, std::placeholders::_1));
        Printer::GetInstance()->m_gcode->register_command("FLOW_CALIBRATION", std::bind(&PrinterExtruder::cmd_FLOW_CALIBRATION, this, std::placeholders::_1));
        Printer::GetInstance()->m_gcode->register_command("LOAD_FILAMENT", std::bind(&PrinterExtruder::cmd_LOAD_FILAMENT, this, std::placeholders::_1));
    }

    Printer::GetInstance()->m_gcode->register_command("M900", std::bind(&PrinterExtruder::cmd_M900, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_mux_command("SET_PRESSURE_ADVANCE", "EXTRUDER", m_name, std::bind(&PrinterExtruder::cmd_SET_PRESSURE_ADVANCE, this, std::placeholders::_1), m_cmd_SET_PRESSURE_ADVANCE_help);
    Printer::GetInstance()->m_gcode->register_mux_command("ACTIVATE_EXTRUDER", "EXTRUDER", m_name, std::bind(&PrinterExtruder::cmd_ACTIVATE_EXTRUDER, this, std::placeholders::_1), m_cmd_ACTIVATE_EXTRUDER_help);
    Printer::GetInstance()->m_gcode->register_mux_command("SET_EXTRUDER_STEP_DISTANCE", "EXTRUDER", m_name, std::bind(&PrinterExtruder::cmd_SET_E_STEP_DISTANCE, this, std::placeholders::_1), m_cmd_SET_E_STEP_DISTANCE_help);
    Printer::GetInstance()->m_gcode->register_command("SET_MIN_EXTRUDE_TEMP", std::bind(&PrinterExtruder::cmd_SET_MIN_EXTRUDE_TEMP, this, std::placeholders::_1));

    set_pressure_advance(pressure_advance, smooth_time);

    m_start_print_flag = false;
    m_flow_calibration = false;
    m_last_speed = 0;
    m_fan_timer = nullptr;
    // m_fan_timer = Printer::GetInstance()->m_reactor->register_timer(std::bind(&PrinterExtruder::set_fan_callback, this, std::placeholders::_1), get_monotonic() + PIN_MIN_TIME);
}

PrinterExtruder::~PrinterExtruder()
{
    if (m_trapq != NULL)
    {
        trapq_free(m_trapq);
    }
    if (m_sk_extruder != NULL)
    {
        free(m_sk_extruder);
    }
}

void PrinterExtruder::cmd_RESET_EXTRUDER(GCodeCommand &gcode)
{
    if (m_start_print_flag)
        reset_extruder();
}

void PrinterExtruder::reset_extruder()
{
    // m_start_print_flag = false;
    // std::cout << "reset extruder !" << std::endl;
    // trapq_free(m_trapq);
    // m_trapq = trapq_alloc();
    // free(m_sk_extruder);
    // m_sk_extruder = extruder_stepper_alloc();
    // m_stepper->set_stepper_kinematics(m_sk_extruder);
}

void PrinterExtruder::update_move_time(double flush_time)
{
    trapq_free_moves(m_trapq, flush_time);
}

void PrinterExtruder::set_pressure_advance(double pressure_advance, double smooth_time)
{
    double old_smooth_time = m_pressure_advance_smooth_time;
    if (m_pressure_advance <= 1e-15) //--IS_DOUBLE_ZERO----------
    {
        old_smooth_time = 0.0;
    }
    double new_smooth_time = smooth_time;
    if (pressure_advance <= 1e-15) //--IS_DOUBLE_ZERO----------
    {
        new_smooth_time = 0.0;
    }
    Printer::GetInstance()->m_tool_head->note_step_generation_scan_time(new_smooth_time * 0.5, old_smooth_time * 0.5);
    extruder_set_pressure_advance(m_sk_extruder, pressure_advance, new_smooth_time);
    m_pressure_advance = pressure_advance;
    m_pressure_advance_smooth_time = smooth_time;
}

void PrinterExtruder::get_status(double eventtime)
{
    // return dict(self.heater.get_status(eventtime),
    //         can_extrude=self.heater.can_extrude,
    //         pressure_advance=self.pressure_advance,
    //         smooth_time=self.pressure_advance_smooth_time) //---??--- PrinterExtruder
}

std::string PrinterExtruder::get_name()
{
    return m_name;
}

trapq *PrinterExtruder::get_trapq()
{
    return m_trapq;
}

Heater *PrinterExtruder::get_heater()
{
    return m_heater;
}

void PrinterExtruder::sync_stepper(MCU_stepper *stepper)
{
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    double epos = m_stepper->get_commanded_position();
    double pos[3] = {epos, 0., 0.};
    stepper->set_position(pos);
    stepper->set_trapq(m_trapq);
}

void PrinterExtruder::stats(double eventtime)
{
    // return self.heater.stats(eventtime); //---??--- PrinterExtruder
}

bool PrinterExtruder::check_move(Move &move) //--2-move-2task-G-G--UI_control_task--    //根据E轴移动相对距离限速
{
    double axis_r = move.m_axes_r[3];
    if (!m_heater->m_can_extrude && m_heater->m_can_extrude_switch)
    {
        // serial_error("Extrude below minimum temp\n"
        //              "See the 'min_extrude_temp' config option for details");
        return false;
    }
    if ((move.m_axes_d[0] == 0 && move.m_axes_d[1] == 0) || axis_r < 0) //--IS_DOUBLE_ZERO----------
    {
        if (fabs(move.m_axes_d[3]) > m_max_e_dist)
        {
            char errormsg[MAX_SERIAL_MSG_LENGTH];
            sprintf(errormsg, "Extrude only move too long (%.3fmm vs %.3fmm)\n"
                              "See the 'max_extrude_only_distance' config"
                              " option for details",
                    move.m_axes_d[3], m_max_e_dist);
            // serial_error(errormsg);
            return true;
        }
        double inv_extrude_r = 1.0 / fabs(axis_r);
        move.limit_speed(move, m_max_e_velocity * inv_extrude_r, m_max_e_accel * inv_extrude_r); // 根据E轴移动相对距离限速
    }
    else if (axis_r > m_max_extrude_ratio)
    {
        if (move.m_axes_d[3] <= m_nozzle_diameter * m_max_extrude_ratio)
        {
            // Permit extrusion if amount extruded is tiny
            return true;
        }
        double area = axis_r * m_filament_area;
        LOG_E("Overextrude: %f vs %f (area=%.3f dist=%.3f) \n",
               axis_r, m_max_extrude_ratio, area, move.m_move_d);
        char errormsg[MAX_SERIAL_MSG_LENGTH];
        sprintf(errormsg, "Move exceeds maximum extrusion (%.3fmm^2 vs %.3fmm^2)\n"
                          "See the 'max_extrude_cross_section' config option for details",
                (area, m_max_extrude_ratio * m_filament_area));
        // serial_error(errormsg);
        return false;
    }
    return true;
}

double PrinterExtruder::calc_junction(Move &prev_move, Move &move) // 根据E轴占比变化来求运动降速情况
{
    double diff_r = move.m_axes_r[3] - prev_move.m_axes_r[3];
    if (diff_r)
    {
        return std::min(move.m_max_cruise_v2, pow(m_instant_corner_v / fabs(diff_r), 2)); // 根据E轴占比变化来求运动降速情况
    }
    return move.m_max_cruise_v2; // 最大巡航速度平方
}

void PrinterExtruder::move(double print_time, Move &move) //--12-move-2task-G-G--UI_control_task--
{
    double axis_r = move.m_axes_r[3];
    double accel = move.m_accel * axis_r;
    double start_v = move.m_start_v * axis_r;
    double cruise_v = move.m_cruise_v * axis_r;
    // double pressure_advance = 0.0;
    int can_pressure_advance = 0;
    if (axis_r > 0 && (fabs(move.m_axes_d[0]) > 1e-15 || move.m_axes_d[1]))
    {
        // pressure_advance = m_pressure_advance;
        // pressure_advance = 1;
        can_pressure_advance = 1;
    }
    //  GAM_DEBUG_send_UI("2-129-\n" );
    // 队列运动（x是挤出机运动，y是压力推进)
    // std::cout << "print_time = " << print_time << std::endl;
    // std::cout << "move.m_accel_t = " << move.m_accel_t << std::endl;
    // std::cout << "move.m_cruise_t = " << move.m_cruise_t << std::endl;
    // std::cout << "move.m_decel_t = " << move.m_decel_t << std::endl;
    // std::cout << "move.m_start_pos[3] = " << move.m_start_pos[3] << std::endl;
    // std::cout << "pressure_advance = " << pressure_advance << std::endl;
    // std::cout << "start_v = " << start_v << std::endl;
    // std::cout << "cruise_v = " << cruise_v << std::endl;
    // std::cout << "accel = " << accel << std::endl;
    trapq_append(m_trapq, print_time, move.m_accel_t, move.m_cruise_t, move.m_decel_t, move.m_start_pos[3], 0, 0, 1, can_pressure_advance, 0, start_v, cruise_v, accel); //---12-2task-G-G--UI_control_task--
}

double PrinterExtruder::find_past_position(double print_time)
{
    return m_stepper->get_past_commanded_position(print_time);
}

void PrinterExtruder::cmd_M104(GCodeCommand &gcmd)
{
    // Set Extruder Temperature
    double temp = gcmd.get_double("S", 0.);
    int index = gcmd.get_int("T", INT32_MIN, 0);
    PrinterExtruder *extruder;
    if (index != INT32_MIN)
    {
        std::string section = "extruder";
        if (index)
            section = "extruder" + std::to_string(index);
        bool is_find = Printer::GetInstance()->lookup_object(section);
        if (!is_find)
        {
            if (temp <= 0.)
                return;
            std::cout << "Extruder not configured" << std::endl;
        }
        else
        {
            extruder = Printer::GetInstance()->m_tool_head->get_extruder();
        }
    }
    else
    {
        extruder = Printer::GetInstance()->m_tool_head->get_extruder();
    }
    if (m_fan_timer != nullptr)
    {
        Printer::GetInstance()->m_reactor->update_timer(Printer::GetInstance()->m_printer_extruder->m_fan_timer, Printer::GetInstance()->m_reactor->m_NOW);
    }
    Printer::GetInstance()->m_pheaters->set_temperature(extruder->get_heater(), temp, false);
}

double PrinterExtruder::set_fan_callback(double eventtime)
{
    double speed = 0;
    double target_temp = Printer::GetInstance()->m_pconfig->GetDouble("extruder", "model_fan_temp", 45);
    double fan_speed = 1;
    std::vector<double> temp = m_heater->get_temp(eventtime);
    if (temp[0] > target_temp)
        speed = fan_speed;
    if (speed != m_last_speed)
    {
        m_last_speed = speed;
        double curtime = get_monotonic();
        double print_time = Printer::GetInstance()->m_printer_fan->m_fan->get_mcu()->estimated_print_time(curtime);
        Printer::GetInstance()->m_printer_fan->m_fan->m_current_speed = speed;
        Printer::GetInstance()->m_printer_fan->m_fan->set_speed(print_time + PIN_MIN_TIME, speed);
    }
    return eventtime + 1.;
}

void PrinterExtruder::unregister_fan_callback()
{
    if (m_fan_timer != nullptr)
    {
        // Printer::GetInstance()->m_reactor->delay_unregister_timer(&Printer::GetInstance()->m_printer_extruder->m_fan_timer);
        Printer::GetInstance()->m_reactor->update_timer(Printer::GetInstance()->m_printer_extruder->m_fan_timer, Printer::GetInstance()->m_reactor->m_NEVER);
        m_last_speed = 0;
        double curtime = get_monotonic();
        double print_time = Printer::GetInstance()->m_printer_fan->m_fan->get_mcu()->estimated_print_time(curtime);
        Printer::GetInstance()->m_printer_fan->m_fan->m_current_speed = 0;
        Printer::GetInstance()->m_printer_fan->m_fan->set_speed(print_time + PIN_MIN_TIME, 0);
    }
}

void PrinterExtruder::cmd_M109(GCodeCommand &gcmd)
{

    // Set Extruder Temperature and Wait
    double temp = gcmd.get_double("S", 0.);
    int index = gcmd.get_int("T", INT32_MIN, 0);
    PrinterExtruder *extruder;
    if (index != INT32_MIN)
    {
        std::string section = "extruder";
        if (index)
            section = "extruder" + std::to_string(index);
        bool is_find = Printer::GetInstance()->lookup_object(section);
        if (!is_find)
        {
            if (temp <= 0.)
                return;
            std::cout << "Extruder not configured" << std::endl;
        }
        else
        {
            extruder = Printer::GetInstance()->m_tool_head->get_extruder();
        }
    }
    else
    {
        extruder = Printer::GetInstance()->m_tool_head->get_extruder();
    }
    if (m_fan_timer != nullptr)
    {
        Printer::GetInstance()->m_reactor->update_timer(Printer::GetInstance()->m_printer_extruder->m_fan_timer, Printer::GetInstance()->m_reactor->m_NOW);
    }
    Printer::GetInstance()->m_pheaters->set_temperature(extruder->get_heater(), temp, true);

}

void PrinterExtruder::cmd_default_SET_PRESSURE_ADVANCE(GCodeCommand &gcmd)
{
    PrinterExtruder *extruder = Printer::GetInstance()->m_tool_head->get_extruder();
    extruder->cmd_SET_PRESSURE_ADVANCE(gcmd);
}

void PrinterExtruder::cmd_SET_PRESSURE_ADVANCE(GCodeCommand &gcmd)
{
    double pressure_advance = gcmd.get_double("ADVANCE", m_pressure_advance, 0.);
    double smooth_time = gcmd.get_double("SMOOTH_TIME", m_pressure_advance_smooth_time, 0., 0.200);
    set_pressure_advance(pressure_advance, smooth_time);
    std::stringstream msg;
    msg << "pressure_advance: " << pressure_advance << "\n"
        << "pressure_advance_smooth_time: " << smooth_time;
    // Printer::GetInstance()->set_rollover_info(self.name, m_name + ": " + msg.str());
    // gcmd.m_respond_info(msg.str(), log=false);  //---??--- PrinterExtruder
}

void PrinterExtruder::cmd_M900(GCodeCommand &gcmd)
{
    double pressure_advance = gcmd.get_double("K", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "pressure_advance", 0.03), 0.);
    set_pressure_advance(pressure_advance, m_pressure_advance_smooth_time);
}

void PrinterExtruder::cmd_SET_E_STEP_DISTANCE(GCodeCommand &gcmd)
{
    double dist = gcmd.get_double("DISTANCE", DBL_MIN, DBL_MIN, DBL_MAX, 0.);
    std::stringstream msg;
    if (dist == DBL_MIN)
    {
        double step_dist = m_stepper->get_step_dist();
        msg << "Extruder " << m_name << " step distance is " << step_dist;
        // gcmd.m_respond_info(msg.str()); //---??--- PrinterExtruder
        return;
    }
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    m_stepper->set_step_dist(dist);
    msg << "Extruder " << m_name << " step distance is " << dist;
    // gcmd.m_respond_info(msg.str()); //---??--- PrinterExtruder
}

void PrinterExtruder::cmd_ACTIVATE_EXTRUDER(GCodeCommand &gcmd)
{
    if (Printer::GetInstance()->m_tool_head->get_extruder() == this)
    {
        // gcmd.m_respond_info("Extruder " + std::to_string(m_name) + " already active"); //---??--- PrinterExtruder
        return;
    }
    // gcmd.m_respond_info("Activating extruder " + std::to_string(m_name)); //---??--- PrinterExtruder
    Printer::GetInstance()->m_tool_head->flush_step_generation();
    Printer::GetInstance()->m_tool_head->set_extruder(this, m_stepper->get_commanded_position());
    Printer::GetInstance()->send_event("extruder:activate_extruder");
}

// std::string print_pa_lines(double start_x, double start_y, double start_pa, double step_pa, int num) {

//     // auto& writer = mp_gcodegen->writer();

//     Flow line_flow = Flow(m_line_width, 0.2, mp_gcodegen->config().nozzle_diameter.get_at(0));
//     Flow thin_line_flow = Flow(0.44, 0.2, mp_gcodegen->config().nozzle_diameter.get_at(0));
//     const double e_calib = line_flow.mm3_per_mm() / 2.40528; // filament_mm/extrusion_mm
//     const double e = thin_line_flow.mm3_per_mm() / 2.40528; // filament_mm/extrusion_mm

//     const double fast = m_fast_speed * 60.0;
//     const double slow = m_slow_speed * 60.0;
//     std::stringstream gcode;
//     gcode << mp_gcodegen->writer().travel_to_z(0.2);
//     double y_pos = start_y;

//     // prime line
//     auto prime_x = std::max(start_x - 5, 0.5);
//     gcode << move_to(Vec2d(prime_x, y_pos + (num - 4) * m_space_y));
//     gcode << writer.set_speed(slow);
//     gcode << writer.extrude_to_xy(Vec2d(prime_x, y_pos + 3 * m_space_y), e_calib * m_space_y * num * 1.1);

//     for (int i = 0; i < num; ++i) {

//         gcode << writer.set_pressure_advance(start_pa + i * step_pa);
//         gcode << move_to(Vec2d(start_x, y_pos + i * m_space_y));
//         gcode << writer.set_speed(slow);
//         gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short, y_pos + i * m_space_y), e_calib * m_length_short);
//         gcode << writer.set_speed(fast);
//         gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short + m_length_long, y_pos + i * m_space_y), e_calib * m_length_long);
//         gcode << writer.set_speed(slow);
//         gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short + m_length_long + m_length_short, y_pos + i * m_space_y), e_calib * m_length_short);

//     }
//     gcode << writer.set_pressure_advance(0.0);

//     if (m_draw_numbers) {
//         // draw indicator lines
//         gcode << writer.set_speed(fast);
//         gcode << move_to(Vec2d(start_x + m_length_short, y_pos + (num - 1) * m_space_y + 2));
//         gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short, y_pos + (num - 1) * m_space_y + 7), e * 7);
//         gcode << move_to(Vec2d(start_x + m_length_short + m_length_long, y_pos + (num - 1) * m_space_y + 7));
//         gcode << writer.extrude_to_xy(Vec2d(start_x + m_length_short + m_length_long, y_pos + (num - 1) * m_space_y + 2), e * 7);

//         for (int i = 0; i < num; i += 2) {
//             gcode << draw_number(start_x + m_length_short + m_length_long + m_length_short + 3, y_pos + i * m_space_y + m_space_y / 2, start_pa + i * step_pa);
//         }
//     }
//     return gcode.str();
// }

void PrinterExtruder::cmd_FLOW_CALIBRATION(GCodeCommand &gcmd)
{
#if 1
    Printer::GetInstance()->m_printer_extruder->reset_extruder();
    double position_endstop = Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "z_offset", DBL_MIN) + 0.2;
    double start_pa = gcmd.get_double("start_pa", 0);
    double start_x = gcmd.get_double("start_x", 20);
    double start_y = gcmd.get_double("start_y", 20);
    // double cur_pressure_val = recommend_val;
    // double start_pa = 0;
    int num = 8;
    char cmd[100];
    Printer::GetInstance()->m_gcode_io->single_command("G28");
    Printer::GetInstance()->m_gcode_io->single_command("M140 S60");
    Printer::GetInstance()->m_gcode_io->single_command("M104 S210");
    Printer::GetInstance()->m_gcode_io->single_command("M190 S60");
    Printer::GetInstance()->m_gcode_io->single_command("M109 S210");
    Printer::GetInstance()->m_gcode_io->single_command("G91");
    Printer::GetInstance()->m_gcode_io->single_command("G1 E10 F300");
    Printer::GetInstance()->m_gcode_io->single_command("G90");
    Printer::GetInstance()->m_gcode_io->single_command("M83");
    // sprintf(cmd, "G0 X%.2f Y%.2f", start_x, start_y);
    // Printer::GetInstance()->m_gcode_io->single_command(cmd);
    double line_length = 150;
    double line_space = 5;
    double cur_y = start_y;
    // double cur_e = 0;
    bool positive = true;
    double e_calib = 0.111416 / 2.40528;
    // for(int i = 0; i < num; i++)
    // {
    //     sprintf(cmd, "SET_PRESSURE_ADVANCE ADVANCE=%.2f", start_pa + (i * 0.01));
    //     Printer::GetInstance()->m_gcode_io->single_command(cmd);

    for (int i = 0; i < num; i++)
    {
        if (start_pa - 0 < 0.0000001)
        {
            sprintf(cmd, "SET_PRESSURE_ADVANCE ADVANCE=%d", 0);
        }
        else
        {
            sprintf(cmd, "SET_PRESSURE_ADVANCE ADVANCE=%.2f", start_pa);
        }
        std::cout << "extruder start pa = " << start_pa << std::endl;
        Printer::GetInstance()->m_gcode_io->single_command(cmd);
        start_pa += 0.01;
        cur_y = line_space * i + start_y;
        if (positive)
        {
            sprintf(cmd, "G0 X%.2f Y%.2f Z%.2f F3000", start_x, cur_y, position_endstop);
            std::cout << "1------" << cmd << std::endl;
            Printer::GetInstance()->m_gcode_io->single_command(cmd);
            sprintf(cmd, "G1 X%.2f Y%.2f E%.2f Z%.2f F3000", start_x + line_length, cur_y, e_calib * line_length, position_endstop);
            std::cout << "2------" << cmd << std::endl;
            Printer::GetInstance()->m_gcode_io->single_command(cmd);
            positive = !positive;
        }
        else
        {
            sprintf(cmd, "G0 X%.2f Y%.2f Z%.2f F3000", start_x + line_length, cur_y, position_endstop);
            std::cout << "1------" << cmd << std::endl;
            Printer::GetInstance()->m_gcode_io->single_command(cmd);
            sprintf(cmd, "G1 X%.2f Y%.2f E%.2f Z%.2f F3000", start_x, cur_y, e_calib * line_length, position_endstop);
            std::cout << "2------" << cmd << std::endl;
            Printer::GetInstance()->m_gcode_io->single_command(cmd);
            positive = !positive;
        }
    }
    Printer::GetInstance()->m_gcode_io->single_command("G0 Z10 F180");
    sprintf(cmd, "G0 X%.2f Y%.2f F3000", start_x + line_length + 10, start_y);
    Printer::GetInstance()->m_gcode_io->single_command(cmd);
    sprintf(cmd, "G0 Z%.2f F180", position_endstop);
    Printer::GetInstance()->m_gcode_io->single_command(cmd);
    sprintf(cmd, "SET_PRESSURE_ADVANCE ADVANCE=%d", 0);
    Printer::GetInstance()->m_gcode_io->single_command(cmd);
    std::vector<std::string> script;
    script.push_back("G1 E5 F2400");
    draw_digital(script, start_x + line_length + 10, start_y + line_space * 0 + 0.5 * line_space, '1');
    script.push_back("G1 E-5 F3600");
    // script.push_back("G1 Z.6 F9000");
    Printer::GetInstance()->m_gcode->run_script(script);
    std::vector<std::string>().swap(script);
    script.push_back("G1 E5 F2400");
    draw_digital(script, start_x + line_length + 10, start_y + line_space * 1 + 0.5 * line_space, '2');
    script.push_back("G1 E-5 F3600");
    // script.push_back("G1 Z.6 F9000");
    Printer::GetInstance()->m_gcode->run_script(script);
    std::vector<std::string>().swap(script);
    script.push_back("G1 E5 F2400");
    draw_digital(script, start_x + line_length + 10, start_y + line_space * 2 + 0.5 * line_space, '3');
    script.push_back("G1 E-5 F3600");
    // script.push_back("G1 Z.6 F9000");
    Printer::GetInstance()->m_gcode->run_script(script);
    std::vector<std::string>().swap(script);
    script.push_back("G1 E5 F2400");
    draw_digital(script, start_x + line_length + 10, start_y + line_space * 3 + 0.5 * line_space, '4');
    script.push_back("G1 E-5 F3600");
    // script.push_back("G1 Z.6 F9000");
    Printer::GetInstance()->m_gcode->run_script(script);
    std::vector<std::string>().swap(script);
    script.push_back("G1 E5 F2400");
    draw_digital(script, start_x + line_length + 10, start_y + line_space * 4 + 0.5 * line_space, '5');
    script.push_back("G1 E-5 F3600");
    // script.push_back("G1 Z.6 F9000");
    Printer::GetInstance()->m_gcode->run_script(script);
    std::vector<std::string>().swap(script);
    script.push_back("G1 E5 F2400");
    draw_digital(script, start_x + line_length + 10, start_y + line_space * 5 + 0.5 * line_space, '6');
    script.push_back("G1 E-5 F3600");
    // script.push_back("G1 Z.6 F9000");
    Printer::GetInstance()->m_gcode->run_script(script);
    std::vector<std::string>().swap(script);
    script.push_back("G1 E5 F2400");
    draw_digital(script, start_x + line_length + 10, start_y + line_space * 6 + 0.5 * line_space, '7');
    script.push_back("G1 E-5 F3600");
    // script.push_back("G1 Z.6 F9000");
    Printer::GetInstance()->m_gcode->run_script(script);
    std::vector<std::string>().swap(script);
    script.push_back("G1 E5 F2400");
    draw_digital(script, start_x + line_length + 10, start_y + line_space * 7 + 0.5 * line_space, '8');
    script.push_back("G1 E-5 F3600");
    // script.push_back("G1 Z.6 F9000");
    Printer::GetInstance()->m_gcode->run_script(script);
    std::vector<std::string>().swap(script);
    // }

    // if(0+0.01*4 - recommend_val < 0.000001)
    // {
    //     start_pa = 0;
    //     num = (int)(recommend_val / 0.01) + 3;
    // }
    // else if(1-4*0.01 - recommend_val < 0.000001)
    // {
    //     num = (int)(1-recommend_val / 0.01) + 3;
    //     start_pa = 1 - 0.01 * num;
    // }
    // else
    // {
    //     num = 7;
    //     start_pa = recommend_val - 0.01 * 3;
    // }

    // for(int i = 0; i < 4; i++)
    // {
    //     cur_pressure_val = recommend_val - (i * 0.01);
    //     std::cout << "cur_pressure_val = " << cur_pressure_val << std::endl;
    //     if(cur_pressure_val - 0 < 0.00001)
    //         break;
    //     sprintf(cmd, "SET_PRESSURE_ADVANCE ADVANCE=%.2f", cur_pressure_val);
    //     Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //     cur_y = line_space * i + start_y;
    //     for()
    //     if(positive)
    //     {
    //         sprintf(cmd, "G0 X%.2f Y%.2f Z%.2f F3000", start_x, cur_y, position_endstop);
    //         std::cout << "1------" << cmd << std::endl;
    //         Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //         sprintf(cmd, "G1 X%.2f Y%.2f E%.2f Z%.2f F3000", start_x + line_length, cur_y, cur_e, position_endstop);
    //         std::cout << "2------" << cmd << std::endl;
    //         Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //         positive = !positive;
    //     }
    //     else
    //     {
    //         sprintf(cmd, "G0 X%.2f Y%.2f Z%.2f F3000", start_x + line_length, cur_y, position_endstop);
    //         std::cout << "1------" << cmd << std::endl;
    //         Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //         sprintf(cmd, "G1 X%.2f Y%.2f E%.2f Z%.2f F3000", start_x, cur_y, cur_e, position_endstop);
    //         std::cout << "2------" << cmd << std::endl;
    //         Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //         positive = !positive;
    //     }

    // }
    // for(int i = 1; i < 4; i ++)
    // {
    //     cur_pressure_val = recommend_val + (i * 0.01);
    //     std::cout << "cur_pressure_val = " << cur_pressure_val << std::endl;
    //     cur_y = line_space * i + start_y;
    //     cur_e += line_length * 0.033848593;
    //     sprintf(cmd, "SET_PRESSURE_ADVANCE ADVANCE=%.2f", cur_pressure_val);
    //     Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //     if(positive)
    //     {
    //         sprintf(cmd, "G0 X%.2f Y%.2f Z%.2f", start_x, cur_y, position_endstop);
    //         std::cout << "3------" << cmd << std::endl;
    //         Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //         sprintf(cmd, "G1 X%.2f Y%.2f E%.2f Z%.2f", start_x + line_length, cur_y, cur_e, position_endstop);
    //         std::cout << "4------" << cmd << std::endl;
    //         Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //         positive = !positive;
    //     }
    //     else
    //     {
    //         sprintf(cmd, "G0 X%.2f Y%.2f Z%.2f", start_x + line_length, cur_y, position_endstop);
    //         std::cout << "3------" << cmd << std::endl;
    //         Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //         sprintf(cmd, "G1 X%.2f Y%.2f E%.2f Z%.2f", start_x, cur_y, cur_e, position_endstop);
    //         std::cout << "4------" << cmd << std::endl;
    //         Printer::GetInstance()->m_gcode_io->single_command(cmd);
    //         positive = !positive;
    //     }
    // }
    Printer::GetInstance()->m_gcode_io->single_command("G0 X110 Y110 Z50");
#endif
    m_flow_calibration = true;
}

#define LOAD_FILAMENT_STATE_CALLBACK_SIZE 16
static load_filament_state_callback_t load_filament_callback[LOAD_FILAMENT_STATE_CALLBACK_SIZE];

void PrinterExtruder::cmd_LOAD_FILAMENT(GCodeCommand &gcmd)
{
    load_filament_flag = false;
    load_filament_state_callback_call(LOAD_FILAMENT_STATE_START_HEAT);
    Printer::GetInstance()->m_gcode_io->single_command("M109 S%d", gcmd.get_int("TEMPERATURE", 215));
    load_filament_state_callback_call(LOAD_FILAMENT_STATE_HEAT_FINISH);
    // load_filament_state_callback_call(LOAD_FILAMENT_STATE_START_EXTRUDE);
    // Printer::GetInstance()->m_gcode_io->single_command("M83");
    // while (!load_filament_flag)
    // {
    //     Printer::GetInstance()->m_gcode_io->single_command("G1 E500 F%d", gcmd.get_int("SPEED", 300));
    //     Printer::GetInstance()->m_tool_head->wait_moves();
    // }
    // load_filament_state_callback_call(LOAD_FILAMENT_STATE_STOP_EXTRUDE);
    // Printer::GetInstance()->m_gcode_io->single_command("M104 S0");
    // Printer::GetInstance()->m_gcode_io->single_command("M82");
    // load_filament_state_callback_call(LOAD_FILAMENT_STATE_COMPLETE);
}

void PrinterExtruder::cmd_SET_MIN_EXTRUDE_TEMP(GCodeCommand &gcmd)
{
    double temp = gcmd.get_double("S", 0.);
    std::string temp_state = gcmd.get_string("RESET", "");
    if(temp_state == "RESET")
    {
        m_heater->m_min_extrude_temp = m_heater->last_min_extrude_temp;
        m_heater->m_can_extrude_switch = true;
    }
    else
    {
        m_heater->m_min_extrude_temp = temp;
        m_heater->m_can_extrude_switch = false;
    }
}

int load_filament_register_state_callback(load_filament_state_callback_t state_callback)
{
    for (int i = 0; i < LOAD_FILAMENT_STATE_CALLBACK_SIZE; i++)
    {
        if (load_filament_callback[i] == NULL)
        {
            load_filament_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}
int load_filament_state_callback_call(int state)
{
    for (int i = 0; i < LOAD_FILAMENT_STATE_CALLBACK_SIZE; i++)
    {
        if (load_filament_callback[i] != NULL)
        {
            load_filament_callback[i](state);
        }
    }
    return 0;
}

// Dummy extruder class used when a printer has no extruder at all
DummyExtruder::DummyExtruder()
{
}

DummyExtruder::~DummyExtruder()
{
}

void DummyExtruder::update_move_time(double flush_time)
{
}

void DummyExtruder::check_move(Move *move)
{
    // move.move_error("Extrude when no extruder present");
}

double DummyExtruder::find_past_position(double print_time)
{
    return 0.;
}

double DummyExtruder::calc_junction(Move *prev_move, Move *move)
{
    return move->m_max_cruise_v2;
}

std::string DummyExtruder::get_name()
{
    return "";
}

void DummyExtruder::get_heater()
{
    // Printer::GetInstance()->m_tool_head->command_error("Extruder not configured");
}

void add_printer_extruder()
{
    for (int i = 0; i < 99; i++)
    {
        std::stringstream section;
        section << "extruder";
        if (i)
        {
            section << i;
        }
        if (Printer::GetInstance()->m_pconfig->GetSection(section.str()) == nullptr)
        {
            break;
        }
        PrinterExtruder *extruder = new PrinterExtruder(section.str(), i);
        if (i == 0)
        {
            Printer::GetInstance()->m_printer_extruder = extruder; //
        }
        else
        {
            // extruder1, extruder2, extruder3...
        }
        Printer::GetInstance()->add_object(section.str(), extruder);
    }
}