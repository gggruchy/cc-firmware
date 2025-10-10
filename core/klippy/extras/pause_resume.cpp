#include "pause_resume.h"
#include "klippy.h"
#include "debug.h"

#define LOG_TAG "pause_resume"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

static std::string cmd_PAUSE_help = ("Pauses the current print");
static std::string cmd_RESUME_help = ("Resumes the print from a pause");
static std::string cmd_CLEAR_PAUSE_help = ("Clears the current paused state without resuming the print");
static std::string cmd_CANCEL_PRINT_help = ("Cancel the current print");

PauseResume::PauseResume(std::string section_name)
{
    m_recover_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "recover_velocity", 50.);
    m_v_sd = nullptr;
    m_is_paused = false;
    m_is_busying = false;
    m_sd_paused = false;
    m_pause_command_sent = false;
    m_wait = false;
    Printer::GetInstance()->register_event_handler("klippy:connect:PauseResume", std::bind(&PauseResume::handle_connect, this));
    Printer::GetInstance()->m_gcode->register_command("M76", std::bind(&PauseResume::cmd_PAUSE, this, std::placeholders::_1), false, cmd_PAUSE_help);
    Printer::GetInstance()->m_gcode->register_command("M601", std::bind(&PauseResume::cmd_PAUSE, this, std::placeholders::_1), false, cmd_PAUSE_help);
    Printer::GetInstance()->m_gcode->register_command("PAUSE", std::bind(&PauseResume::cmd_PAUSE, this, std::placeholders::_1), false, cmd_PAUSE_help);
    Printer::GetInstance()->m_gcode->register_command("M75", std::bind(&PauseResume::cmd_RESUME, this, std::placeholders::_1), false, cmd_RESUME_help);
    Printer::GetInstance()->m_gcode->register_command("M602", std::bind(&PauseResume::cmd_RESUME, this, std::placeholders::_1), false, cmd_RESUME_help);
    Printer::GetInstance()->m_gcode->register_command("RESUME", std::bind(&PauseResume::cmd_RESUME, this, std::placeholders::_1), false, cmd_RESUME_help);
    Printer::GetInstance()->m_gcode->register_command("CLEAR_PAUSE", std::bind(&PauseResume::cmd_CLEAR_PAUSE, this, std::placeholders::_1), false, cmd_CLEAR_PAUSE_help);
    Printer::GetInstance()->m_gcode->register_command("M77", std::bind(&PauseResume::cmd_CANCEL_PRINT, this, std::placeholders::_1), false, cmd_CANCEL_PRINT_help);
    Printer::GetInstance()->m_gcode->register_command("M603", std::bind(&PauseResume::cmd_CANCEL_PRINT, this, std::placeholders::_1), false, cmd_CANCEL_PRINT_help);
    Printer::GetInstance()->m_gcode->register_command("CANCEL_PRINT", std::bind(&PauseResume::cmd_CANCEL_PRINT, this, std::placeholders::_1), false, cmd_CANCEL_PRINT_help);
    Printer::GetInstance()->m_gcode->register_command("STOP_RESUME", std::bind(&PauseResume::cmd_STOP_RESUME, this, std::placeholders::_1), false, cmd_RESUME_help);
    Printer::GetInstance()->m_gcode->register_command("STOP_PRINT", std::bind(&PauseResume::cmd_STOP_PRINT, this, std::placeholders::_1), false, cmd_CANCEL_PRINT_help);
    Printer::GetInstance()->m_gcode->register_command("M600", std::bind(&PauseResume::cmd_M600, this, std::placeholders::_1), false, cmd_PAUSE_help);
    // Printer::GetInstance()->m_webhooks->register_endpoint("pause_resume/cancel", std::bind(&PauseResume::handle_cancel_request, this, std::placeholders::_1));
    // Printer::GetInstance()->m_webhooks->register_endpoint("pause_resume/pause", std::bind(&PauseResume::handle_pause_request, this, std::placeholders::_1));
    // Printer::GetInstance()->m_webhooks->register_endpoint("pause_resume/resume", std::bind(&PauseResume::handle_resume_request, this, std::placeholders::_1));

    m_save_extruder_temp = 0.;
    m_save_hotbed_temp = 0.;
    m_save_model_fan_speed = 0.;
    m_save_model_helper_fan_speed = 0.;

    m_pause_abs_pos_x = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_abs_pos_x", 0.);
    m_pause_abs_pos_y = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_abs_pos_y", 0.);
    m_pause_rel_pos_z = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_rel_pos_z", 10.);
    m_pause_move_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_move_speed", 3000.);
    m_pause_move_z_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_move_z_speed", 600.);
    m_pause_extruder_timeout = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_extruder_timeout", 0.);
    m_pause_extruder_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_extruder_temp", 0.);
    m_pause_fan_timeout = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_fan_timeout", 0.);
    m_pause_fan_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pause_fan_speed", 0.);
    m_resume_extrude_fan_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "resume_extrude_fan_speed", 0.);
    m_last_extruder_temp = 0.;
    m_last_fan_speed = 0.;
}

PauseResume::~PauseResume()
{
}

void PauseResume::handle_connect()
{
    Printer::GetInstance()->load_object("virtual_sdcard");
    m_v_sd = Printer::GetInstance()->m_virtual_sdcard;
}

// void PauseResume::handle_cancel_request(WebRequest* web_request)
// {
//     std::string cmd = "CANCEL_PRINT";
//     Printer::GetInstance()->m_gcode->run_script(cmd);
// }

// void PauseResume::handle_pause_request(WebRequest* web_request)
// {
//     std::string cmd = "PAUSE";
//     Printer::GetInstance()->m_gcode->run_script(cmd);
// }

// void PauseResume::handle_resume_request(WebRequest* web_request)
// {
//     std::string cmd = "RESUME";
//     Printer::GetInstance()->m_gcode->run_script(cmd);
// }

PauseResume_status_t PauseResume::get_status()
{
    PauseResume_status_t status{
        .is_paused = m_is_paused,
        .is_busying = m_is_busying};
    return status;
}

bool PauseResume::is_sd_active()
{
    return (m_v_sd != nullptr && m_v_sd->is_active());
}

void PauseResume::clear_pause_timer()
{
    if (m_pause_timer != nullptr)
    {
        Printer::GetInstance()->m_reactor->unregister_timer(m_pause_timer);
        m_last_extruder_temp = 0.;
        m_last_fan_speed = 0.;
        m_last_pause = 0.;
    }
}

void PauseResume::send_pause_command()
{
    // This sends the appropriate pause command from an event.  Note
    // the difference between pause_command_sent and is_paused, the
    // module isn't officially paused until the PAUSE gcode executes.
    if (!m_pause_command_sent)
    {
        if (is_sd_active())
        {
            // Printing from virtual sd, run pause command
            m_sd_paused = true;
            m_v_sd->do_pause();
        }
        else
        {
            m_sd_paused = false;
            Printer::GetInstance()->m_gcode->respond_info("action:paused");
        }
        m_pause_command_sent = true;
    }
}

double PauseResume::pause_callback(double eventtime)
{
    double temp = 0;
    double speed = 0;
    if (m_last_pause > m_pause_extruder_timeout && !m_last_extruder_temp)
    {
        m_last_extruder_temp = 1;
        Printer::GetInstance()->m_gcode_io->single_command("M104 S%.2f", std::min(m_save_extruder_temp, m_pause_extruder_temp));
    }
    if (m_last_pause > m_pause_fan_timeout && !m_last_fan_speed)
    {
        m_last_fan_speed = 1;
        Printer::GetInstance()->m_gcode_io->single_command("M106 S%d", int(m_pause_fan_speed + 0.5));
    }
    m_last_pause += 1;

    return eventtime + 1.;
}

void PauseResume::cmd_PAUSE(GCodeCommand &gcmd)
{
    if (m_is_paused)
    {
        gcmd.m_respond_info("Print already paused", true);
        return;
    }
    int need_extrude = gcmd.get_int("EXTRUDE", 0);
    gcode_move_state_t gcode_status;
    gcode_status = Printer::GetInstance()->m_gcode_move->get_status(get_monotonic());
    bool absolute_extrude = gcode_status.absolute_extrude;
    m_is_busying = true;
    Printer::GetInstance()->m_print_stats->note_pauseing();
    send_pause_command();
    m_save_extruder_temp = Printer::GetInstance()->m_printer_extruder->m_heater->m_target_temp;
    m_save_hotbed_temp = Printer::GetInstance()->m_bed_heater->m_heater->m_target_temp;
    m_save_model_fan_speed = Printer::GetInstance()->m_printer_fans[MODEL_FAN]->m_fan->m_current_speed;
    m_save_model_helper_fan_speed = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->m_fan->m_current_speed;
    // Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->m_fan->set_speed_from_command(0);
    std::vector<std::string> script;
    stringstream ss;

    script.push_back("SAVE_GCODE_STATE STATE=PAUSE_STATE");
    script.push_back("M83");
    script.push_back("G1 E-2 F1800");
    if (absolute_extrude)
        script.push_back("M82");
    
    script.push_back("M211 S0");
    std::map<std::string, std::string> kin_status = Printer::GetInstance()->m_tool_head->get_status(get_monotonic());
    if (kin_status["homed_axes"].find("x") == std::string::npos || kin_status["homed_axes"].find("y") == std::string::npos || kin_status["homed_axes"].find("z") == std::string::npos)
    {
        script.push_back("G28");
    }
    script.push_back("G91");
    ss << "G1 Z" << m_pause_rel_pos_z << " F600";
    script.push_back(ss.str());
    ss.str("");
    script.push_back("G90");
    ss << "G1 X" << m_pause_abs_pos_x << " F" << m_pause_move_speed;
    script.push_back(ss.str());
    ss.str("");
    ss << "G1 Y" << m_pause_abs_pos_y << " F" << m_pause_move_speed;
    script.push_back(ss.str());
    ss.str("");
    if (need_extrude)
    {
        script.push_back("M83");
        for (uint8_t i = 0; i < 7; i++)
        {
            script.push_back("G1 E13 F523");
            script.push_back("G1 E2 F150");
            script.push_back("M400");
        }
        script.push_back("G1 E-5 F1800");
        for (uint8_t i = 0; i < 8; i++)
        {
            script.push_back("G1 E13 F523");
            script.push_back("G1 E2 F150");
            script.push_back("M400");
        }
        if (absolute_extrude)
            script.push_back("M82");
        script.push_back("M729");
    }
    script.push_back("M211 S1");
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_tool_head->wait_moves();
    m_wait = false;
    m_is_paused = true;
    m_last_extruder_temp = 0;
    m_last_fan_speed = 0;
    m_is_busying = false;
    m_pause_timer = Printer::GetInstance()->m_reactor->register_timer("pause_timer", std::bind(&PauseResume::pause_callback, this, std::placeholders::_1), get_monotonic() + PIN_MIN_TIME);
    Printer::GetInstance()->m_print_stats->note_pause();
}

void PauseResume::send_resume_command()
{
    if (m_sd_paused)
    {
        // Printing from virtual sd, run pause command
        m_v_sd->do_resume();
        m_sd_paused = false;
    }
    else
    {
        Printer::GetInstance()->m_gcode->respond_info("action:resumed");
    }
    m_pause_command_sent = false;
}

void PauseResume::cmd_RESUME(GCodeCommand &gcmd)
{
    if (!m_is_paused)
    {
        gcmd.m_respond_info("Print is not paused, resume aborted", true);
        return;
    }
    if (m_pause_timer != nullptr)
    {
        Printer::GetInstance()->m_reactor->unregister_timer(m_pause_timer);
        m_last_extruder_temp = 0.;
        m_last_fan_speed = 0.;
        m_last_pause = 0.;
    }
    gcode_move_state_t gcode_status;
    gcode_status = Printer::GetInstance()->m_gcode_move->get_status(get_monotonic());
    bool absolute_extrude = gcode_status.absolute_extrude;
    m_is_busying = true;
    Printer::GetInstance()->m_print_stats->note_resuming();
    m_v_sd->resume_active();
    double velocity = gcmd.get_double("VELOCITY", m_recover_velocity);
    std::vector<std::string> script;
    stringstream ss;
    std::string cmd = "RESTORE_GCODE_STATE STATE=PAUSE_STATE MOVE=1 MOVE_SPEED=" + to_string(m_recover_velocity);
    ss << "M190 S" << m_save_hotbed_temp;
    script.push_back(ss.str());
    ss.str("");
    ss << "M109 S" << m_save_extruder_temp;
    script.push_back(ss.str());
    ss.str("");
    script.push_back("M211 S0");
    script.push_back("M106 S255");
    script.push_back("M83");
    for (uint8_t i = 0; i < 8; i++)
    {
        script.push_back("G1 E13 F523");
        script.push_back("G1 E2 F150");
        script.push_back("M400");
    }
    script.push_back("G4 S4");
    script.push_back("M204 S15000");
    script.push_back("M729");
    script.push_back("M106 S0");
    if (absolute_extrude)
        script.push_back("M82");

    script.push_back("M106 S" + to_string(m_resume_extrude_fan_speed));
    script.push_back("M211 S1");
    script.push_back(cmd);
    Printer::GetInstance()->m_gcode->run_script(script);
    m_save_extruder_temp = 0.;
    Printer::GetInstance()->m_printer_fans[MODEL_FAN]->m_fan->set_speed_from_command(m_save_model_fan_speed);
    Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->m_fan->set_speed_from_command(m_save_model_helper_fan_speed);
    Printer::GetInstance()->m_tool_head->wait_moves();
    send_resume_command();
    m_wait = false;
    m_is_paused = false;
    m_is_busying = false;
    std::cout << "-------------------------cmd_RESUME end--------------------------" << std::endl;
}

double PauseResume::get_save_extruder_temp(void)
{
    return m_save_extruder_temp;
}

void PauseResume::cmd_CLEAR_PAUSE(GCodeCommand &gcmd)
{
    m_is_paused = m_pause_command_sent = false;
    clear_pause_timer();
}

void PauseResume::cmd_CANCEL_PRINT(GCodeCommand &gcmd)
{
    // int cancel_print_save = 0;
    if (is_sd_active() || m_sd_paused)
    {
        // if (!m_sd_paused)
        // {
        //     cancel_print_save = 1;
        //     std::string cmd = "BREAK_SAVE_STATUS save_reason=cmd_CANCEL_PRINT_start";
        //     Printer::GetInstance()->m_gcode->run_script(cmd);
        // }
        m_v_sd->do_cancel();
    }
    else
    {
        gcmd.m_respond_info("action:cancel", true);
    }
    m_is_busying = true;
    Printer::GetInstance()->m_print_stats->note_canceling();
    cmd_CLEAR_PAUSE(gcmd);
    std::vector<std::string> script;
    stringstream ss;
    script.push_back("G91");
    script.push_back("G0 Z10 F600");
    script.push_back("G90");
    script.push_back("G28 X Y");
    ss << "G1 X" << m_pause_abs_pos_x << " F" << m_pause_move_speed;
    script.push_back(ss.str());
    ss.str("");
    ss << "G1 Y" << m_pause_abs_pos_y << " F" << m_pause_move_speed;
    script.push_back(ss.str());
    ss.str("");
    script.push_back("M104 S0");
    script.push_back("M140 S0");
    script.push_back("M107");
    script.push_back("M18");
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_print_stats->note_cancel();
    Printer::GetInstance()->m_tool_head->wait_moves();
    // if (cancel_print_save)
    // {
    //     std::string cmd = "BREAK_SAVE_STATUS save_reason=cmd_CANCEL_PRINT_end";
    //     Printer::GetInstance()->m_gcode->run_script(cmd);
    // }
    // else
    // {
    //     std::string cmd = "BREAK_SAVE_STATUS save_reason=cmd_PAUSE_end";
    //     Printer::GetInstance()->m_gcode->run_script(cmd);
    // }
    // if (Printer::GetInstance()->m_break_save != nullptr)
    // {
    //     Printer::GetInstance()->m_break_save->delete_save_files();
    // }
    m_wait = false;
    m_is_busying = false;
}

void PauseResume::cmd_STOP_RESUME(GCodeCommand &gcmd) // 断电后恢复
{
    std::cout << "STOP_RESUME" << std::endl;
    // 取消应用床网数据
    ZMesh *mesh = Printer::GetInstance()->m_bed_mesh->get_mesh();
    Printer::GetInstance()->m_bed_mesh->set_mesh(nullptr);
    double velocity = gcmd.get_double("VELOCITY", m_recover_velocity);
    std::vector<std::string> script;
    std::string cmd;
    // 归0
    // script.push_back("G28 X Y Z");           //Z轴向上归0
    // double gcode_z1 = Printer::GetInstance()->m_gcode_move->get_status(0.).base_position[2];
    Printer::GetInstance()->m_gcode_io->single_command("G90");
    std::vector<double> position = Printer::GetInstance()->m_tool_head->get_position();
    std::string path = "/board-resource/break_save0.gcode";
    ConfigParser break_pconfig(path, false);
    std::vector<string> keys;
    keys.push_back("is_z_homing");
    if (!Printer::GetInstance()->m_break_save->s_saved_print_para.is_z_homing)
    {
        Printer::GetInstance()->m_gcode_io->single_command("G1 Z%f F360", position[2] + 10);
        break_pconfig.SetBool("z_homing_stats", "is_z_homing", true);
        break_pconfig.WriteI_specified_Ini(path, "z_homing_stats", keys);
    }
    else
    {
        position[2] += 10;
        Printer::GetInstance()->m_tool_head->set_position(position);
    }
    Printer::GetInstance()->m_gcode_io->single_command("G28 X Y");
    Printer::GetInstance()->m_tool_head->wait_moves();
    // script.push_back("G28 X Y");
    std::cout << "G28 X Y" << std::endl;
    script.push_back("G1 X" + to_string(m_pause_abs_pos_x) + " F4500");
    script.push_back("G1 Y" + to_string(m_pause_abs_pos_y) + " F4500");
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_tool_head->wait_moves();
    std::vector<std::string>().swap(script);
    script.push_back("M400");
    script.push_back("M106 S255");
    script.push_back("M83");
    for (uint8_t i = 0; i < 8; i++)
    {
        script.push_back("G1 E13 F523");
        script.push_back("G1 E2 F150");
        script.push_back("M400");
    }
    script.push_back("G4 S4");
    script.push_back("M204 S15000");
    script.push_back("M729");
    script.push_back("M106 S0");
    script.push_back("M82");

    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_tool_head->wait_moves();
    script = std::vector<std::string>();

    // double gcode_z2 = Printer::GetInstance()->m_gcode_move->get_status(0.).base_position[2];
    // std::cout << "gcode_z1  "<< gcode_z1 << "gcode_z2  "<< gcode_z2<< std::endl;
    // double m_z_hop = gcode_z2 - gcode_z1;

    // Printer::GetInstance()->m_gcode->run_script(Printer::GetInstance()->m_break_save->m_gcode_cmds_before);
    script.push_back(cmd);
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_tool_head->wait_moves();
    script = std::vector<std::string>();
    // 运动到断电保存位置
    // cmd = "G0 Z" + to_string(gcmd.get_double("Z", 0)) + "  F12000"; // 先抬升Z 避免撞模型
    // script.push_back(cmd);
    // cmd = "G0 X" + to_string(gcmd.get_double("X", 0)) + " Y" + to_string(gcmd.get_double("Y", 0)) + " Z" + to_string(gcmd.get_double("Z", 0)) + "  F12000";
    cmd = "G0 X" + to_string(gcmd.get_double("X", 0)) + " Y" + to_string(gcmd.get_double("Y", 0)) + "  F12000";
    script.push_back(cmd);
    if(Printer::GetInstance()->m_safe_z_homing != nullptr)
    {
        // cmd = "G91";
        // script.push_back(cmd);
        // cmd = "G0 Z" + to_string(-Printer::GetInstance()->m_safe_z_homing->m_z_hop) + "  F360";
        // script.push_back(cmd);
        cmd = "G90";
        script.push_back(cmd);
        cmd = "G0 Z" + to_string(gcmd.get_double("save_z_stop", 0)) + " F360";
        script.push_back(cmd);
        printf("cmd_SDCARD_PRINT_FILE_from_break %s\n", cmd.c_str());
        // Printer::GetInstance()->m_gcode_io->single_command("G1 Z" + to_string(-Printer::GetInstance()->m_safe_z_homing->m_z_hop) + " F" + to_string(Printer::GetInstance()->m_safe_z_homing->m_z_hop_speed));
    }   
    else
    {
        // Printer::GetInstance()->m_gcode_io->single_command("G91");
        cmd = "G91";
        script.push_back(cmd);
        cmd = "G1 Z-10 F360";
        script.push_back(cmd);
        // Printer::GetInstance()->m_gcode_io->single_command("G1 Z-10 F600");
        printf("m_safe_z_homing no res\n");
    }
    break_pconfig.SetBool("z_homing_stats", "is_z_homing", false);
    break_pconfig.WriteI_specified_Ini(path, "z_homing_stats", keys);
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_tool_head->wait_moves();
    script = std::vector<std::string>();
    // Printer::GetInstance()->m_gcode->run_script(Printer::GetInstance()->m_break_save->m_gcode_cmds_after);

    // 设置料位置
    cmd = "G92 E" + to_string(gcmd.get_double("E", 0));
    // std::cout << "cmd_SDCARD_PRINT_FILE_from_break  " << cmd << std::endl;
    script.push_back(cmd);
    // 设置速度
    cmd = "G1 F" + to_string(gcmd.get_double("F", 0));
    // std::cout << "cmd_SDCARD_PRINT_FILE_from_break  " << cmd << std::endl;
    script.push_back(cmd);

    Printer::GetInstance()->m_gcode->run_script(script);

    std::vector<double> curpos = Printer::GetInstance()->m_tool_head->get_position();
    double save_z_stop = gcmd.get_double("save_z_stop", 0);
    std::vector<double> pos = {curpos[0], curpos[1], save_z_stop, curpos[3]};
    std::vector<int> axes = {0, 1, 2};
    Printer::GetInstance()->m_tool_head->set_position(pos, axes);
    // 应用床网数据
    Printer::GetInstance()->m_bed_mesh->set_mesh(mesh);
    double z = gcmd.get_double("Z", 0);
    Printer::GetInstance()->m_gcode_io->single_command("G90");
    Printer::GetInstance()->m_gcode_io->single_command("G0 X" + to_string(curpos[0]) + " Y" + to_string(curpos[1]) + " Z" + to_string(z) + " F" + to_string(gcmd.get_double("F", 0)));
    Printer::GetInstance()->m_tool_head->wait_moves();

    send_resume_command();
    m_wait = false;
    m_is_paused = false;
    m_is_busying = false;
}

void PauseResume::cmd_STOP_PRINT(GCodeCommand &gcmd)
{
    if (is_sd_active() || m_sd_paused)
    {
        m_v_sd->do_cancel();
    }
    else
    {
        gcmd.m_respond_info("action:cancel", true);
    }
    cmd_CLEAR_PAUSE(gcmd);
    std::vector<std::string> script;
    script.push_back("M104 S0");
    script.push_back("M140 S0");
    script.push_back("M107");
    script.push_back("M18");
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_print_stats->note_cancel();
    Printer::GetInstance()->m_tool_head->wait_moves();
}

void PauseResume::cmd_M600(GCodeCommand &gcmd)
{
    LOG_I("pause because of M600\n");
    Printer::GetInstance()->m_print_stats->note_change_filament_start();
    std::vector<std::string> script;
    script.push_back("PAUSE");
    script.push_back("CUT_OFF_FILAMENT");
    script.push_back("EXTRUDE_FILAMENT E=-60 F=240 FAN_ON=0 REPORT=0");
    Printer::GetInstance()->m_gcode->run_script(script);
    Printer::GetInstance()->m_tool_head->wait_moves();
    Printer::GetInstance()->m_print_stats->note_change_filament_completed();
}