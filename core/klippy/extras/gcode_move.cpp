#include "gcode_move.h"
#include "klippy.h"
#define LOG_TAG "gcode_move"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

GCodeMove::GCodeMove(std::string section_name)
{
    Printer::GetInstance()->register_event_handler("klippy:ready:GCodeMove", std::bind(&GCodeMove::_handle_ready, this));
    Printer::GetInstance()->register_event_handler("klippy:shutdown:GCodeMove", std::bind(&GCodeMove::_handle_shutdown, this));
    Printer::GetInstance()->register_event_handler("toolhead:set_position:GCodeMove", std::bind(&GCodeMove::reset_last_position, this));
    Printer::GetInstance()->register_event_handler("toolhead:manual_move:GCodeMove", std::bind(&GCodeMove::reset_last_position, this));
    Printer::GetInstance()->register_event_handler("gcode:command_error:GCodeMove", std::bind(&GCodeMove::reset_last_position, this));
    Printer::GetInstance()->register_event_handler("extruder:activate_extruder:GCodeMove", std::bind(&GCodeMove::_handle_activate_extruder, this));
    Printer::GetInstance()->register_event_homing_handler("homing:home_rails_end:GCodeMove", std::bind(&GCodeMove::_handle_home_rails_end, this, std::placeholders::_1, std::placeholders::_2));
    m_is_printer_ready = false;
    m_absolute_coord = true;
    m_absolute_extrude = true;
    m_base_position = {0.0, 0.0, 0.0, 0.0};
    m_last_position = {0.0, 0.0, 0.0, 0.0};
    m_homing_position = {0.0, 0.0, 0.0, 0.0};
    m_extrude_factor = 1.;
    m_speed = 25.;
    m_speed_factor = 1. / 60.;
    m_move_transform == nullptr;

    std::vector<std::string> handlers = {"G1", "G20", "G21", "M82", "M83", "G90", "G91", "G92", "M220", "M221", "SET_GCODE_OFFSET", "SAVE_GCODE_STATE", "RESTORE_GCODE_STATE"};
    std::vector<std::function<void(GCodeCommand &)>> funcs;
    funcs.push_back(std::bind(&GCodeMove::cmd_G1, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_G20, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_G21, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_M82, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_M83, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_G90, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_G91, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_G92, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_M220, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_M221, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_SET_GCODE_OFFSET, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_SAVE_GCODE_STATE, this, std::placeholders::_1));
    funcs.push_back(std::bind(&GCodeMove::cmd_RESTORE_GCODE_STATE, this, std::placeholders::_1));

    for (int i = 0; i < handlers.size(); i++)
    {
        std::string desc = "cmd_" + handlers[i] + "_help";
        Printer::GetInstance()->m_gcode->register_command(handlers[i], funcs[i], true, desc);
    }
    cmd_GET_POSITION_help = "Return information on the current location of the toolhead";
    Printer::GetInstance()->m_gcode->register_command("G0", std::bind(&GCodeMove::cmd_G1, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M114", std::bind(&GCodeMove::cmd_M114, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("GET_POSITION", std::bind(&GCodeMove::cmd_GET_POSITION, this, std::placeholders::_1), false, cmd_GET_POSITION_help);
    Printer::GetInstance()->m_gcode->register_command("MANUAL_MOVE", std::bind(&GCodeMove::cmd_MANUAL_MOVE, this, std::placeholders::_1));
}

GCodeMove::~GCodeMove()
{
}

void GCodeMove::_handle_ready()
{
    m_is_printer_ready = true;
    if (m_move_transform == nullptr)
    {
        m_move_transform = std::bind(&ToolHead::move, Printer::GetInstance()->m_tool_head, std::placeholders::_1, std::placeholders::_2);
        m_get_position_transform = std::bind(&ToolHead::get_position, Printer::GetInstance()->m_tool_head);
    }
    reset_last_position();
}

void GCodeMove::_handle_shutdown()
{
    if (!m_is_printer_ready)
        return;
    m_is_printer_ready = false;
    // logging.info("gcode state: m_absolute_coord=%s m_absolute_extrude=%s"
    //                 " m_base_position=%s m_last_position=%s m_homing_position=%s"
    //                 " m_speed_factor=%s m_extrude_factor=%s speed=%s",
    //                 self.m_absolute_coord, self.m_absolute_extrude,
    //                 self.m_base_position, self.m_last_position,
    //                 self.m_homing_position, self.m_speed_factor,
    //                 self.m_extrude_factor, self.speed)
}

void GCodeMove::_handle_activate_extruder()
{
    reset_last_position();
    m_extrude_factor = 1.;
    m_base_position[3] = m_last_position[3];
}

void GCodeMove::_handle_home_rails_end(Homing *homing_state, std::vector<PrinterRail *> rails)
{
    reset_last_position();
    std::vector<int> axes = homing_state->get_axes();
    for (auto axis : axes)
    {
        m_base_position[axis] = m_homing_position[axis];
        // printf("GCodeMove::_handle_home_rails_end: m_base_position[%d] = %f\n", axis, m_base_position[axis]);
        // printf("GCodeMove::_handle_home_rails_end: m_homing_position[%d] = %f\n", axis, m_homing_position[axis]);
        // printf("GCodeMove::_handle_home_rails_end: m_last_position[%d] = %f\n", axis, m_last_position[axis]);
    }
}

std::function<bool(std::vector<double> &, double)> GCodeMove::set_move_transform(std::function<bool(std::vector<double> &, double)> move_fun, bool force)
{
    if (m_move_transform != nullptr && !force)
    {
        std::cout << "G-Code move transform already specified" << std::endl;
    }
    std::function<bool(std::vector<double> &, double)> old_transform = m_move_transform;
    if (old_transform == nullptr)
        old_transform = std::bind(&ToolHead::move, Printer::GetInstance()->m_tool_head, std::placeholders::_1, std::placeholders::_2);
    m_move_transform = move_fun;
    return old_transform;
}

std::function<std::vector<double>()> GCodeMove::set_get_position_transform(std::function<std::vector<double>()> get_position_fun, bool force)
{
    if (m_get_position_transform != nullptr && !force)
    {
        std::cout << "G-Code move transform already specified" << std::endl;
    }
    std::function<std::vector<double>()> old_transform = m_get_position_transform;
    if (old_transform == nullptr)
        old_transform = std::bind(&ToolHead::get_position, Printer::GetInstance()->m_tool_head);
    m_get_position_transform = get_position_fun;
    return old_transform;
}

gcode_move_state_t GCodeMove::get_status(double eventtime)
{
    gcode_move_state_t ret = {
        .absolute_coord = m_absolute_coord,
        .absolute_extrude = m_absolute_extrude,
        .speed = get_gcode_speed(),
        .speed_factor = get_gcode_speed_override(),
        .extrude_factor = m_extrude_factor,
        .last_position = m_last_position,
        .homing_position = m_homing_position,
        .base_position = get_gcode_position()};
    return ret;
}

void GCodeMove::reset_last_position()
{
    if (m_is_printer_ready)
    {
        m_last_position = m_get_position_transform();
    }
}

void GCodeMove::cmd_G1(GCodeCommand &gcmd)
{
    std::vector<double> last_position = m_last_position;
    std::map<std::string, std::string> params;
    gcmd.get_command_parameters(params);
    std::vector<std::string> axiss = {"X", "Y", "Z"};
    for (int i = 0; i < axiss.size(); i++)
    {
        auto iter = params.find(axiss[i]);
        if (iter != params.end())
        {
            if (!m_absolute_coord)
            {
                m_last_position[i] += stod(iter->second);
            }
            else
            {
                // printf("cmd_G1 axis: %d m_base_position: %f aim_position: %f m_last_position: %f\n", i, m_base_position[i], stod(iter->second), m_last_position[i]);
                m_last_position[i] = stod(iter->second) + m_base_position[i]; // 给定参数加上基准位置，得到当前目标位置，也相当于上一次的位置。待会会将这坐标作为目标位置下发
            }
        }
    }
    auto iter_E = params.find("E");
    if (iter_E != params.end())
    {
        double value = stod(iter_E->second) * m_extrude_factor;
        if (!m_absolute_coord || !m_absolute_extrude)
        {
            m_last_position[AXIS_E] += value;
        }
        else
        {
            m_last_position[AXIS_E] = value + m_base_position[AXIS_E];
        }
    }
    auto iter_F = params.find("F");
    if (iter_F != params.end())
    {
        double gcode_speed = stod(iter_F->second);
        m_speed = gcode_speed * m_speed_factor;
        if(m_speed <= 0.001)            //避免速度为0导致后面死机
        {
            LOG_E("ERROR: G1 (speed here) m_speed_factor:%f\n",m_speed_factor);
            m_speed = 40;
        }
    }
    std::vector<double> position = {m_last_position[AXIS_X], m_last_position[AXIS_Y], m_last_position[AXIS_Z], m_last_position[AXIS_E]};

    if(Printer::GetInstance()->m_change_filament->is_active() && Printer::GetInstance()->m_virtual_sdcard->is_active())
    {
        if(!Printer::GetInstance()->m_change_filament->check_move(position))
        {
            // m_last_position = last_position;
            // Printer::GetInstance()->m_gcode_io->single_command("PAUSE");
            // Printer::GetInstance()->m_virtual_sdcard->m_pending_command.push_back(gcmd);
            // return;
        }
    }

    if (!m_move_transform(position, m_speed)) // 如果配置有bed_mesh，那么MOVE函数指向BedMesh::move
    {
        m_last_position = last_position;
    }
}

void GCodeMove::cmd_MANUAL_MOVE(GCodeCommand &gcmd)
{
    std::vector<double> last_position = m_last_position;
    std::map<std::string, std::string> params;
    gcmd.get_command_parameters(params);
    std::vector<std::string> axiss = {"X", "Y", "Z"};
    for (int i = 0; i < axiss.size(); i++)
    {
        auto iter = params.find(axiss[i]);
        if (iter != params.end())
        {
            if (!m_absolute_coord)
            {
                m_last_position[i] += stod(iter->second);
            }
            else
            {
                // printf("cmd_G1 axis: %d m_base_position: %f aim_position: %f m_last_position: %f\n", i, m_base_position[i], stod(iter->second), m_last_position[i]);
                m_last_position[i] = stod(iter->second) + m_base_position[i]; // 给定参数加上基准位置，得到当前目标位置，也相当于上一次的位置。待会会将这坐标作为目标位置下发
            }
        }
    }
    auto iter_E = params.find("E");
    if (iter_E != params.end())
    {
        double value = stod(iter_E->second) * m_extrude_factor;
        if (!m_absolute_coord || !m_absolute_extrude)
        {
            m_last_position[AXIS_E] += value;
        }
        else
        {
            m_last_position[AXIS_E] = value + m_base_position[AXIS_E];
        }
    }
    auto iter_F = params.find("F");
    if (iter_F != params.end())
    {
        double gcode_speed = stod(iter_F->second);
        m_speed = gcode_speed * m_speed_factor;
        if(m_speed <= 0.001)            //避免速度为0导致后面死机
        {
            LOG_E("G1 F%d m_speed_factor:%f\n",(int)gcode_speed,m_speed_factor);
            m_speed = 40;
        }
    }

    // 移动轴软限位
    m_last_position[0] = m_last_position[0] > 256. ? 256. : m_last_position[0];
    m_last_position[0] = m_last_position[0] < 0. ? 0. : m_last_position[0];
    m_last_position[1] = m_last_position[1] > 265. ? 265. : m_last_position[1];
    m_last_position[1] = m_last_position[1] < 0. ? 0. : m_last_position[1];
    m_last_position[2] = m_last_position[2] > 256. ? 256. : m_last_position[2];
    m_last_position[2] = m_last_position[2] < 0. ? 0. : m_last_position[2];
    
    std::vector<double> position = {m_last_position[AXIS_X], m_last_position[AXIS_Y], m_last_position[AXIS_Z], m_last_position[AXIS_E]};

    if(Printer::GetInstance()->m_change_filament->is_active() && Printer::GetInstance()->m_virtual_sdcard->is_active())
    {
        if(!Printer::GetInstance()->m_change_filament->check_move(position))
        {
            // m_last_position = last_position;
            // Printer::GetInstance()->m_gcode_io->single_command("PAUSE");
            // Printer::GetInstance()->m_virtual_sdcard->m_pending_command.push_back(gcmd);
            // return;
        }
    }

    if (!m_move_transform(position, m_speed)) // 如果配置有bed_mesh，那么MOVE函数指向BedMesh::move
    {
        m_last_position = last_position;
    }
}

void GCodeMove::cmd_G20(GCodeCommand &gcmd)
{
    // machine does not support G20 (inches) command
}

void GCodeMove::cmd_G21(GCodeCommand &gcmd)
{
    // pass
}

void GCodeMove::cmd_M82(GCodeCommand &gcmd)
{
    m_absolute_extrude = true;
}

void GCodeMove::cmd_M83(GCodeCommand &gcmd)
{
    m_absolute_extrude = false;
}

void GCodeMove::cmd_G90(GCodeCommand &gcmd)
{
    m_absolute_coord = true;
}

void GCodeMove::cmd_G91(GCodeCommand &gcmd)
{
    m_absolute_coord = false;
}

void GCodeMove::cmd_G92(GCodeCommand &gcmd)
{
    std::vector<std::string> axiss = {"X", "Y", "Z", "E"};
    std::map<std::string, std::string> params;
    gcmd.get_command_parameters(params);
    for (int i = 0; i < axiss.size(); i++)
    {
        auto iter = params.find(axiss[i]);
        if (iter != params.end())
        {
            m_base_position[i] = m_last_position[i] - gcmd.get_double(axiss[i], 0);
        }
    }
}

std::vector<double> GCodeMove::get_gcode_position()
{
    std::vector<double> cur_pos;
    cur_pos.push_back(m_last_position[AXIS_X] - m_base_position[AXIS_X]);
    cur_pos.push_back(m_last_position[AXIS_Y] - m_base_position[AXIS_Y]);
    cur_pos.push_back(m_last_position[AXIS_Z] - m_base_position[AXIS_Z]);
    cur_pos.push_back((m_last_position[AXIS_E] - m_base_position[AXIS_E]) / m_extrude_factor);
    return cur_pos;
}

void GCodeMove::cmd_M114(GCodeCommand &gcmd)
{
    std::vector<double> cur_pos = get_gcode_position();
    // gcmd.respond_raw();
    // out put cur_pos
}

double GCodeMove::get_gcode_speed()
{
    return m_speed / m_speed_factor;
}

double GCodeMove::get_gcode_speed_override()
{
    return m_speed_factor * 60.;
}

void GCodeMove::cmd_M220(GCodeCommand &gcmd)
{
    double value = gcmd.get_double("S", 100., DBL_MIN, DBL_MAX, 0.) / (60. * 100.);
    m_speed = get_gcode_speed() * value;
    m_speed_factor = value;
}

void GCodeMove::cmd_M221(GCodeCommand &gcmd)
{
    double new_extrude_factor = gcmd.get_double("S", 100., DBL_MIN, DBL_MAX, 0.) / 100.;
    double last_e_pos = m_last_position[AXIS_E];
    double e_value = (last_e_pos - m_base_position[AXIS_E]) / m_extrude_factor;
    m_base_position[AXIS_E] = last_e_pos - e_value * new_extrude_factor;
    m_extrude_factor = new_extrude_factor;
}

std::string cmd_SET_GCODE_OFFSET_help = "Set a virtual offset to g-code positions";
void GCodeMove::cmd_SET_GCODE_OFFSET(GCodeCommand &gcmd)
{
    double move_delta[4] = {0., 0., 0., 0.};
    std::vector<std::string> axiss = {"X", "Y", "Z", "E"};
    for (int i = 0; i < axiss.size(); i++)
    {
        double offset = gcmd.get_double(axiss[i], DBL_MIN);
        if (offset == DBL_MIN)
        {
            offset = gcmd.get_double(axiss[i] + "_ADJUST", DBL_MIN);
            if (offset == DBL_MIN)
            {
                continue;
            }
            offset += m_homing_position[i];
        }
        double delta = offset - m_homing_position[i];
        move_delta[i] = delta; // 如果参数带了MOVE=1，就马上应用该坐标移动。
        m_base_position[i] += delta;
        m_homing_position[i] = offset;
    }
    // printf("move_delta: %f, %f, %f, %f\n", move_delta[0], move_delta[1], move_delta[2], move_delta[3]);
    // printf("m_base_position: %f, %f, %f, %f\n", m_base_position[0], m_base_position[1], m_base_position[2], m_base_position[3]);
    // printf("m_homing_position: %f, %f, %f, %f\n", m_homing_position[0], m_homing_position[1], m_homing_position[2], m_homing_position[3]);
    if (gcmd.get_int("MOVE", 0)) // 如果参数带了MOVE=1，就马上应用该坐标移动。
    {
        double speed = gcmd.get_double("MOVE_SPEED", m_speed, DBL_MIN, DBL_MAX, 0.);
        for (int i = 0; i < axiss.size(); i++)
        {
            m_last_position[i] += move_delta[i];
        }
        std::vector<double> position = {m_last_position[AXIS_X], m_last_position[AXIS_Y], m_last_position[AXIS_Z], m_last_position[AXIS_E]};
        m_move_transform(position, speed);
    }
}

gcode_move_state_t GCodeMove::get_gcode_state(std::string name)
{
    auto it = saved_states.find(name);
    if (it == saved_states.end())
    {
        // can not find state
        return gcode_move_state_t();
    }
    else
    {
        gcode_move_state_t state = it->second;
        return state;
    }
}

std::string cmd_SAVE_GCODE_STATE_help = "Save G-Code coordinate state";
void GCodeMove::cmd_SAVE_GCODE_STATE(GCodeCommand &gcmd)
{
    std::string state_name = gcmd.get_string("STATE", "default");
    gcode_move_state_t cur_state = {
        .absolute_coord = m_absolute_coord,
        .absolute_extrude = m_absolute_extrude,
        .speed = m_speed,
        .speed_factor = m_speed_factor,
        .extrude_factor = m_extrude_factor,
        .last_position = m_last_position,
        .homing_position = m_homing_position,
        .base_position = m_base_position};
    saved_states[state_name] = cur_state;
}

std::string cmd_RESTORE_GCODE_STATE_help = "Restore a previously saved G-Code state";
void GCodeMove::cmd_RESTORE_GCODE_STATE(GCodeCommand &gcmd)
{
    std::string state_name = gcmd.get_string("STATE", "default");
    auto it = saved_states.find(state_name);
    if (it == saved_states.end())
    {
        // can not find state
        return;
    }
    // Restore state
    gcode_move_state_t state = it->second;
    m_absolute_coord = state.absolute_coord;
    m_absolute_extrude = state.absolute_extrude;
    m_base_position = state.base_position;
    m_homing_position = state.homing_position;
    m_speed = state.speed;
    m_speed_factor = state.speed_factor;
    m_extrude_factor = state.extrude_factor;
    // Restore the relative E position
    double e_diff = m_last_position[AXIS_E] - state.last_position[AXIS_E];
    m_base_position[AXIS_E] += e_diff;
    // Move the toolhead back if requested
    if (gcmd.get_int("MOVE", 0))
    {
        double speed = gcmd.get_double("MOVE_SPEED", m_speed, DBL_MIN, DBL_MAX, 0.);
        m_last_position[AXIS_X] = state.last_position[AXIS_X];
        m_last_position[AXIS_Y] = state.last_position[AXIS_Y];
        std::vector<double> position = {m_last_position[AXIS_X], m_last_position[AXIS_Y], m_last_position[AXIS_Z], m_last_position[AXIS_E]};
        m_move_transform(position, speed);
        m_last_position[AXIS_Z] = state.last_position[AXIS_Z];
        std::vector<double> position_z = {m_last_position[AXIS_X], m_last_position[AXIS_Y], m_last_position[AXIS_Z], m_last_position[AXIS_E]};
        m_move_transform(position_z, speed);

    }
}

void GCodeMove::cmd_GET_POSITION(GCodeCommand &gcmd)
{
}
