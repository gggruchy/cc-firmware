#include "dirzctl.h"
#include "klippy.h"

DirZCtl::DirZCtl(std::string section_name)
{
    m_oid = Printer::GetInstance()->m_mcu->create_oid();
    Printer::GetInstance()->m_mcu->register_config_callback(std::bind(&DirZCtl::_build_config, this, std::placeholders::_1));
    // Printer::GetInstance()->m_mcu->register_response(std::bind(&DirZCtl::_handle_debug_dirzctl, this), "debug_dirzctl", m_oid);
    Printer::GetInstance()->m_mcu->m_serial->register_response(std::bind(&DirZCtl::_handle_result_dirzctl, this, std::placeholders::_1), "result_dirzctl", m_oid);
    Printer::GetInstance()->register_event_handler("klippy:mcu_identify:" + section_name, std::bind(&DirZCtl::_handle_mcu_identify, this));
    Printer::GetInstance()->register_event_handler("klippy:shutdown:" + section_name, std::bind(&DirZCtl::_handle_shutdown, this));
    Printer::GetInstance()->register_event_handler("klippy:disconnect:" + section_name, std::bind(&DirZCtl::_handle_disconnect, this));

    std::string cmd_DIRZCTL_help = "Test DIRZCTL.";
    Printer::GetInstance()->m_gcode->register_command("DIRZCTL", std::bind(&DirZCtl::cmd_DIRZCTL, this, std::placeholders::_1), false, cmd_DIRZCTL_help);

    m_mcu = Printer::GetInstance()->m_mcu;
    m_mcu_freq = 84000000;
    m_step_base = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "step_base", 2, 1, 6);
    m_last_send_heart = 0;
    m_is_shutdown = true;
    m_is_timeout = true;
    m_cmd_queue = Printer::GetInstance()->m_mcu->alloc_command_queue();
}

DirZCtl::~DirZCtl()
{
}

void DirZCtl::_handle_mcu_identify()
{
    std::vector<std::vector<MCU_stepper *>> steppers = Printer::GetInstance()->m_tool_head->m_kin->get_steppers();
    for (int i = 0; i < steppers.size(); i++)
    {
        for (int j = 0; j < steppers[i].size(); j++)
        {
            if (steppers[i][j]->is_active_axis('z'))
            {
                m_steppers.push_back(steppers[i][j]);
            }
        }
    }
    m_mcu_freq = Printer::GetInstance()->m_mcu->m_mcu_freq;
    m_is_shutdown = false;
    m_is_timeout = false;
}

void DirZCtl::_build_config(int para)
{
    if (para & 1)
    {
        std::stringstream config_dirzctl;
        config_dirzctl << "config_dirzctl oid=" << m_oid << " z_count=" << (int)m_steppers.size();
        std::cout << "**************config_dirzctl:" << config_dirzctl.str() << std::endl;
        Printer::GetInstance()->m_mcu->add_config_cmd(config_dirzctl.str());
        for (int i = 0; i < m_steppers.size(); i++)
        {
            pin_info info = m_steppers[i]->get_pin_info();
            std::stringstream add_dirzctl;
            add_dirzctl << "add_dirzctl oid=" << m_oid << " index=" << i << " dir_pin=" << info.dir_pin << " step_pin=" << info.step_pin << " dir_invert=" << info.invert_dir << " step_invert=" << info.invert_step;
            Printer::GetInstance()->m_mcu->add_config_cmd(add_dirzctl.str());
        }
    }
}

bool DirZCtl::_handle_shutdown()
{
    m_is_shutdown = true;
}

bool DirZCtl::_handle_disconnect()
{
    m_is_timeout = true;
}

void DirZCtl::_handle_result_dirzctl(ParseResult params)
{
    m_mutex.lock();
    m_params.push_back(params);
    m_mutex.unlock();
}

std::vector<ParseResult> DirZCtl::get_params()
{
    m_mutex.lock();
    std::vector<ParseResult> tmp = m_params;
    m_mutex.unlock();
    return tmp;
}

void DirZCtl::check_and_run(int direct, uint32_t step_us, uint32_t step_cnt, bool wait_finish, bool is_ck_con)
{
    std::cout << "DirZCtl::check_and_run" << std::endl;
    if (m_is_shutdown || m_is_timeout)
    {
        std::cout << "DirZCtl::check_and_run m_is_shutdown||m_is_timeout." << std::endl;
        return;
    }
    if (step_cnt != 0)
    {
        m_mutex.lock();
        std::vector<ParseResult>().swap(m_params);
        m_mutex.unlock();
    }
    if (!Printer::GetInstance()->m_stepper_enable->get_stepper_state(m_steppers.at(0)->get_name()))
    {
        Printer::GetInstance()->m_stepper_enable->motor_debug_enable(m_steppers.at(0)->get_name(), true);
    }
    std::stringstream run_cmd;
    run_cmd << "run_dirzctl oid=" << m_oid << " direct=" << direct << " step_us=" << step_us << " step_cnt=" << step_cnt;
    std::cout << run_cmd.str() << std::endl;
    Printer::GetInstance()->m_mcu->m_serial->send(run_cmd.str(), 0, 0, m_cmd_queue);
    double t_start = get_monotonic();
    m_mutex.lock();
    int zctl_size = m_params.size();
    m_mutex.unlock();
    while (!(m_is_shutdown || m_is_timeout) && wait_finish && (get_monotonic() - t_start < (1.5 / 1000 / 1000 * step_us * step_cnt) && zctl_size != 2))
    {
        // Printer::GetInstance()->m_reactor->pause(get_monotonic() + 0.05);
        m_mutex.lock();
        zctl_size = m_params.size();
        m_mutex.unlock();
        sleep(0.05);
    }
}

void DirZCtl::cmd_DIRZCTL(GCodeCommand &gcmd)
{
    int index = gcmd.get_int("INDEX", m_steppers.size(), 0, m_steppers.size());
    int direct = gcmd.get_int("DIRECT", 1, 0, 1);
    int step_us = gcmd.get_int("STEP_US", 1500, 4, 1000 * 100);
    int step_cnt = gcmd.get_int("STEP_CNT", 256, 0, 10000);

    check_and_run(direct, step_us, step_cnt, false, false);
}
