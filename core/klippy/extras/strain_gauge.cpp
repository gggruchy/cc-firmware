#include "strain_gauge.h"
#include "klippy.h"
#include "probe.h"
#include "alg.h"
#include "simplebus.h"
#include "srv_state.h"
#define LOG_TAG "strain_gauge"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
#define HOMING_Z_CONTROL_FAN 0

StrainGaugeCFG::StrainGaugeCFG(std::string section_name)
{
    m_base_count = Printer::GetInstance()->m_pconfig->GetInt(section_name, "base_count", 40, 10, 100);
    m_enable_z_home = Printer::GetInstance()->m_pconfig->GetBool(section_name, "enable_z_home", true);
    m_pi_count = Printer::GetInstance()->m_pconfig->GetInt(section_name, "pi_count", 32, 16, 128);
    m_g28_min_hold = Printer::GetInstance()->m_pconfig->GetInt(section_name, "g28_min_hold", 3000, 100, 50000);
    m_g28_max_hold = Printer::GetInstance()->m_pconfig->GetInt(section_name, "g28_max_hold", 50000, 100, 100000);
    m_g29_min_hold = Printer::GetInstance()->m_pconfig->GetInt(section_name, "g29_min_hold", 3000, 100, 50000);
    m_g29_max_hold = Printer::GetInstance()->m_pconfig->GetInt(section_name, "g29_max_hold", 50000, 100, 100000);
    m_hot_min_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "s_hot_min_temp", 140, 80, 200);
    m_hot_max_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "s_hot_max_temp", 200, 180, 300);
    m_bed_max_temp = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "s_bed_max_temp", 60, 45, 100);
    m_pa_fil_len_mm = Printer::GetInstance()->m_pconfig->GetInt(section_name, "pa_fil_len_mm", 2, 2, 100);
    m_pa_fil_dis_mm = Printer::GetInstance()->m_pconfig->GetInt(section_name, "pa_fil_dis_mm", 30, 2, 100);
    m_pa_clr_dis_mm = Printer::GetInstance()->m_pconfig->GetInt(section_name, "pa_clr_dis_mm", 20, 2, 100);
    m_pa_clr_down_mm = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "pa_clr_down_mm", -0.1, -1, 1);
    m_clr_noz_start_x = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "clr_noz_start_x", 0, 0, 1000);
    m_clr_noz_start_y = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "clr_noz_start_y", 0, 0, 1000);
    m_clr_noz_len_x = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "clr_noz_len_x", 0, m_pa_clr_dis_mm + 6, 1000);
    m_clr_noz_len_y = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "clr_noz_len_y", 0, 0, 1000);
    m_bed_max_err = Printer::GetInstance()->m_pconfig->GetInt(section_name, "bed_max_err", 2, 2, 10);
    m_g28_max_err = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "g28_max_err", 0.1, 0, 1);
    m_g28_max_try = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "g28_max_try", 4, 0);
    m_g29_max_detection = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "g29_max_detection", -5, -50, 0);
    std::cout << "m_bed_max_err:" << m_bed_max_err << std::endl;
    m_max_z = Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "position_max", 300, 100, 500);
    m_g29_xy_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "g29_xy_speed", 150, 10, 1000);
    m_platform_material = Printer::GetInstance()->m_pconfig->GetString(section_name, "platform_material", "standard");
    if (m_platform_material == "standard")
    {
        m_fix_z_offset = -Printer::GetInstance()->m_unmodifiable_cfg->GetDouble(section_name, "standard_fix_z_offset", 0.0, -3, 3);
    }
    else if (m_platform_material == "enhancement")
    {
        m_fix_z_offset = -Printer::GetInstance()->m_unmodifiable_cfg->GetDouble(section_name, "enhancement_fix_z_offset", 0.0, -3, 3);
    }

    std::cout << "m_fix_z_offset:" << m_fix_z_offset << std::endl;  
    m_mesh_max = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "mesh_max", 236, -1000, 1000);
    m_max_dis_bef_g28 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_dis_bef_g28", 10, 0, 50);
    m_dead_zone_bef_g28 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "dead_zone_bef_g28", m_max_dis_bef_g28 / 2, 0, 50);
    m_g28_sta0_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "g28_sta0_speed", 2.0, 0.1, 10);
    m_g28_sta1_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "g28_sta1_speed", 2.5, 0.1, 10);
    m_g29_rdy_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "g29_rdy_speed", 2.5, 0.1, 10);
    m_g29_speed = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "g29_speed", 2.0, 0.1, 10);
    m_show_msg = Printer::GetInstance()->m_pconfig->GetBool(section_name, "show_msg", false);
    m_best_above_z = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "best_above_z", 1.5, 0.5, 10);
    m_g28_wait_cool_down = Printer::GetInstance()->m_pconfig->GetBool(section_name, "g28_wait_cool_down", false);
    std::cout << "m_g28_wait_cool_down:" << (int32_t)m_g28_wait_cool_down << std::endl;
    m_shake_cnt = Printer::GetInstance()->m_pconfig->GetInt(section_name, "shake_cnt", 4, 1, 512);
    m_shake_range = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "shake_range", 0.5, 0.1, 2);
    m_shake_max_velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "shake_max_velocity", 100, 1, 5000);
    m_shake_max_accel = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "shake_max_accel", 1000, 1, 50000);
    m_g28_sta0_min_hold = Printer::GetInstance()->m_pconfig->GetInt(section_name, "g28_sta0_min_hold", m_g28_min_hold * 2, 100, 100000);
    m_need_measure_gap = Printer::GetInstance()->m_pconfig->GetBool(section_name, "need_measure_gap", true);
    std::cout << "m_need_measure_gap:" << (int32_t)m_need_measure_gap << std::endl;
    m_gap_dis_range = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "gap_dis_range", 0.6, 0.2, 2);
    m_z_gap_0 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_gap_00", 0, -1, 1);
    m_z_gap_1 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_gap_01", 0, -1, 1);
    m_z_gap_2 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_gap_10", 0, -1, 1);
    // m_z_gap_11 = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "z_gap_11", 0, -1, 1);
    m_check_bed_mesh_max_err = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "check_bed_mesh_max_err", 0.2, 0.01, 1);
    m_tri_wave_ip = Printer::GetInstance()->m_pconfig->GetString(section_name, "tri_wave_ip", "");
    m_self_z_offset = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "self_z_offset", 0.0, -2, 2);
    m_enable_ts_compense = Printer::GetInstance()->m_pconfig->GetBool(section_name, "enable_ts_compense", true); // 使能时间戳补偿
    std::cout << "m_enable_ts_compense: " << (int32_t)m_enable_ts_compense << std::endl;
    m_use_probe_by_step = Printer::GetInstance()->m_pconfig->GetBool(section_name, "use_probe_by_step", false); // G28|G29 z 使用 probe_by_step 命令
    std::cout << "m_use_probe_by_step: " << (int32_t)m_use_probe_by_step << std::endl;

    // self.stored_profs = config.get_prefix_sections('prtouch')
    // self.stored_profs = self.stored_profs[1] if (len(self.stored_profs) == 2 and self.need_measure_gap) else None
}

StrainGaugeCFG::~StrainGaugeCFG()
{
}

StrainGaugeVAL::StrainGaugeVAL(std::string section_name)
{
    m_out_index = 0;
    m_out_val_mm = 0.;
    std::vector<std::vector<double>>(3, std::vector<double>(3, 0)).swap(m_rdy_pos);
    std::vector<std::vector<double>>(3, std::vector<double>(3, 0)).swap(m_gap_pos);
    m_g29_cnt = int(0);
    m_re_probe_cnt = 0;
    // m_home_xy;
    m_jump_probe_ready = false;
}

StrainGaugeVAL::~StrainGaugeVAL()
{
}

StrainGaugeOBJ::StrainGaugeOBJ(std::string section_name)
{
    // m_bed_mesh = Printer::GetInstance()->m_bed_mesh;
    // m_filter = Printer::GetInstance()->m_filter;
    // m_hx711s = Printer::GetInstance()->m_hx711s;
    // m_dirzctl = Printer::GetInstance()->m_dirzctl;
}

StrainGaugeOBJ::~StrainGaugeOBJ()
{
}

void StrainGaugeOBJ::find_objs()
{
    m_tool_head = Printer::GetInstance()->m_tool_head;
    m_hx711s = Printer::GetInstance()->m_hx711s;
    m_pheaters = Printer::GetInstance()->m_pheaters;
    m_printer_extruder = Printer::GetInstance()->m_printer_extruder;
    m_bed_heater = Printer::GetInstance()->m_bed_heater;
    m_bed_mesh = Printer::GetInstance()->m_bed_mesh;
    m_dirzctl = Printer::GetInstance()->m_dirzctl;
    m_mcu = m_hx711s->m_mcu;
    m_filter = Printer::GetInstance()->m_filter;
    m_kin = Printer::GetInstance()->m_tool_head->get_kinematics();
}

StrainGaugeWrapper::StrainGaugeWrapper(std::string section_name)
{
    m_cfg = new StrainGaugeCFG(section_name);
    m_val = new StrainGaugeVAL(section_name);
    m_obj = new StrainGaugeOBJ(section_name);
    // m_AlgSg = new Alg();

    Printer::GetInstance()->register_event_handler("klippy:mcu_identify:" + section_name, std::bind(&StrainGaugeWrapper::_handle_mcu_identify, this));

    Printer::GetInstance()->m_gcode->register_command("STRAINGAUGE_Z_HOME", std::bind(&StrainGaugeWrapper::cmd_STRAINGAUGE_Z_HOME, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("STRAINGAUGE_TEST", std::bind(&StrainGaugeWrapper::cmd_STRAINGAUGE_TEST, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("PRTOUCH_READY", std::bind(&StrainGaugeWrapper::cmd_PRTOUCH_READY, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("NOZZLE_CLEAR", std::bind(&StrainGaugeWrapper::cmd_NOZZLE_CLEAR, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("CHECK_BED_MESH", std::bind(&StrainGaugeWrapper::cmd_CHECK_BED_MESH, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("MEASURE_GAP_TEST", std::bind(&StrainGaugeWrapper::cmd_MEASURE_GAP_TEST, this, std::placeholders::_1));

    m_step_base = 1;
    // usb_uart_init();
}

StrainGaugeWrapper::~StrainGaugeWrapper()
{
}

void StrainGaugeWrapper::_handle_mcu_identify()
{
    m_obj->find_objs();
    double min_x, min_y, max_x, max_y;
    min_x = m_obj->m_bed_mesh->m_bmc->m_mesh_min[0];
    min_y = m_obj->m_bed_mesh->m_bmc->m_mesh_min[1];
    max_x = m_obj->m_bed_mesh->m_bmc->m_mesh_max[0];
    max_y = m_obj->m_bed_mesh->m_bmc->m_mesh_max[1];

    m_val->m_rdy_pos[0][0] = min_x;
    m_val->m_rdy_pos[0][1] = min_y;
    m_val->m_rdy_pos[0][2] = m_cfg->m_bed_max_err + 1.;
    m_val->m_rdy_pos[1][0] = min_x;
    m_val->m_rdy_pos[1][1] = max_y;
    m_val->m_rdy_pos[1][2] = m_cfg->m_bed_max_err + 1.;
    m_val->m_rdy_pos[2][0] = max_x;
    m_val->m_rdy_pos[2][1] = max_y;
    m_val->m_rdy_pos[2][2] = m_cfg->m_bed_max_err + 1.;
    // m_val->m_rdy_pos[3][0] = max_x;
    // m_val->m_rdy_pos[3][1] = min_y;
    // m_val->m_rdy_pos[3][2] = m_cfg->m_bed_max_err + 1.;

    m_val->m_gap_pos[0][0] = max_x / 2.;
    m_val->m_gap_pos[0][1] = max_y - 30.;
    m_val->m_gap_pos[0][2] = 0.1; // m_cfg->m_z_gap_0;
    m_val->m_gap_pos[1][0] = max_x - 1.;
    m_val->m_gap_pos[1][1] = min_y - 1.;
    m_val->m_gap_pos[1][2] = 0.1; // m_cfg->m_z_gap_1;
    m_val->m_gap_pos[2][0] = min_x + 1.;
    m_val->m_gap_pos[2][1] = min_y + 1.;
    m_val->m_gap_pos[2][2] = 0.1; // m_cfg->m_z_gap_2;
    // m_val->m_gap_pos[3][0] = max_x - 1.;
    // m_val->m_gap_pos[3][1] = min_y + 1.;
    // m_val->m_gap_pos[3][2] = m_cfg->m_z_gap_10;

    if (m_cfg->m_clr_noz_start_x <= 0 || m_cfg->m_clr_noz_start_y <= 0 || m_cfg->m_clr_noz_len_x <= 0 || m_cfg->m_clr_noz_len_y <= 0)
    {
        m_cfg->m_clr_noz_start_x = (max_x - min_x) * 1 / 3 + min_x;
        m_cfg->m_clr_noz_start_y = max_y - 6;
        m_cfg->m_clr_noz_len_x = (max_x - min_x) * 1 / 3;
        m_cfg->m_clr_noz_len_y = 5;
    }

    m_val->m_home_xy.push_back((max_x - min_x) / 2 + min_x);
    m_val->m_home_xy.push_back((max_y - min_y) / 2 + min_y);
}

bool StrainGaugeWrapper::ck_sys_sta()
{
    return (!m_obj->m_hx711s->m_is_shutdown && !m_obj->m_hx711s->m_is_timeout && !m_obj->m_dirzctl->m_is_shutdown && !m_obj->m_dirzctl->m_is_timeout);
}

void StrainGaugeWrapper::_ck_g28ed()
{
    for (int i = 0; i < 3; i++)
    {
        if (Printer::GetInstance()->m_tool_head->m_kin->m_limits[i][0] > Printer::GetInstance()->m_tool_head->m_kin->m_limits[i][1])
        {
            Printer::GetInstance()->m_gcode_io->single_command("G28");
            break;
        }
    }
}

void StrainGaugeWrapper::_move(std::vector<double> pos, double speed, bool wait)
{
    if (1)
    {
        if (pos.size() >= 3)
        {
            Printer::GetInstance()->m_tool_head->manual_move(pos, speed);
        }
        else
        {
            Printer::GetInstance()->m_tool_head->manual_move(pos, speed);
        }
    }
    if (wait)
    {
        Printer::GetInstance()->m_tool_head->wait_moves();
    }
}

bool StrainGaugeWrapper::_check_index(int index)
{
    if (index <= m_cfg->m_pi_count - 3 && index >= m_cfg->m_pi_count * 2 / 3)
    {
        return true;
    }
    else
    {
        return false;
    }
}

std::vector<double> StrainGaugeWrapper::_get_linear2(std::vector<double> &p1, std::vector<double> &p2, std::vector<double> &po, bool is_base_x)
{
    // p1[0]: start_time    p1[1]: 0   p1[2]: start_z
    // p2[0]: end_time      p1[1]: 0   p1[2]: end_z
    // po[0]: sg_trigger_time      p0[1]: 0   p0[2]: 0
    if ((std::fabs(p1[0] - p2[0]) < 0.001 && is_base_x) || (std::fabs(p1[1] - p2[1]) < 0.001 && !is_base_x))
    {
        return po;
    }
    double a = (p2[2] - p1[2]) / (is_base_x ? (p2[0] - p1[0]) : (p2[1] - p1[1]));
    double b = p1[2] - ((is_base_x ? p1[0] : p1[1]) * a);
    //
    po[2] = a * (is_base_x ? po[0] : po[1]) + b;
    return po;
}

bool StrainGaugeWrapper::_check_trigger(int arg_index, std::vector<double> fit_vals, double min_hold, double max_hold)
{
    static int32_t loop = 0;
    loop++;
    std::vector<double> fit_vals_t(fit_vals.begin(), fit_vals.end());
    // 未用到
    m_val->m_out_index = m_cfg->m_pi_count - 1;
    // 最后三个都大于最大值
    if (fit_vals.size() >= (m_cfg->m_pi_count / 2) && std::fabs(fit_vals.back()) >= max_hold &&
        std::fabs(fit_vals[fit_vals.size() - 2]) >= max_hold && std::fabs(fit_vals[fit_vals.size() - 3]) >= max_hold)
    {
        std::cout << "...............................both > max_hold: " << fit_vals.back() << " " << fit_vals[fit_vals.size() - 2] << " " << fit_vals[fit_vals.size() - 3] << std::endl;
        return true;
    }
    // 数组采样够
    if (fit_vals.size() != m_cfg->m_pi_count)
    {
        std::cout << "fit_vals.size() != m_cfg->m_pi_count fit_vals.size():" << fit_vals.size() << " m_cfg->m_pi_count:" << m_cfg->m_pi_count << std::endl;
        return false;
    }

    // 中途一个值大于最大阈值，前后都小于最大阈值一半，过滤
    for (int i = 0; i < m_cfg->m_pi_count - 1; i++)
    {
        if (fit_vals_t[i] >= max_hold && fit_vals_t[i - 1] < (max_hold / 2) && fit_vals_t[i + 1] < (max_hold / 2))
        {
            fit_vals_t[i] = fit_vals_t[i - 1];
        }
    }

    // 找触发点
    std::vector<double> vals_p = fit_vals_t;
    double max_val = *std::max_element(vals_p.begin(), vals_p.end());
    double min_val = *std::min_element(vals_p.begin(), vals_p.end());
    max_val += (max_val - min_val) == 0 ? 1 : 0;

    for (size_t i = 0; i < vals_p.size(); i++)
    {
        vals_p[i] = (vals_p[i] - min_val) / (max_val - min_val);
    }
    double angle = std::atan((vals_p.back() - vals_p[0]) / vals_p.size());
    double sin_angle = std::sin(-angle);
    double cos_angle = std::cos(-angle);

    for (size_t i = 0; i < vals_p.size(); i++)
    {
        vals_p[i] = (i - 0) * sin_angle + (vals_p[i] - 0) * cos_angle + 0;
    }
    // 找到触发点的下标
    size_t out_index = std::distance(vals_p.begin(), std::min_element(vals_p.begin(), vals_p.end()));
    if (out_index > 0)
    {
        // 触发值处理
        for (size_t i = out_index; i < m_cfg->m_pi_count; i++)
        {
            fit_vals_t[i] = fit_vals_t[i] * (m_obj->m_filter->lft_k1_oft / 2) + fit_vals_t[i - 1] * (1 - (m_obj->m_filter->lft_k1_oft / 2));
        }
    }
    // 无用
    vals_p = fit_vals_t;

    // 非递增 pass
    if (!(fit_vals_t.back() > fit_vals_t[fit_vals_t.size() - 2] && fit_vals_t[fit_vals_t.size() - 2] > fit_vals_t[fit_vals_t.size() - 3]))
    {
        if (loop % 1000 == 0)
        {
            std::cout << "not Incremental " << fit_vals_t.back() << " " << fit_vals_t[fit_vals_t.size() - 2] << " " << fit_vals_t[fit_vals_t.size() - 3] << std::endl;
        }
        return false;
    }

    // 处理最后三个值：都要大于前面的最大值
    double max_val_2 = *std::max_element(fit_vals_t.begin(), fit_vals_t.end() - 3);
    if (!((fit_vals_t.back() > max_val_2) && (fit_vals_t[fit_vals_t.size() - 2] > max_val_2) && (fit_vals_t[fit_vals_t.size() - 3] > max_val_2)))
    {
        if (loop % 1000 == 0)
        {
            std::cout << "last three not Incremental: " << fit_vals_t.back() << " " << fit_vals_t[fit_vals_t.size() - 2] << " " << fit_vals_t[fit_vals_t.size() - 3] << std::endl;
        }
        return false;
    }

    max_val = *std::max_element(fit_vals_t.begin(), fit_vals_t.end());
    min_val = *std::min_element(fit_vals_t.begin(), fit_vals_t.end());
    // 归一化
    for (size_t i = 0; i < m_cfg->m_pi_count; i++)
    {
        fit_vals_t[i] = (fit_vals_t[i] - min_val) / (max_val - min_val);
    }

    for (size_t i = 0; i < m_cfg->m_pi_count - 1; i++)
    {
        //
        if ((fit_vals_t.back() - fit_vals_t[i]) / ((m_cfg->m_pi_count - i) * 1. / m_cfg->m_pi_count) < 0.8)
        {
            if (loop % 1000 == 0)
            {
                std::cout << "not ???" << fit_vals_t.back() << std::endl;
            }
            return false;
        }
    }
    //
    if (fit_vals.back() < min_hold || fit_vals[fit_vals.size() - 2] < (min_hold / 2) || fit_vals[fit_vals.size() - 3] < (min_hold / 3))
    {
        if (loop % 1000 == 0)
        {
            std::cout << "< min_hold: " << fit_vals.back() << std::endl;
        }
        return false;
    }
    std::cout << ".....................trigger........................" << std::endl;
    return true;
}

bool StrainGaugeWrapper::_check_trigger(int arg_index, std::vector<double> fit_vals, const std::vector<double> &fusion_values, double min_hold, double max_hold)
{
    static int32_t loop = 0;
    loop++;
    std::vector<double> fit_vals_t(fit_vals.begin(), fit_vals.end());
    // 未用到
    // m_val->m_out_index = m_cfg->m_pi_count - 1;

    if (Printer::GetInstance()->m_auto_leveling->enable_fusion_trigger)
    {
        if (std::fabs(fusion_values.back()) >= max_hold && std::fabs(fusion_values[fusion_values.size() - 2]) >= max_hold && std::fabs(fusion_values[fusion_values.size() - 3]) >= max_hold && std::fabs(fusion_values[fusion_values.size() - 4]) >= max_hold && std::fabs(fusion_values[fusion_values.size() - 5]) >= max_hold)
        {
            std::cout << "...............................three sensor both last five > max_hold: " << fusion_values.back() << " " << fusion_values[fusion_values.size() - 2] << " " << fusion_values[fusion_values.size() - 3] << " max_hold:" << max_hold << std::endl;
            return true;
        }
        if ((std::fabs(fusion_values.back()) >= std::fabs(fusion_values[fusion_values.size() - 2])) && (std::fabs(fusion_values[fusion_values.size() - 2]) >= std::fabs(fusion_values[fusion_values.size() - 3]) && std::fabs(fusion_values[fusion_values.size() - 3]) >= std::fabs(fusion_values[fusion_values.size() - 4]) && (std::fabs(fusion_values[fusion_values.size() - 4]) >= min_hold)))
        {
            std::cout << "...............................three sensor last four Incremental and morethan min_hold: " << fusion_values.back() << " " << fusion_values[fusion_values.size() - 2] << " " << fusion_values[fusion_values.size() - 3] << " min_hold:" << min_hold << std::endl;
            return true;
        }
    }

    // 最后三个都大于最大值
    if (fit_vals.size() >= (m_cfg->m_pi_count / 2) && std::fabs(fit_vals.back()) >= max_hold &&
        std::fabs(fit_vals[fit_vals.size() - 2]) >= max_hold && std::fabs(fit_vals[fit_vals.size() - 3]) >= max_hold)
    {
        std::cout << "...............................one sensor both > max_hold: " << fit_vals.back() << " " << fit_vals[fit_vals.size() - 2] << " " << fit_vals[fit_vals.size() - 3] << std::endl;
        return true;
    }
    // // 数组采样够
    // if (fit_vals.size() != m_cfg->m_pi_count)
    // {
    //     if (loop%100==0){
    //         std::cout << "fit_vals.size() != m_cfg->m_pi_count fit_vals.size():"<< fit_vals.size() << " m_cfg->m_pi_count:" << m_cfg->m_pi_count << std::endl;
    //     }
    //     return false;
    // }

    // 中途一个值大于最大阈值，前后都小于最大阈值一半，过滤
    for (int i = 0; i < fit_vals.size() - 1; i++)
    {
        if (fit_vals_t[i] >= max_hold && fit_vals_t[i - 1] < (max_hold / 2) && fit_vals_t[i + 1] < (max_hold / 2))
        {
            fit_vals_t[i] = fit_vals_t[i - 1];
        }
    }

    // 找触发点
    std::vector<double> vals_p = fit_vals_t;
    double max_val = *std::max_element(vals_p.begin(), vals_p.end());
    double min_val = *std::min_element(vals_p.begin(), vals_p.end());
    max_val += (max_val - min_val) == 0 ? 1 : 0;

    for (size_t i = 0; i < vals_p.size(); i++)
    {
        vals_p[i] = (vals_p[i] - min_val) / (max_val - min_val);
    }
    double angle = std::atan((vals_p.back() - vals_p[0]) / vals_p.size());
    double sin_angle = std::sin(-angle);
    double cos_angle = std::cos(-angle);

    for (size_t i = 0; i < vals_p.size(); i++)
    {
        vals_p[i] = (i - 0) * sin_angle + (vals_p[i] - 0) * cos_angle + 0;
    }
    // 找到触发点的下标
    size_t out_index = std::distance(vals_p.begin(), std::min_element(vals_p.begin(), vals_p.end()));
    if (out_index > 0)
    {
        // 触发值处理
        for (size_t i = out_index; i < fit_vals.size(); i++)
        {
            fit_vals_t[i] = fit_vals_t[i] * (m_obj->m_filter->lft_k1_oft / 2) + fit_vals_t[i - 1] * (1 - (m_obj->m_filter->lft_k1_oft / 2));
        }
    }
    // 无用
    vals_p = fit_vals_t;

    // 非递增 pass
    if (!(fit_vals_t.back() > fit_vals_t[fit_vals_t.size() - 2] && fit_vals_t[fit_vals_t.size() - 2] > fit_vals_t[fit_vals_t.size() - 3]))
    {
        if (loop % 1000 == 0)
        {
            std::cout << "not Incremental " << fit_vals_t.back() << " " << fit_vals_t[fit_vals_t.size() - 2] << " " << fit_vals_t[fit_vals_t.size() - 3] << std::endl;
        }
        return false;
    }

    // 处理最后三个值：都要大于前面的最大值
    double max_val_2 = *std::max_element(fit_vals_t.begin(), fit_vals_t.end() - 3);
    if (!((fit_vals_t.back() > max_val_2) && (fit_vals_t[fit_vals_t.size() - 2] > max_val_2) && (fit_vals_t[fit_vals_t.size() - 3] > max_val_2)))
    {
        if (loop % 1000 == 0)
        {
            std::cout << "last three not Incremental: " << fit_vals_t.back() << " " << fit_vals_t[fit_vals_t.size() - 2] << " " << fit_vals_t[fit_vals_t.size() - 3] << std::endl;
        }
        return false;
    }

    max_val = *std::max_element(fit_vals_t.begin(), fit_vals_t.end());
    min_val = *std::min_element(fit_vals_t.begin(), fit_vals_t.end());
    // 归一化
    for (size_t i = 0; i < fit_vals.size(); i++)
    {
        fit_vals_t[i] = (fit_vals_t[i] - min_val) / (max_val - min_val);
    }

    for (size_t i = 0; i < fit_vals.size() - 1; i++)
    {
        //
        if ((fit_vals_t.back() - fit_vals_t[i]) / ((fit_vals.size() - i) * 1. / fit_vals.size()) < 0.8)
        {
            if (loop % 1000 == 0)
            {
                std::cout << "not ???" << fit_vals_t.back() << std::endl;
            }
            return false;
        }
    }
    //
    if (fit_vals.back() < min_hold || fit_vals[fit_vals.size() - 2] < (min_hold / 2) || fit_vals[fit_vals.size() - 3] < (min_hold / 3))
    {
        if (loop % 1000 == 0)
        {
            std::cout << "< min_hold: " << fit_vals.back() << std::endl;
        }
        return false;
    }
    std::cout << ".....................trigger........................" << std::endl;
    return true;
}

void StrainGaugeWrapper::_set_hot_temps(double temp, double fan_spd, bool wait, int err)
{
    Printer::GetInstance()->m_gcode_io->single_command("M106 S%.2f", fan_spd);
    if (wait)
    {
        Printer::GetInstance()->m_gcode_io->single_command("M109 S%.2f", temp);
        // while ck_sys_sta() and abs(self.obj.heater_hot.target_temp - self.obj.heater_hot.smoothed_temp) > err and self.obj.heater_hot.target_temp > 0:
        //         self.obj.hx711s.delay_s(0.100)
    }
    else
    {
        Printer::GetInstance()->m_gcode_io->single_command("M104 S%.2f", temp);
    }
}

void StrainGaugeWrapper::_set_bed_temps(double temp, bool wait, int err)
{
    Printer::GetInstance()->m_gcode_io->single_command("M190 S%.2f", temp);
    // if (wait)
    // {
    // while self.ck_sys_sta() and abs(self.obj.heater_bed.target_temp - self.obj.heater_bed.smoothed_temp) > err and self.obj.heater_bed.target_temp > 0:
    //         self.obj.hx711s.delay_s(0.100)
    // }
}

double StrainGaugeWrapper::_probe_times(int max_times, std::vector<double> rdy_pos, double speed_mm, double min_dis_mm, double max_z_err, double min_hold, double max_hold)
{
    double o_mm = 0;
    double rdy_pos_z = rdy_pos[2];
    std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();
    now_pos[2] = rdy_pos_z;
    _move(now_pos, m_cfg->m_g29_rdy_speed);
    _move(rdy_pos, m_cfg->m_g29_xy_speed);
    for (int i = 0; i < max_times; i++)
    {
        double o_mm0, o_mm1;
        bool deal_sta;
        int o_index0, o_index1;
        std::tie(o_index0, o_mm0, deal_sta) = probe_by_step({rdy_pos[0], rdy_pos[1], rdy_pos_z}, speed_mm, min_dis_mm, min_hold, max_hold, true);
        if (!deal_sta && rdy_pos_z == rdy_pos[2])
        {
            rdy_pos_z += 2;
            continue;
        }
        std::tie(o_index1, o_mm1, deal_sta) = probe_by_step({rdy_pos[0], rdy_pos[1], rdy_pos_z}, speed_mm, min_dis_mm, min_hold, max_hold, true);
        o_mm = (o_mm0 + o_mm1) / 2;
        if (std::fabs(o_mm0 - o_mm1) <= max_z_err || !ck_sys_sta())
        {
            break;
        }
        m_val->m_re_probe_cnt += 1;
        // pntMsg("***_probe_times must be reprobe= o_mm0=%.2f, o_mm1=%.2f", o_mm0, o_mm1); // Assuming pntMsg function exists
    }
    return o_mm;
}

double StrainGaugeWrapper::get_best_rdy_z(double rdy_x, double rdy_y, std::vector<std::vector<double>> base_pos)
{
    if (base_pos.size() != 0)
    {
        base_pos = m_val->m_rdy_pos;
    }
    std::vector<double> p_left = {base_pos[0][0], rdy_y, 0};
    std::vector<double> p_right = {base_pos[0][2], rdy_y, 0};
    std::vector<double> p_mid = {rdy_x, rdy_y, 0};

    p_left = _get_linear2(base_pos[0], base_pos[1], p_left, false);
    p_right = _get_linear2(base_pos[2], base_pos[3], p_right, false);
    p_mid = _get_linear2(p_left, p_right, p_mid, true);

    // printf("Get best rdy z: Src=%s, x=%.2f, y=%.2f, cal_z=%.2f\n", ((base_pos == m_val->m_rdy_pos ? "RDY" : "GAP"), rdy_x, rdy_y, p_mid[2]));
    return p_mid[2] < m_cfg->m_bed_max_err ? p_mid[2] : m_cfg->m_bed_max_err;
}

void StrainGaugeWrapper::shake_motor(int cnt)
{
    LOG_I("shake motor: %d\n", cnt);
    double max_velocity = Printer::GetInstance()->m_tool_head->m_max_velocity;
    double max_accel = Printer::GetInstance()->m_tool_head->m_max_accel;
    double max_z_velocity = Printer::GetInstance()->m_tool_head->m_kin->m_max_z_velocity;
    double max_z_accel = Printer::GetInstance()->m_tool_head->m_kin->m_max_z_accel;

    // Printer::GetInstance()->m_tool_head->m_max_velocity = m_cfg->m_shake_max_velocity;
    // Printer::GetInstance()->m_tool_head->m_max_accel = m_cfg->m_shake_max_accel;
    // Printer::GetInstance()->m_tool_head->m_kin->m_max_z_velocity = m_cfg->m_shake_max_velocity;
    // Printer::GetInstance()->m_tool_head->m_kin->m_max_z_accel = m_cfg->m_shake_max_accel;

    std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();
    double now_z = now_pos[2];
    std::vector<double> shake_pos_up = {now_pos[0], now_pos[1], now_pos[2] + m_cfg->m_shake_range};
    std::vector<double> shake_pos_down = {now_pos[0], now_pos[1], now_pos[2]};
    for (int i = 0; i < cnt; i++)
    {
        Printer::GetInstance()->m_tool_head->manual_move(shake_pos_up, max_velocity);
        Printer::GetInstance()->m_tool_head->manual_move(shake_pos_down, max_velocity);
        // Printer::GetInstance()->m_gcode_io->single_command("G1 X%.2f Y%.2f Z%.2f F600", now_pos[0], now_pos[1], now_pos[2] - m_cfg->m_shake_range / 2);
        // Printer::GetInstance()->m_gcode_io->single_command("G1 X%.2f Y%.2f Z%.2f F600", now_pos[0], now_pos[1], now_pos[2] + m_cfg->m_shake_range / 2);
        // Printer::GetInstance()->m_gcode_io->single_command("G1 X%.2f Y%.2f Z%.2f F600", now_pos[0], now_pos[1], now_pos[2] - m_cfg->m_shake_range);
        // Printer::GetInstance()->m_gcode_io->single_command("G1 X%.2f Y%.2f Z%.2f F600", now_pos[0], now_pos[1], now_pos[2] + m_cfg->m_shake_range);
        // while (Printer::GetInstance()->m_tool_head->m_movequeue->moveq.size() >= 4 && ck_sys_sta())
        // {
        //     // self.obj.hx711s.delay_s(0.010)
        //     // Printer::GetInstance()->m_reactor->pause(get_monotonic() + 0.010);
        // }
    }
    Printer::GetInstance()->m_tool_head->wait_moves();
    now_pos[2] = now_z;
    _move(now_pos, m_cfg->m_g29_xy_speed);
    Printer::GetInstance()->m_tool_head->wait_moves();

    // Printer::GetInstance()->m_tool_head->m_max_velocity = max_velocity;
    // Printer::GetInstance()->m_tool_head->m_max_accel = max_accel;
    // Printer::GetInstance()->m_tool_head->m_kin->m_max_z_velocity = max_z_velocity;
    // Printer::GetInstance()->m_tool_head->m_kin->m_max_z_accel = max_z_accel;
}

double StrainGaugeWrapper::measure_gap(double zero_z)
{
    double min_dis_mm = m_cfg->m_gap_dis_range;
    double speed_mm = m_cfg->m_gap_dis_range;
    std::vector<double> p0_vals;
    std::vector<double> p1_vals;

    int rd_cnt = int(2 * 80 * (min_dis_mm / speed_mm));
    int step_cnt = int(min_dis_mm / (Printer::GetInstance()->m_tool_head->m_stepper[2].get_step_dist() * m_step_base));
    int step_us = int(((min_dis_mm / speed_mm) * 1000 * 1000) / step_cnt);

    std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();
    now_pos[2] = zero_z + min_dis_mm / 2;
    _move(now_pos, m_cfg->m_g29_rdy_speed);
    // m_obj->m_hx711s->read_base(int(m_cfg->m_base_count / 2), m_cfg->m_g28_max_hold); //????????
    m_obj->m_hx711s->CalibrationStart(m_cfg->m_base_count);

    m_obj->m_hx711s->query_start(rd_cnt, rd_cnt, false, false);
    m_obj->m_dirzctl->check_and_run(0, step_us, step_cnt, true);
    std::vector<std::vector<double>> p0_valss = m_obj->m_hx711s->get_vals();

    m_obj->m_hx711s->query_start(rd_cnt, rd_cnt, false, false);
    m_obj->m_dirzctl->check_and_run(1, step_us, step_cnt, true);
    std::vector<std::vector<double>> p1_valss = m_obj->m_hx711s->get_vals();

    m_obj->m_hx711s->query_start(rd_cnt, 0, false, false);
    if (p0_valss[0].size() == 0 || p1_valss[0].size() == 0)
    {
        // this->pnt_msg("measure_gap: Error! Cannot recv datas from hx711s!!!");
        return 0;
    }

    // self.pnt_msg('---------------------------------')
    // for i in range(int(self.obj.hx711s.s_count)):
    //     self.pnt_array('p0_%d_valss = ' % i, p0_valss[i], len(p0_valss[i]))
    // self.pnt_msg('---------------------------------')
    // for i in range(int(self.obj.hx711s.s_count)):
    //     self.pnt_array('p1_%d_valss = ' % i, p1_valss[i], len(p1_valss[i]))
    // self.pnt_msg('---------------------------------')

    std::vector<double> p0_diss;
    std::vector<double> p1_diss;
    std::vector<double> gaps;

    for (int gap_index = 0; gap_index < m_obj->m_hx711s->m_s_count; gap_index++)
    {
        std::vector<double> p0_vals = p0_valss[gap_index];
        std::vector<double> p1_vals = p1_valss[gap_index];

        if (p0_vals[0] > p0_vals.back())
        {
            for (auto &p0_val : p0_vals)
            {
                p0_val = p0_val * -1;
            }
        }

        if (p1_vals[0] < p1_vals.back())
        {
            for (auto &p1_val : p1_vals)
            {
                p1_val = p1_val * -1;
            }
        }

        double max_val = *std::max_element(p0_vals.begin(), p0_vals.end());
        double min_val = *std::min_element(p0_vals.begin(), p0_vals.end());

        for (int i = 0; i < p0_vals.size(); i++)
        {
            p0_vals[i] = (p0_vals[i] - min_val) / (max_val - min_val);
        }
        double angle = std::atan((p0_vals.back() - p0_vals[0]) / p0_vals.size());
        double sin_angle = std::sin(-angle);
        double cos_angle = std::cos(-angle);
        for (int i = 0; i < p0_vals.size(); i++)
        {
            p0_vals[i] = (i - 0) * sin_angle + (p0_vals[0] - 0) * cos_angle + 0;
        }
        int p0_out_index = std::distance(p0_vals.begin(), std::min_element(p0_vals.begin(), p0_vals.end()));
        double p0_dis = (p0_vals.size() - p0_out_index) * 0.012 * speed_mm;
        p0_diss.push_back(p0_dis);

        std::reverse(p1_vals.begin(), p1_vals.end());
        max_val = *std::max_element(p1_vals.begin(), p1_vals.end());
        min_val = *std::min_element(p1_vals.begin(), p1_vals.end());

        for (int i = 0; i < p1_vals.size(); i++)
        {
            p1_vals[i] = (p1_vals[i] - min_val) / (max_val - min_val);
        }

        angle = std::atan((p1_vals.back() - p1_vals[0]) / p1_vals.size());
        sin_angle = std::sin(-angle);
        cos_angle = std::cos(-angle);

        for (int i = 0; i < p1_vals.size(); i++)
        {
            p1_vals[i] = (i - 0) * sin_angle + (p1_vals[i] - 0) * cos_angle + 0;
        }

        int p1_out_index = std::distance(p1_vals.begin(), std::min_element(p1_vals.begin(), p1_vals.end()));
        double p1_dis = (p1_vals.size() - p1_out_index) * 0.012 * speed_mm;

        return p1_dis;

        p1_diss.push_back(p1_dis);

        gaps.push_back(p1_dis - p0_dis);
    }

    // 计算v_cnt和v_gap（参见原始Python代码）
    int v_cnt = 0;
    double v_gap = 0.0;
    for (int i = 0; i < p0_valss.size(); i++)
    {
        if ((0 < p0_diss[i] < min_dis_mm) && (0 < p1_diss[i] < min_dis_mm) && (0 <= gaps[i] < 0.2))
        {
            v_cnt++;
            v_gap += (gaps[i] <= 0.1 ? gaps[i] : 0.1);
        }
    }
    v_gap = (v_cnt == 0 ? 0 : (v_gap / v_cnt));

    // self.pnt_msg('measure_gap: v_cnt=%d v_gap = %.2f' % (v_cnt, v_gap))

    return v_gap;
}

double StrainGaugeWrapper::_gap_times(int max_times, double zero_pos)
{
    std::vector<double> gaps;
    std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();

    for (int i = 0; i < max_times; i++)
    {
        _move({now_pos[0], now_pos[1], zero_pos + 2, now_pos[3]}, m_cfg->m_g29_rdy_speed);
        shake_motor(static_cast<int>(m_cfg->m_shake_cnt / 2));
        gaps.push_back(measure_gap(zero_pos));
    }

    std::sort(gaps.begin(), gaps.end());

    // Output the gap measure values
    std::cout << "Gap measure vals = ";
    for (auto val : gaps)
    {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    now_pos = Printer::GetInstance()->m_tool_head->get_position();
    _move({now_pos[0], now_pos[1], m_cfg->m_bed_max_err + 1.0, now_pos[3]}, m_cfg->m_g29_rdy_speed);

    return gaps[static_cast<int>((max_times + 1) / 2)];
}

bool StrainGaugeWrapper::probe_ready()
{
    if (m_val->m_jump_probe_ready)
    {
        m_val->m_jump_probe_ready = false;
        return false;
    }
    _ck_g28ed();
    double min_x = m_obj->m_bed_mesh->m_bmc->m_mesh_min[0];
    double min_y = m_obj->m_bed_mesh->m_bmc->m_mesh_min[1];
    double max_x = m_obj->m_bed_mesh->m_bmc->m_mesh_max[0];
    double max_y = m_obj->m_bed_mesh->m_bmc->m_mesh_max[1];
    std::vector<std::vector<double>> rdy_pos{{min_x, min_y, m_cfg->m_bed_max_err + 1.},
                                             {min_x, max_y, m_cfg->m_bed_max_err + 1.},
                                             {max_x, max_y, m_cfg->m_bed_max_err + 1.},
                                             {max_x, min_y, m_cfg->m_bed_max_err + 1.}};
    ZMesh *mesh = m_obj->m_bed_mesh->get_mesh();
    m_obj->m_bed_mesh->set_mesh(nullptr);
    for (int i = 0; i < m_obj->m_hx711s->m_s_count; i++)
    {
        rdy_pos[i][2] = _probe_times(3, rdy_pos[i], m_cfg->m_g29_speed, 10, 0.2, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold);
        // if (self.cfg.need_measure_gap) {
        //     self.val.gap_pos[i] = rdy_pos[i];
        //     self.val.gap_pos[i][2] = _gap_times(3, self.val.gap_pos[i][2]);
        // } else {
        //     self.val.gap_pos[i][2] = 0;
        // }
    }
    if (m_cfg->m_need_measure_gap)
    {
        Printer::GetInstance()->m_pconfig->SetDouble("strain_gauge", "z_gap_00", m_val->m_gap_pos[0][2]);
        Printer::GetInstance()->m_pconfig->SetDouble("strain_gauge", "z_gap_01", m_val->m_gap_pos[1][2]);
        Printer::GetInstance()->m_pconfig->SetDouble("strain_gauge", "z_gap_11", m_val->m_gap_pos[2][2]);
        Printer::GetInstance()->m_pconfig->SetDouble("strain_gauge", "z_gap_10", m_val->m_gap_pos[3][2]);
    }
    std::cout << "RDY_POS = [00=" << rdy_pos[0][2] << ", 01=" << rdy_pos[1][2] << ", 11=" << rdy_pos[2][2] << ", 10=" << rdy_pos[3][2] << "]" << std::endl;
    std::cout << "GAP_POS = [00=" << m_val->m_gap_pos[0][2] << ", 01=" << m_val->m_gap_pos[1][2] << ", 11=" << m_val->m_gap_pos[2][2] << ", 10=" << m_val->m_gap_pos[3][2] << "]" << std::endl;
    m_obj->m_bed_mesh->set_mesh(mesh);
    return true;
}

void StrainGaugeWrapper::check_bed_mesh(bool auto_g29)
{
    double min_x = m_obj->m_bed_mesh->m_bmc->m_mesh_min[0];
    double min_y = m_obj->m_bed_mesh->m_bmc->m_mesh_min[1];
    double max_x = m_obj->m_bed_mesh->m_bmc->m_mesh_max[0];
    double max_y = m_obj->m_bed_mesh->m_bmc->m_mesh_max[1];
    std::random_device rd;
    std::mt19937 rng(rd());
    std::vector<std::vector<double>> rdy_pos{{min_x + std::uniform_real_distribution<double>(2.0, 5.0)(rng), min_y + std::uniform_real_distribution<double>(2.0, 5.0)(rng), m_cfg->m_bed_max_err + 1.},
                                             {min_x + std::uniform_real_distribution<double>(2.0, 5.0)(rng), max_y - std::uniform_real_distribution<double>(2.0, 5.0)(rng), m_cfg->m_bed_max_err + 1.},
                                             {max_x - std::uniform_real_distribution<double>(2.0, 5.0)(rng), max_y - std::uniform_real_distribution<double>(2.0, 5.0)(rng), m_cfg->m_bed_max_err + 1.},
                                             {max_x - std::uniform_real_distribution<double>(2.0, 5.0)(rng), min_y + std::uniform_real_distribution<double>(2.0, 5.0)(rng), m_cfg->m_bed_max_err + 1.}};
    int err_cnt = 0;
    m_val->m_jump_probe_ready = true;
    auto mesh = m_obj->m_bed_mesh->get_mesh();
    if (mesh == nullptr)
    {
        if (auto_g29)
        {
            // pnt_msg("The bed_mesh data is invalid and cannot be verified.");
            _ck_g28ed();
            Printer::GetInstance()->m_gcode_io->single_command("BED_MESH_CALIBRATE");
            Printer::GetInstance()->m_gcode_io->single_command("CXSAVE_CONFIG");
        }
        else
        {
            // throw std::runtime_error("The bed_mesh data is invalid and cannot be verified.");
            std::cout << "The bed_mesh data is invalid and cannot be verified." << std::endl;
            // throw std::runtime_error("{\"code\":\"key504\", \"msg\":\"The bed_mesh data is invalid and cannot be verified.\"}");
        }
        return;
    }
    m_obj->m_bed_mesh->set_mesh(nullptr);
    _ck_g28ed();
    _move({m_val->m_home_xy[0], m_val->m_home_xy[1], m_cfg->m_bed_max_err + 1.0, Printer::GetInstance()->m_tool_head->get_position()[3]}, m_cfg->m_g29_xy_speed);
    for (int i = 0; i < 4; i++)
    {   
        // 和 position_end_stop 一起生效
        // rdy_pos[i][2] = _probe_times(3, rdy_pos[i], m_cfg->m_g29_speed, 10, m_cfg->m_check_bed_mesh_max_err * 2, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold) + m_cfg->m_fix_z_offset;
        rdy_pos[i][2] = _probe_times(3, rdy_pos[i], m_cfg->m_g29_speed, 10, m_cfg->m_check_bed_mesh_max_err * 2, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold);
        rdy_pos[i][2] += (m_cfg->m_need_measure_gap ? get_best_rdy_z(rdy_pos[i][0], rdy_pos[i][1], m_val->m_gap_pos) : 0);
    }
    m_obj->m_bed_mesh->set_mesh(mesh);
    std::vector<double> errs;
    for (int i = 0; i < 4; i++)
    {
        double mesh_z = m_obj->m_bed_mesh->m_z_mesh->calc_z(m_val->m_rdy_pos[i][0], m_val->m_rdy_pos[i][1]);
        errs.push_back(fabs(rdy_pos[i][2] - mesh_z));
        if (rdy_pos[i][2] > m_cfg->m_bed_max_err)
        {
            err_cnt++;
        }
        // self.pnt_msg('P%d = [x=%.2f, y=%.2f, mest_z=%.2f, probe_z=%.2f, err_z=%.2f]' % (i, self.val.rdy_pos[i][0], self.val.rdy_pos[i][1], mesh_z, self.val.rdy_pos[i][2], errs[i]))
    }

    if (err_cnt > 2)
    {
        // pnt_msg("The bed_mesh data is invalid and cannot be verified.");
        // std::cout << "The bed_mesh data is invalid and cannot be verified." << std::endl;
        if (auto_g29)
        {
            // self.pnt_array("check_bed_mesh: Due to the great change of the hot bed, it needs to be re-leveled. errs = ", errs, len(errs))
            m_val->m_g29_cnt = 0;
            Printer::GetInstance()->m_gcode_io->single_command("BED_MESH_CALIBRATE");
            Printer::GetInstance()->m_gcode_io->single_command("CXSAVE_CONFIG");
        }
        else
        {
            // throw std::runtime_error("The bed_mesh data is invalid and cannot be verified.");
            std::cout << "The bed_mesh data is invalid and cannot be verified." << std::endl;
            // throw std::runtime_error("{\"code\":\"key504\", \"msg\":\"The bed_mesh data is invalid and cannot be verified.\"}");
        }
    }
    else
    {
        // self.pnt_array("check_bed_mesh: Errs = ", errs, len(errs))
    }
}

void StrainGaugeWrapper::raise_z_bef_g28()
{
    if (Printer::GetInstance()->m_tool_head->m_kin->m_limits[2][0] <= Printer::GetInstance()->m_tool_head->m_kin->m_limits[2][1] && Printer::GetInstance()->m_tool_head->get_position()[2] < 5)
    {
        std::vector<double> position = Printer::GetInstance()->m_tool_head->get_position();
        position[2] = 5;
        Printer::GetInstance()->m_tool_head->manual_move(position, 5);
        Printer::GetInstance()->m_tool_head->wait_moves();
    }
}

double StrainGaugeWrapper::cal_z(double start_z, double start_time, double end_z, double end_time)
{
    double sg_trigger_time = m_obj->m_hx711s->m_trigger_timestamp;
    std::cout << "sg trigger time: " << sg_trigger_time << " m_trigger_tick: " << m_obj->m_hx711s->m_trigger_tick << std::endl;
    std::cout << "start_time: " << start_time << std::endl;
    std::cout << "start_z: " << start_z << std::endl;
    std::cout << "end_time: " << end_time << " sg_trigger_time:" << sg_trigger_time << " delta_time:" << end_time - sg_trigger_time << std::endl;
    std::cout << "end_z: " << end_z << std::endl;

    std::vector<double> p1 = {start_time, 0, start_z};
    std::vector<double> p2 = {end_time, 0, end_z};
    std::vector<double> po = {sg_trigger_time, 0, 0};
    double result = _get_linear2(p1, p2, po, true)[2];
    m_val->m_out_val_mm = result;
    double delta = result - end_z;
    std::cout << "m_out_val_mm result:" << result << " end_z:" << end_z << " delta:" << result - end_z << " at:" << get_monotonic() << std::endl;
    // 只会超前，补偿值小于 0.1
    if (delta > 0.1 || delta < 0)
    {
        LOG_E("cal_z error, end_z:%f result:%f delta:%f\n\n", end_z, result, delta);
        return 0;
    }
    else
    {
        return delta;
    }
}

/**
 * @description: 计算 z 轴位置精确停止位置
 * @author:
 * @param {double} start_z
 * @param {vector<double>} &hx711_vals
 * @return {*}
 */
std::tuple<int, int, bool> StrainGaugeWrapper::cal_min_z(double start_z, const std::vector<double> &hx711_vals)
{
    m_obj->m_dirzctl->m_mcu_freq = Printer::GetInstance()->m_mcu->m_mcu_freq;
    // Get parameters
    std::vector<ParseResult> dirzctl_params = m_obj->m_dirzctl->get_params();
    // Check parameters
    while (dirzctl_params.size() == 0 || dirzctl_params.size() != 2)
    {
        m_obj->m_hx711s->delay_s(0.15);
        dirzctl_params = m_obj->m_dirzctl->get_params();
        std::cout << "probe_by_step: Can not recv stepper-z status. size : " << dirzctl_params.size() << std::endl;
        // throw std::runtime_error("probe_by_step: Can not recv stepper-z status.");
    }
    // 应变片未触发
    if (!(m_obj->m_hx711s->m_is_trigger > 0))
    {
        int up_all_cnt = dirzctl_params[0].PT_uint32_outs.at("step") - dirzctl_params[1].PT_uint32_outs.at("step") + 1;
        return std::make_tuple(up_all_cnt, up_all_cnt, false);
    }

    // get z-axis start|end time end position
    double dirzctl_start_time = m_obj->m_dirzctl->m_mcu->clock_to_print_time(m_obj->m_dirzctl->m_mcu->m_clocksync->clock32_to_clock64(dirzctl_params[0].PT_uint32_outs.at("tick")));
    double dirzctl_end_time = m_obj->m_dirzctl->m_mcu->clock_to_print_time(m_obj->m_dirzctl->m_mcu->m_clocksync->clock32_to_clock64(dirzctl_params[1].PT_uint32_outs.at("tick")));
    double end_z = start_z - (dirzctl_params[0].PT_uint32_outs.at("step") - dirzctl_params[1].PT_uint32_outs.at("step") + 1) * (m_obj->m_dirzctl->m_steppers[0]->get_step_dist() * m_obj->m_dirzctl->m_step_base);
    double trigger_time = m_obj->m_hx711s->m_trigger_timestamp;

    double dirzctl_estimated_time = m_obj->m_dirzctl->m_mcu->m_clocksync->estimated_print_time(get_monotonic());
    LOG_D("z轴当前预估时间:dirzctl_estimated_time:%f\n", dirzctl_estimated_time);

    double sg_sensor_estimated_time = Printer::GetInstance()->m_hx711s->m_mcu->m_clocksync->estimated_print_time(get_monotonic());
    LOG_D("应变片轴当前预估时间:sg_sensor_estimated_time:%f\n", sg_sensor_estimated_time);

    LOG_D("dirzctl_start_time:%f\n", dirzctl_start_time);
    LOG_D("dirzctl_end_time:%f\n", dirzctl_end_time);
    LOG_D("start z:%f\n", start_z);
    LOG_D("end z:%f\n", end_z);
    m_val->m_end_z_mm = end_z;
    LOG_D("trigger_time:%f trigger_tick:%d  delta:%f\n", trigger_time, m_obj->m_hx711s->m_trigger_tick, dirzctl_end_time-trigger_time);

    // Calculate out_val_mm
    std::vector<double> p1;
    p1.push_back(dirzctl_start_time);
    p1.push_back(0);
    p1.push_back(start_z);
    std::vector<double> p2;
    p2.push_back(dirzctl_end_time);
    p2.push_back(0);
    p2.push_back(end_z);
    std::vector<double> po;
    po.push_back(trigger_time);
    po.push_back(0);
    po.push_back(0);
    m_val->m_out_val_mm = _get_linear2(p1, p2, po, true)[2];
    double delta = m_val->m_out_val_mm-end_z;
    std::cout << "call_min_z, re_probe_cnt=" << m_val->m_re_probe_cnt << ", out_val_mm=" << m_val->m_out_val_mm << " end_z=" << end_z << " delta: " << delta << std::endl;
    // Calculate up_min_cnt and up_all_cnt
    int up_min_cnt = static_cast<int>((m_val->m_out_val_mm - end_z) / (m_obj->m_dirzctl->m_steppers[0]->get_step_dist() * m_obj->m_dirzctl->m_step_base));
    int up_all_cnt = dirzctl_params[0].PT_uint32_outs.at("step") - dirzctl_params[1].PT_uint32_outs.at("step") + 1;
    // int limt_up_cnt = static_cast<int>(10 / (m_obj->m_dirzctl->m_steppers[0]->get_step_dist() * m_obj->m_dirzctl->m_step_base));
    up_min_cnt = (up_min_cnt >= 0) ? up_min_cnt : 0;
    // up_min_cnt = (up_min_cnt < limt_up_cnt) ? up_min_cnt : limt_up_cnt;
    // up_all_cnt = (up_all_cnt < limt_up_cnt) ? up_all_cnt : limt_up_cnt;
    std::cout << "up_min_cnt=" << up_min_cnt << ", up_all_cnt=" << up_all_cnt << std::endl;
    if (std::fabs(delta)>3) {
        LOG_E("cal_min_z m_out_val_mm - end_z:%f  > 3\n",delta);
        Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStop(true);
        throw MCUException(Printer::GetInstance()->m_hx711s->m_mcu->m_serial->m_name, "No response from MCU");
        return std::make_tuple(up_min_cnt, up_all_cnt, false);
    }
    return std::make_tuple(up_min_cnt, up_all_cnt, true);
}

std::tuple<int, double, bool> StrainGaugeWrapper::probe_by_step(std::vector<double> rdy_pos, double speed_mm, double min_dis_mm, double min_hold, double max_hold, bool up_after, double up_dis_mm)
{
    // m_obj->m_hx711s->read_base((int)m_cfg->m_base_count / 2, max_hold);
    // m_obj->m_hx711s->CalibrationStart(m_cfg->m_base_count);

    int step_cnt = (int)(min_dis_mm / (m_obj->m_dirzctl->m_steppers[0]->get_step_dist() * m_obj->m_dirzctl->m_step_base));
    int step_us = (int)(((min_dis_mm / speed_mm) * 1000 * 1000) / step_cnt);
    Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStart(rdy_pos[0], rdy_pos[1], rdy_pos[2]);
    // m_obj->m_hx711s->query_start(m_cfg->m_pi_count * 2, 65535, true, false, true);
    m_obj->m_dirzctl->check_and_run(0, step_us, step_cnt, false, false);
    m_obj->m_hx711s->delay_s(0.015);
    // pnt_msg("*********************************************************");
    // pnt_msg("PROBE_BY_STEP x=%.2f y=%.2f z=%.2f speed_mm=%.2f step_us=%d step_cnt=%d", rdy_pos[0], rdy_pos[1], rdy_pos[2], speed_mm, step_us, step_cnt);
    std::cout << "PROBE_BY_STEP x= " << rdy_pos[0] << " y= " << rdy_pos[1] << " z= " << rdy_pos[2] << " speed_mm= " << speed_mm << " step_us= " << step_us
              << " step_cnt= " << step_cnt << std::endl;
    std::cout << "******************************probe_by_step****************************" << std::endl;
    std::tuple<int, int, bool> cal_result;
    while (ck_sys_sta())
    {
        if (m_obj->m_dirzctl->m_params.size() == 2)
        {
            LOG_E("axis z wait home timeout \n");
            probe_state_callback_call(probe_data_t{.state = CMD_BEDMESH_PROBE_EXCEPTION});
            if (up_after)
            {
                m_obj->m_dirzctl->check_and_run(1, step_us / 3, step_cnt);
            }
            Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStop(true);
            return std::make_tuple(0, 0, false);
        }
        // _check_trigger(i, fit_vals[i], unfit_vals[i], min_hold, max_hold)
        // if (!_check_trigger(i, fit_vals.tmp_vals[i], min_hold, max_hold))
        if (!(Printer::GetInstance()->m_hx711s->m_is_trigger > 0))
        {
            usleep(0.001);
            continue;
        }
        std::cout << "probe_by_step trigger!!!" << std::endl;
        m_obj->m_dirzctl->check_and_run(0, 0, 0, false);
        // m_obj->m_hx711s->delay_s(0.015);
        // m_obj->m_hx711s->delay_s(0.2);
        usleep(3 * 1000);
        std::vector<double> temp;
        cal_result = cal_min_z(rdy_pos[2], temp);
        std::cout << "cal_min_z result=" << std::get<1>(cal_result) << std::endl;
        // std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();
        // now_pos[2] += std::get<1>(cal_result) + m_obj->m_dirzctl->m_steppers[0]->get_step_dist();
        // Printer::GetInstance()->m_tool_head->set_position(now_pos, {2});
        if (up_after)
        {
            // int up_cnt = (int)(up_dis_mm / (m_obj->m_dirzctl->m_steppers[0]->get_step_dist() * m_obj->m_dirzctl->m_step_base));
            std::cout << "up_after : " << std::get<1>(cal_result) * m_obj->m_dirzctl->m_steppers[0]->get_step_dist() << std::endl;
            m_obj->m_dirzctl->check_and_run(1, step_us / 3, std::get<1>(cal_result));
        }

        if (!std::get<2>(cal_result)){
            LOG_E("cal_min_z result error!!!!!!!!!!!!!!\n");
            Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStop(true);
            return std::make_tuple(0, 0, false);
        }
        // 测试模式
        if (Printer::GetInstance()->m_hx711s->m_enable_test ==1 || Printer::GetInstance()->m_hx711s->m_enable_test ==2) {
            sleep(1);  
            for (int i=0; i<10; i++) {
                Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStop(true); 
                sleep(1);
            }
        } else {
            // usleep(1000);
            Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStop(true);
        }
        return std::make_tuple(m_val->m_out_index, m_val->m_out_val_mm, std::get<2>(cal_result));
    }
    return std::make_tuple(m_val->m_out_index, m_val->m_out_val_mm, true);
}

bool StrainGaugeWrapper::run_probe(bool need_shake, bool need_force_retract, bool need_cool_down)
{
    Printer::GetInstance()->m_tool_head->wait_moves();
    double target_temp = Printer::GetInstance()->m_printer_extruder->m_heater->m_target_temp;
    if (m_cfg->m_g28_wait_cool_down && Printer::GetInstance()->m_printer_extruder->m_heater->m_smoothed_temp > (m_cfg->m_hot_min_temp + 5) && need_cool_down)
    {
        // pnt_msg("G28_Z: Wait for Nozzle to cool down[%.2f -> %.2f]...", target_temp, m_cfg->m_hot_min_temp)
        printf("G28_Z: Wait for Nozzle to cool down[%.2f -> %.2f]...", target_temp, m_cfg->m_hot_min_temp);
        _set_hot_temps(m_cfg->m_hot_min_temp, 255, true, 5);
        _set_hot_temps(m_cfg->m_hot_min_temp, 0, false, 5);
    }
    if (need_shake)
    {
        shake_motor(20);
    }
    m_obj->m_hx711s->CalibrationStart(30, false);
    // m_obj->m_hx711s->read_base(20, m_cfg->m_g28_max_hold);
    std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();
    std::vector<double> now_pos_sta0 = {
        now_pos[0],
        now_pos[1],
        now_pos[2],
        now_pos[3]};
    ZMesh *mesh = m_obj->m_bed_mesh->get_mesh();
    m_obj->m_bed_mesh->set_mesh(nullptr);
    bool is_uped = false;
    auto result = probe_by_step(now_pos_sta0, m_cfg->m_g28_sta0_speed, m_cfg->m_max_z, m_cfg->m_g28_sta0_min_hold, m_cfg->m_g28_max_hold, false);

    // 设置当前z位置为0，并移动至z=2的地方，即z轴往归零的反方向移动2mm
    Printer::GetInstance()->m_tool_head->set_position({now_pos_sta0[0], now_pos_sta0[1], 0, now_pos_sta0[3]}, {2});
    printf("_move(%f,%f,%f) speed:%f\n", now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3], 2 * m_cfg->m_g28_sta1_speed);
    _move({now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3]}, 2 * m_cfg->m_g28_sta1_speed);
    now_pos_sta0 = Printer::GetInstance()->m_tool_head->get_position();

    // 归零结束后是否需要回退，即擦嘴动作
    if (!need_force_retract)
    {
        std::cout << "start wipe nozzle !!! " << std::endl;
        for (int i = 0; i < 2; i++)
        {
            probe_by_step(now_pos_sta0, m_cfg->m_g28_sta1_speed, m_cfg->m_max_z, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold, true);
        }
        auto result = probe_by_step(now_pos_sta0, m_cfg->m_g28_sta1_speed, m_cfg->m_max_z, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold, false);
        now_pos_sta0[2] -= std::get<1>(result) * m_obj->m_dirzctl->m_steppers[0]->get_step_dist();
        now_pos[2] = 0;
        Printer::GetInstance()->m_tool_head->set_position(now_pos, {2});
        std::cout << "end wipe nozzle !!! " << std::endl;
    }

    if (m_cfg->m_g28_wait_cool_down && need_cool_down)
    {
        printf("G28_Z: Wait for Nozzle to recovery[%.2f -> %.2f]...\n", m_cfg->m_hot_min_temp, target_temp);
        _set_hot_temps(target_temp, 0, target_temp > m_cfg->m_hot_min_temp);
    }
    m_obj->m_bed_mesh->set_mesh(mesh);
    return true;
}

bool StrainGaugeWrapper::run_G28_Z(Homing &homing_state, bool need_shake, bool need_force_retract, bool need_cool_down)
{
    if (!m_cfg->m_use_probe_by_step)
    {
        std::cout << "--------------run_G28_z use drop_move---------------" << std::endl;
        // Printer::GetInstance()->m_gcode_io->single_command("G91");
        bool is_succ = true;
        srv_state_home_msg_t home_msg;
        home_msg.axis = AXIS_Z;
        home_msg.st = SRV_STATE_HOME_HOMING;
        simple_bus_publish_async("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));

        m_val->m_re_probe_cnt = 0;
        m_val->m_g29_cnt = 0;
        Printer::GetInstance()->m_tool_head->wait_moves();
        double target_temp = Printer::GetInstance()->m_printer_extruder->m_heater->m_target_temp;
        if (m_cfg->m_g28_wait_cool_down && Printer::GetInstance()->m_printer_extruder->m_heater->m_smoothed_temp > (m_cfg->m_hot_min_temp + 5) && need_cool_down)
        {
            // pnt_msg("G28_Z: Wait for Nozzle to cool down[%.2f -> %.2f]...", target_temp, m_cfg->m_hot_min_temp);
            printf("G28_Z: Wait for Nozzle to cool down[%.2f -> %.2f]...", target_temp, m_cfg->m_hot_min_temp);
            _set_hot_temps(m_cfg->m_hot_min_temp, 255, true, 5);
            _set_hot_temps(m_cfg->m_hot_min_temp, 0, false, 5);
        }
        if (need_shake)
        {
            shake_motor(20);
        }
        m_obj->m_hx711s->CalibrationStart(30, false);
        // m_obj->m_hx711s->read_base(20, m_cfg->m_g28_max_hold);

        // bool is_uped = false;

        // for(int i = 0;i < 10; i++)
        // {
        //     Printer::GetInstance()->m_tool_head->set_position({now_pos[0], now_pos[1], i==0?2:0, now_pos[3]}, {2});
        //     _move(now_pos, m_cfg->m_g29_rdy_speed);
        //     double z_val = hmove.homing_z_move(home_pos, hi.speed, true)[2];
        // }
        // shake_motor(8);

        // Printer::GetInstance()->m_gcode_io->single_command("G91");
        // for (int i = 0; i < 100; i++)
        // {
        //     Printer::GetInstance()->m_gcode_io->single_command("G1 Z-0.5 F3000");
        //     Printer::GetInstance()->m_gcode_io->single_command("G1 Z0.5 F3000");
        // }
        // Printer::GetInstance()->m_tool_head->wait_moves();
        std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();

        // Determine movement
        double position_min = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_range()[0];
        double position_max = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_range()[1];
        struct homingInfo hi = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_homing_info();
        std::vector<double> homepos = {DO_NOT_MOVE_F, DO_NOT_MOVE_F, DO_NOT_MOVE_F, DO_NOT_MOVE_F};
        homepos[2] = hi.position_endstop;
        printf("position_endstop:%f", hi.position_endstop);
        std::vector<double> forcepos = homepos;
        if (hi.positive_dir)
            forcepos[2] -= 1.5 * (hi.position_endstop - position_min);
        else
            forcepos[2] += 1.5 * (position_max - hi.position_endstop);
        // Perform homing
        std::vector<double> force_pos = {forcepos[0], forcepos[1], forcepos[2], forcepos[3]};
        std::vector<double> home_pos = {homepos[0], homepos[1], homepos[2], homepos[3]};

        std::vector<PrinterRail *> rails = {Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]};
        double in_homing_print_fan_speed = 0.0f;
        // Notify of upcoming homing operation
        Printer::GetInstance()->send_event("homing:home_rails_begin", &homing_state, rails);
        // Alter kinematics class to think printer is at forcepos
        GAM_DEBUG_send_UI_home("2-H3-\n");
        std::vector<int> homing_axes;
        for (int i = 0; i < 3; i++)
        {
            if (force_pos[i] != DO_NOT_MOVE_F)
            {
                homing_axes.push_back(i);
            }
        }
        force_pos = homing_state.fill_coord(force_pos);
        home_pos = homing_state.fill_coord(home_pos);
        LOG_D("force_pos: %f %f %f %f\n", force_pos[0], force_pos[1], force_pos[2], force_pos[3]);
        LOG_D("home_pos: %f %f %f %f\n", home_pos[0], home_pos[1], home_pos[2], home_pos[3]);
        Printer::GetInstance()->m_tool_head->set_position(force_pos, homing_axes); //--4-home-2task-G-G--UI_control_task--
        std::vector<MCU_endstop *> endstops;
        for (int i = 0; i < rails.size(); i++)
        {
            std::vector<MCU_endstop *> ret_endstops = rails[i]->get_endstops();
            for (int j = 0; j < ret_endstops.size(); j++)
            {
                endstops.push_back(ret_endstops[j]);
            }
        }
        hi = rails[0]->get_homing_info();
        HomingMove hmove(endstops);
        homing_state_callback_call(HOMING_STATE_SEED_LIMIT); // 回调

        hmove.homing_z_move(home_pos, hi.speed);
        Printer::GetInstance()->m_tool_head->set_position(home_pos);

        int home_cnt = 2;
        // double home_val = 0;
        // std::vector<double> z_mms;
        double total_retract_dist = hi.retract_dist;
        int samples = 3;
        bool done = false;
        int sample_times = 0;
        while (!done)
        {
            if (sample_times >= 5)
            {
                // Z轴归零失败
                is_succ = false;
                break;
            }
            std::vector<double> home_val;
            for (int i = 0; i < samples; i++)
            {
                std::cout << "------------------------------" << i << "-------------------------------" << std::endl;

                // hi.retract_dist = 3;
                // if (fabs(hi.retract_dist) > 1e-15)
                // if (fabs(hi.retract_dist) > 1e-15)
                // {
                //     total_retract_dist = hi.retract_dist;
                // }
                if (fabs(total_retract_dist) > 1e-15)
                {
                    // std::cout << "home_pos: " << home_pos[0] << " " << home_pos[1] << " " << home_pos[2] << " " << home_pos[3] << std::endl;
                    // Printer::GetInstance()->m_tool_head->set_position(home_pos);
                    std::vector<double> before_retractpos = Printer::GetInstance()->m_tool_head->get_position();
                    // std::cout << "before_retractpos: " << before_retractpos[0] << " " << before_retractpos[1] << " " << before_retractpos[2] << " " << before_retractpos[3] << std::endl;
                    std::vector<double> axes_d = {home_pos[0] - force_pos[0], home_pos[1] - force_pos[1], home_pos[2] - force_pos[2], home_pos[3] - force_pos[3]};
                    // std::cout << "home_pos: " << home_pos[0] << " " << home_pos[1] << " " << home_pos[2] << " " << home_pos[3] << std::endl;
                    // std::cout << "force_pos: " << force_pos[0] << " " << force_pos[1] << " " << force_pos[2] << " " << force_pos[3] << std::endl;
                    double move_d = sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);
                    // std::cout << "move_d: " << move_d << std::endl;
                    // std::cout << "total_retract_dist: " << total_retract_dist << std::endl;
                    double retract_r = std::min(1.0, total_retract_dist / move_d);
                    // std::cout << "retract_r: " << retract_r << std::endl;
                    std::vector<double> retractpos = {home_pos[0] - axes_d[0] * retract_r, home_pos[1] - axes_d[1] * retract_r, home_pos[2] - axes_d[2] * retract_r, home_pos[3] - axes_d[3] * retract_r};
                    // std::cout << "retractpos: " << retractpos[0] << " " << retractpos[1] << " " << retractpos[2] << " " << retractpos[3] << std::endl;
                    Printer::GetInstance()->m_tool_head->move(retractpos, hi.retract_speed); // 回退
                    homing_state_callback_call(HOMING_STATE_OUT_LIMIT);                      // 回调
                    Printer::GetInstance()->m_tool_head->wait_moves();                       // 等待回退完成

                    // forcepos = {retractpos[0] - axes_d[0] * retract_r, retractpos[1] - axes_d[1] * retract_r, retractpos[2] - axes_d[2] * retract_r, retractpos[3] - axes_d[3] * retract_r};
                    // std::cout << "force_pos: " << force_pos[0] << " " << force_pos[1] << " " << force_pos[2] << " " << force_pos[3] << std::endl;
                    Printer::GetInstance()->m_tool_head->set_position(force_pos);
                    HomingMove homemove(endstops);

                    homing_state_callback_call(HOMING_STATE_SEED_LIMIT); // 再次找限位
                    // std::cout << "homing_z_move " << get_monotonic() << std::endl;
                    double z_val = hmove.homing_z_move(home_pos, hi.second_homing_speed, true);
                    // total_retract_dist = -z_val;
                    // std::cout << "----------------------z_val = " << z_val << "-----------------" << std::endl;
                    // z_mms.push_back(z_val);
                    home_val.push_back(z_val);
                    // if (fabs(home_val - z_val) > 0)
                    // {
                    //     home_val = z_val;
                    //     home_cnt++;
                    //     // std::cout << "-----------------------------cnt -" << home_cnt << "-----" << home_val << "--------------------------" << std::endl;
                    // }

                    // if (homemove.check_no_movement() != "")
                    // {
                    //     // raise self.printer.command_error( "Endstop %s still triggered after retract" % (hmove.check_no_movement(),))
                    // }
                }
            }
            if (home_val.size() == samples)
            {
                // std::vector<double> fabs_diff_vals;
                for (int i = 0; i < home_val.size() - 1; i++)
                {
                    for (int j = 1; j < home_val.size(); j++)
                    {
                        // fabs_diff_vals.push_back(fabs(home_val[i] - home_val[j]));
                        if (fabs(home_val[i] - home_val[j]) < 0.05)
                        {
                            double z = (home_val[i] * 3 + home_val[j] * 2) / 5;
                            std::cout << "z = " << z << std::endl;
                            std::vector<double> finish_home_pos = Printer::GetInstance()->m_tool_head->get_position();
                            std::cout << "1finish_home_pos: " << finish_home_pos[0] << " " << finish_home_pos[1] << " " << finish_home_pos[2] << " " << finish_home_pos[3] << std::endl;
                            finish_home_pos[2] = home_val.back() - z + hi.position_endstop;
                            Printer::GetInstance()->m_tool_head->set_position(finish_home_pos);
                            std::cout << "2finish_home_pos: " << finish_home_pos[0] << " " << finish_home_pos[1] << " " << finish_home_pos[2] << " " << finish_home_pos[3] << std::endl;
                            done = true;
                            i = home_val.size();
                            break;
                        }
                    }
                }
            }
            sample_times++;
            // if (i > 10)
            // {
            //     LOG_E("z home failed !!!\n");
            //     is_succ = false;
            //     break;
            // }
        }

        // int home_cnt = 2;
        // double home_val = 0;
        // std::vector<double> z_mms;
        // for (int i = 0; i < home_cnt; i++)
        // {
        //     std::cout << "------------------------------" << i << "-------------------------------" << std::endl;
        //     if (i > 5)
        //     {
        //         LOG_E("z home failed !!!\n");
        //         break;
        //     }
        //     hi.retract_dist = 3;
        //     if (fabs(hi.retract_dist) > 1e-15)
        //     {
        //         std::cout << "home_pos: " << home_pos[0] << " " << home_pos[1] << " " << home_pos[2] << " " << home_pos[3] << std::endl;
        //         // Printer::GetInstance()->m_tool_head->set_position(home_pos);
        //         std::vector<double> axes_d = {home_pos[0] - force_pos[0], home_pos[1] - force_pos[1], home_pos[2] - force_pos[2], home_pos[3] - force_pos[3]};
        //         double move_d = sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);
        //         double retract_r = std::min(1.0, hi.retract_dist / move_d);
        //         std::vector<double> retractpos = {home_pos[0] - axes_d[0] * retract_r, home_pos[1] - axes_d[1] * retract_r, home_pos[2] - axes_d[2] * retract_r, home_pos[3] - axes_d[3] * retract_r};
        //         std::cout << "retractpos: " << retractpos[0] << " " << retractpos[1] << " " << retractpos[2] << " " << retractpos[3] << std::endl;
        //         Printer::GetInstance()->m_tool_head->move(retractpos, hi.retract_speed); // 回退
        //         homing_state_callback_call(HOMING_STATE_OUT_LIMIT);                      // 回调
        //         Printer::GetInstance()->m_tool_head->wait_moves();                       // 等待回退完成

        //         forcepos = {retractpos[0] - axes_d[0] * retract_r, retractpos[1] - axes_d[1] * retract_r, retractpos[2] - axes_d[2] * retract_r, retractpos[3] - axes_d[3] * retract_r};
        //         std::cout << "forcepos: " << forcepos[0] << " " << forcepos[1] << " " << forcepos[2] << " " << forcepos[3] << std::endl;
        //         Printer::GetInstance()->m_tool_head->set_position(forcepos);
        //         HomingMove homemove(endstops);

        //         homing_state_callback_call(HOMING_STATE_SEED_LIMIT); // 再次找限位
        //         std::cout << "homing_z_move " << get_monotonic() << std::endl;
        //         double z_val = hmove.homing_z_move(home_pos, hi.speed, true)[2];
        //         std::cout << "z_val = " << z_val << std::endl;
        //         z_mms.push_back(z_val);
        //         if (fabs(home_val - z_val) > 0.1)
        //         {
        //             home_val = z_val;
        //             home_cnt++;
        //             std::cout << "-----------------------------cnt -" << home_cnt << "-----" << home_val << "--------------------------" << std::endl;
        //         }

        //         if (homemove.check_no_movement() != "")
        //         {
        //             // raise self.printer.command_error( "Endstop %s still triggered after retract" % (hmove.check_no_movement(),))
        //         }
        //     }
        // }
        // std::sort(z_mms.begin(), z_mms.end());
        // now_pos = Printer::GetInstance()->m_tool_head->get_position();
        // std::cout << "---------988--------z : " << z_mms[z_mms.size() / 2] - Printer::GetInstance()->m_strain_gauge->m_cfg->m_self_z_offset << std::endl;
        // Printer::GetInstance()->m_tool_head->set_position({now_pos[0], now_pos[1], z_mms[z_mms.size() / 2] - Printer::GetInstance()->m_strain_gauge->m_cfg->m_self_z_offset, now_pos[3]}, {2});
        // std::cout << "now_pos : " << z_mms[z_mms.size() / 2] - Printer::GetInstance()->m_strain_gauge->m_cfg->m_self_z_offset << std::endl;
        if (is_succ)
        {
            // Printer::GetInstance()->m_tool_head->set_position(home_pos);
            Printer::GetInstance()->m_tool_head->flush_step_generation();
            std::map<std::string, double> kin_spos;
            std::vector<std::vector<MCU_stepper *>> kin_steppers = Printer::GetInstance()->m_tool_head->m_kin->get_steppers();
            for (int i = 0; i < kin_steppers.size(); i++)
            {
                for (int j = 0; j < kin_steppers[i].size(); j++)
                {
                    kin_spos[kin_steppers[i][j]->get_name()] = kin_steppers[i][j]->get_commanded_position();
                }
            }
            homing_state.m_kin_spos = kin_spos;
            Printer::GetInstance()->send_event("homing:home_rails_end", &homing_state, rails);
            if (kin_spos != homing_state.m_kin_spos)
            {
                // Apply any homing offsets
                std::vector<double> adjustpos = Printer::GetInstance()->m_tool_head->m_kin->calc_position(homing_state.m_kin_spos);
                for (int axis = 0; axis < homing_axes.size(); axis++)
                {
                    home_pos[axis] = adjustpos[axis];
                }
                Printer::GetInstance()->m_tool_head->set_position(home_pos);
                // std::cout << "home_pos: " << home_pos[0] << "," << home_pos[1] << "," << home_pos[2] << "," << home_pos[3] << std::endl;
            }
            // now_pos = Printer::GetInstance()->m_tool_head->get_position();
            // now_pos[2] = 10;
            // Printer::GetInstance()->m_tool_head->move(now_pos, hi.speed * 2);
        }
        if (fabs(hi.force_retract) > 1e-15 && need_force_retract)
        {
            std::vector<double> axes_d = {home_pos[0] - force_pos[0], home_pos[1] - force_pos[1], home_pos[2] - force_pos[2], home_pos[3] - force_pos[3]};
            double move_d = sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);
            double retract_r = std::min(1.0, hi.force_retract / move_d);
            std::vector<double> retractpos = {home_pos[0] - axes_d[0] * retract_r, home_pos[1] - axes_d[1] * retract_r, home_pos[2] - axes_d[2] * retract_r, home_pos[3] - axes_d[3] * retract_r};
            // std::cout << "retractpos: " << retractpos[0] << " " << retractpos[1] << " " << retractpos[2] << " " << retractpos[3] << std::endl;
            Printer::GetInstance()->m_tool_head->move(retractpos, hi.retract_speed); // 回退
            homing_state_callback_call(HOMING_STATE_OUT_LIMIT);                      // 回调
            Printer::GetInstance()->m_tool_head->wait_moves();                       // 等待回退完成
            Printer::GetInstance()->m_tool_head->set_position(retractpos);
        }

        if (m_cfg->m_g28_wait_cool_down && need_cool_down)
        {
            // pnt_msg("G28_Z: Wait for Nozzle to recovery[%.2f -> %.2f]...", m_cfg->hot_min_temp, target_temp);
            _set_hot_temps(target_temp, 0, target_temp > m_cfg->m_hot_min_temp);
        }
        std::vector<double> after_g28_pos = Printer::GetInstance()->m_tool_head->get_position();
        // std::cout << "after_g28_pos: " << after_g28_pos[0] << "," << after_g28_pos[1] << "," << after_g28_pos[2] << "," << after_g28_pos[3] << std::endl;
        if (is_succ)
            home_msg.st = SRV_STATE_HOME_END_SUCCESS;
        else
        {
            home_msg.st = SRV_STATE_HOME_END_FAILED;
            Printer::GetInstance()->m_tool_head->m_kin->motor_off(0); // 标记归零失败
        }
        simple_bus_publish_async("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));
        return hmove.is_succ;
    }
    else
    {
        // pnt_msg("***run_G28_Z*** Start...");
        std::cout << "--------------run_G28_z use probe_by_step---------------" << " need_force_retract:" << (int32_t)need_force_retract << std::endl;
        struct homingInfo hi = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_homing_info();
        std::cout << "hi.position_endstop : " << hi.position_endstop << std::endl;
        bool is_succ = false;
        srv_state_home_msg_t home_msg;
        home_msg.axis = AXIS_Z;
        home_msg.st = SRV_STATE_HOME_HOMING;
        simple_bus_publish_async("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));

        m_val->m_re_probe_cnt = 0;
        m_val->m_g29_cnt = 0;
        Printer::GetInstance()->m_tool_head->wait_moves();
        double target_temp = Printer::GetInstance()->m_printer_extruder->m_heater->m_target_temp;
        if (m_cfg->m_g28_wait_cool_down && Printer::GetInstance()->m_printer_extruder->m_heater->m_smoothed_temp > (m_cfg->m_hot_min_temp + 5) && need_cool_down)
        {
            // pnt_msg("G28_Z: Wait for Nozzle to cool down[%.2f -> %.2f]...", target_temp, m_cfg->m_hot_min_temp)
            printf("G28_Z: Wait for Nozzle to cool down[%.2f -> %.2f]...", target_temp, m_cfg->m_hot_min_temp);
            _set_hot_temps(m_cfg->m_hot_min_temp, 255, true, 5);
            _set_hot_temps(m_cfg->m_hot_min_temp, 0, false, 5);
        }
        if (need_shake)
        {
            shake_motor(20);
        }
        m_obj->m_hx711s->CalibrationStart(30, false);
        // m_obj->m_hx711s->read_base(20, m_cfg->m_g28_max_hold);
        std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();
        std::vector<double> now_pos_sta0 = {
            now_pos[0],
            now_pos[1],
            now_pos[2],
            now_pos[3]};
        ZMesh *mesh = m_obj->m_bed_mesh->get_mesh();
        m_obj->m_bed_mesh->set_mesh(nullptr);
        bool is_uped = false;
        auto result = probe_by_step(now_pos_sta0, m_cfg->m_g28_sta0_speed, m_cfg->m_max_z, m_cfg->m_g28_sta0_min_hold, m_cfg->m_g28_max_hold, false);

        // 设置当前z位置为0，并移动至z=2的地方，即z轴往归零的反方向移动2mm
        Printer::GetInstance()->m_tool_head->set_position({now_pos_sta0[0], now_pos_sta0[1], 0, now_pos_sta0[3]}, {2});
        printf("_move(%f,%f,%f) speed:%f\n", now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3], 2 * m_cfg->m_g28_sta1_speed);
        _move({now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3]}, 2 * m_cfg->m_g28_sta1_speed);
        now_pos_sta0 = Printer::GetInstance()->m_tool_head->get_position();

        // 归零结束后是否需要回退，即擦嘴动作
        if (!need_force_retract)
        {
            std::cout << "start wipe nozzle !!! " << std::endl;
            for (int i = 0; i < 2; i++)
            {
                m_obj->m_hx711s->CalibrationStart(30,false);
                probe_by_step(now_pos_sta0, m_cfg->m_g28_sta1_speed, m_cfg->m_max_z, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold, true);
            }
            m_obj->m_hx711s->CalibrationStart(30,false);
            probe_by_step(now_pos_sta0, m_cfg->m_g28_sta1_speed, m_cfg->m_max_z, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold, false);
            
            std::cout << "end wipe nozzle !!! " << std::endl;
            is_succ = true;
        }
        else // 正常z轴归零
        {
#if 0
            int error_times = 0;
            int g28_times = 10;
            while (error_times < 3 && !is_succ)
            {
                if (error_times)
                {
                    m_obj->m_hx711s->CalibrationStart(30,false);
                    auto result = probe_by_step(now_pos_sta0, m_cfg->m_g28_sta0_speed, m_cfg->m_max_z, m_cfg->m_g28_sta0_min_hold, m_cfg->m_g28_max_hold, false);
                    // 设置当前z位置为0，并移动至z=2的地方，即z轴往归零的反方向移动2mm
                    Printer::GetInstance()->m_tool_head->set_position({now_pos_sta0[0], now_pos_sta0[1], 0, now_pos_sta0[3]}, {2});
                    printf("_move(%f,%f,%f) speed:%f\n", now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3], 2 * m_cfg->m_g28_sta1_speed);
                    _move({now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3]}, 2 * m_cfg->m_g28_sta1_speed);
                }
                std::vector<double> out_mms;
                double homing_speed = m_cfg->m_g28_sta1_speed;
                for (int i = 0; i < g28_times; i++)
                {
                    std::vector<double> now_pos_copy = now_pos_sta0;
                    // 探测4mm
                    m_obj->m_hx711s->CalibrationStart(30,false);
                    auto o_result = probe_by_step(now_pos_copy, homing_speed, m_cfg->m_g28_max_try, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold, false);
                    Printer::GetInstance()->m_tool_head->set_position({now_pos_sta0[0], now_pos_sta0[1], 0, now_pos_sta0[3]}, {2});
                    _move({now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3]}, 2 * homing_speed);
                    if (!std::get<2>(o_result))
                    {
                        error_times++;
                        break;
                    }
                    double o_mm0 = std::get<1>(o_result);
                    std::cout << "o_mm0 : " << o_mm0 << " i : " << i << std::endl;
                    out_mms.push_back(o_mm0);
                    if (out_mms.size() >= 2)
                    {
                        if (std::fabs(out_mms.back() - out_mms.at(out_mms.size() - 2)) < m_cfg->m_g28_max_err && fabs(out_mms.back()) < 1)
                        {
                            is_succ = true;
                            double z_val = (out_mms.back() + out_mms.at(out_mms.size() - 2)) / 2.;
                            std::vector<double> new_pos = {now_pos_sta0[0], now_pos_sta0[1], now_pos_sta0[2] - z_val - m_cfg->m_self_z_offset + hi.position_endstop, now_pos_sta0[3]};
                            Printer::GetInstance()->m_tool_head->set_position(new_pos, {2});
                            break;
                        }
                        else
                        {
                            homing_speed = std::max(homing_speed * 0.5, 1.);
                        }
                    }
                }
                if (out_mms.size() >= g28_times)
                {
                    is_succ = false;
                    LOG_E("homing z failure ! The number of attempts has been exhausted \n");
                    break;
                }
            }
            if (error_times >= 3)
            {
                is_succ = false;
                LOG_E("homing z failure ! \n");
                std::cout << "error ! Homing Z failure, During zeroing, please place the machine on a stable platform and do not touch the hot bed. " << std::endl;
                // throw printer.command_err;
            }
            // std::cout << "now_pos_sta0[2] : " << now_pos_sta0[2] << std::endl;
            // std::cout << "ave : " << ave << std::endl;
            // std::cout << "m_cfg->m_self_z_offset : " << m_cfg->m_self_z_offset << std::endl;
            // std::cout << "hi.position_endstop : " << hi.position_endstop << std::endl;
            // std::cout << "new_pos[2] : " << new_pos[2] << std::endl;
            // printf("_move(%f,%f,%f) speed:",now_pos[0], now_pos[1], 10, now_pos[3],m_cfg->m_g29_rdy_speed);
            _move({now_pos[0], now_pos[1], 10, now_pos[3]}, 2 * m_cfg->m_g28_sta1_speed);
#else
            int error_times = 0;
            int g28_times = 10;
            while (error_times < 3 && !is_succ)
            {
                if (error_times)
                {
                    m_obj->m_hx711s->CalibrationStart(30,false);
                    auto result = probe_by_step(now_pos_sta0, m_cfg->m_g28_sta0_speed, m_cfg->m_max_z, m_cfg->m_g28_sta0_min_hold, m_cfg->m_g28_max_hold, false);
                    // 设置当前z位置为0，并移动至z=2的地方，即z轴往归零的反方向移动2mm
                    Printer::GetInstance()->m_tool_head->set_position({now_pos_sta0[0], now_pos_sta0[1], 0, now_pos_sta0[3]}, {2});
                    printf("_move(%f,%f,%f) speed:%f\n", now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3], 2 * m_cfg->m_g28_sta1_speed);
                    _move({now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3]}, 2 * m_cfg->m_g28_sta1_speed);
                }
                std::vector<double> out_mms;
                double homing_speed = m_cfg->m_g28_sta1_speed;
                for (int i = 0; i < g28_times; i++)
                {
                    std::vector<double> now_pos_copy = now_pos_sta0;
                    // 探测4mm
                    m_obj->m_hx711s->CalibrationStart(30,false);
                    auto o_result = probe_by_step(now_pos_copy, homing_speed, m_cfg->m_g28_max_try, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold, true);
                    // Printer::GetInstance()->m_tool_head->set_position({now_pos_sta0[0], now_pos_sta0[1], 0, now_pos_sta0[3]}, {2});
                    // _move({now_pos_sta0[0], now_pos_sta0[1], 2, now_pos_sta0[3]}, 2 * homing_speed);
                    if (!std::get<2>(o_result))
                    {
                        error_times++;
                        break;
                    }
                    double o_mm0 = std::get<1>(o_result);
                    std::cout << "o_mm0 : " << o_mm0 << " i : " << i << std::endl;
                    out_mms.push_back(o_mm0);
                    if (out_mms.size() >= 2)
                    {
                        if (std::fabs(out_mms.back() - out_mms.at(out_mms.size() - 2)) < m_cfg->m_g28_max_err && fabs(out_mms.back()) < 1)
                        {
                            is_succ = true;
                            double z_val = (out_mms.back() + out_mms.at(out_mms.size() - 2)) / 2.;
                            std::vector<double> new_pos = {now_pos_sta0[0], now_pos_sta0[1], now_pos_sta0[2] - z_val - m_cfg->m_self_z_offset + hi.position_endstop, now_pos_sta0[3]};
                            Printer::GetInstance()->m_tool_head->set_position(new_pos, {2});
                            break;
                        }
                        else
                        {
                            homing_speed = std::max(homing_speed * 0.5, 1.);
                        }
                    }
                }
                if (out_mms.size() >= g28_times)
                {
                    is_succ = false;
                    LOG_E("homing z failure ! The number of attempts has been exhausted \n");
                    break;
                }
            }
            if (error_times >= 3)
            {
                is_succ = false;
                LOG_E("homing z failure ! \n");
                std::cout << "error ! Homing Z failure, During zeroing, please place the machine on a stable platform and do not touch the hot bed. " << std::endl;
                // throw printer.command_err;
            }
            // std::cout << "now_pos_sta0[2] : " << now_pos_sta0[2] << std::endl;
            // std::cout << "ave : " << ave << std::endl;
            // std::cout << "m_cfg->m_self_z_offset : " << m_cfg->m_self_z_offset << std::endl;
            // std::cout << "hi.position_endstop : " << hi.position_endstop << std::endl;
            // std::cout << "new_pos[2] : " << new_pos[2] << std::endl;
            // printf("_move(%f,%f,%f) speed:",now_pos[0], now_pos[1], 10, now_pos[3],m_cfg->m_g29_rdy_speed);
            _move({now_pos[0], now_pos[1], 10, now_pos[3]}, 2 * m_cfg->m_g28_sta1_speed);
#endif
        }
        m_obj->m_bed_mesh->set_mesh(mesh);

        if (m_cfg->m_g28_wait_cool_down && need_cool_down)
        {
            // pnt_msg("G28_Z: Wait for Nozzle to recovery[%.2f -> %.2f]...", m_cfg->hot_min_temp, target_temp);
            printf("G28_Z: Wait for Nozzle to recovery[%.2f -> %.2f]...\n", m_cfg->m_hot_min_temp, target_temp);
            _set_hot_temps(target_temp, 0, target_temp > m_cfg->m_hot_min_temp);
        }

        if (is_succ)
            home_msg.st = SRV_STATE_HOME_END_SUCCESS;
        else
            home_msg.st = SRV_STATE_HOME_END_FAILED;
        simple_bus_publish_async("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));
    }
}

std::vector<double> StrainGaugeWrapper::run_G29_Z(HomingMove *hmove)
{
    if (!m_cfg->m_use_probe_by_step)
    {
        std::cout << "-----------------run G29 Z use drip_move-----------------" << std::endl;
        Printer::GetInstance()->m_tool_head->wait_moves();
        std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();
        m_val->m_jump_probe_ready = false;

        if (m_val->m_g29_cnt == 0 || m_val->m_g29_cnt % 12 == 0)
        {
            shake_motor(20);
        }
        m_obj->m_hx711s->CalibrationStart(30, false);
        m_val->m_g29_cnt++;
        std::vector<double> g29_z_move_result = hmove->G29_z_move(now_pos, m_cfg->m_g29_speed, true);    
        double probe_result = g29_z_move_result[2];
        now_pos[2] = probe_result;
        return now_pos;
    }
    else
    {
        std::cout << "-----------------run G29 Z use probe_by_step-----------------" << std::endl;
        Printer::GetInstance()->m_reactor->pause(get_monotonic());
        Printer::GetInstance()->m_tool_head->wait_moves();
        std::vector<double> now_pos = Printer::GetInstance()->m_tool_head->get_position();
        m_val->m_jump_probe_ready = false;
        if (m_val->m_g29_cnt == 0 || m_val->m_g29_cnt % 12 == 0)
        {
            shake_motor(20);
        }
        m_obj->m_hx711s->CalibrationStart(30, false);
        m_val->m_g29_cnt++;
        
        double max_z_detection = now_pos[2] - m_cfg->m_g29_max_detection;
        std::cout << "max_z_detection:" << max_z_detection << std::endl;
        std::tuple<int, double, bool> result = probe_by_step(now_pos, m_cfg->m_g29_speed, max_z_detection, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold, true);

        Printer::GetInstance()->m_tool_head->wait_moves();
        now_pos = Printer::GetInstance()->m_tool_head->get_position();
        if (!std::get<2>(result))
        {
            now_pos[2] -= 10;
            Printer::GetInstance()->m_tool_head->move(now_pos, 360);
            throw std::runtime_error("G29 probe_by_step failed");
            return now_pos;
        } 
        now_pos[2] = std::get<1>(result);
        now_pos[3] = m_val->m_end_z_mm;
        std::cout << "***************now_pos[2] : " << now_pos[2] << "*****************" << std::endl;

        return now_pos;
    }
}

void StrainGaugeWrapper::cmd_STRAINGAUGE_Z_HOME(GCodeCommand &gcmd)
{
    // Homing hmove;
    // hmove.set_axes({2});
    // Printer::GetInstance()->m_strain_gauge->run_G28_Z(hmove, false, false, false);
    Printer::GetInstance()->m_strain_gauge->run_probe(false, false, false);
    // Printer::GetInstance()->m_strain_gauge->run_G29_Z(nullptr);
}

std::string cmd_STRAINGAUGE_TEST_help = "Test the PR-Touch.";
void StrainGaugeWrapper::cmd_STRAINGAUGE_TEST(GCodeCommand &gcmd)
{
    std::vector<double> pos = Printer::GetInstance()->m_tool_head->get_position();
    double rdy_x = gcmd.get_double("X", pos[0]);
    double rdy_y = gcmd.get_double("Y", pos[1]);
    double rdy_z = gcmd.get_double("Z", pos[2]);
    double speed = gcmd.get_double("SPEED", 1.);
    int min_hold = gcmd.get_int("MIN_HOLD", m_cfg->m_g28_min_hold);
    int max_hold = gcmd.get_int("MAX_HOLD", m_cfg->m_g28_max_hold);
    _move({rdy_x, rdy_y, rdy_z}, m_cfg->m_g29_xy_speed);
    probe_by_step({rdy_x, rdy_y, rdy_z}, speed, 50, min_hold, max_hold, true); 
}

std::string cmd_PRTOUCH_READY_help = "Test the ready point.";
void StrainGaugeWrapper::cmd_PRTOUCH_READY(GCodeCommand &gcmd)
{
    probe_ready();
}

std::string cmd_CHECK_BED_MESH_help = "Check the bed mesh.";
void StrainGaugeWrapper::cmd_CHECK_BED_MESH(GCodeCommand &gcmd)
{
    check_bed_mesh(gcmd.get_int("AUTO_G29", 0) > 0);
}

std::string cmd_MEASURE_GAP_TEST_help = "Measure z gap.";
void StrainGaugeWrapper::cmd_MEASURE_GAP_TEST(GCodeCommand &gcmd)
{
    _ck_g28ed();
    int gap_index = gcmd.get_int("INDEX", 0);
    std::vector<int> n_index = {0, 3, 1, 2};
    _move({m_val->m_rdy_pos[n_index[gap_index]][0], m_val->m_rdy_pos[n_index[gap_index]][1], m_cfg->m_bed_max_err + 1.}, m_cfg->m_g29_xy_speed);
    double zero_z = _probe_times(10, {m_val->m_rdy_pos[n_index[gap_index]][0], m_val->m_rdy_pos[n_index[gap_index]][1], m_cfg->m_bed_max_err + 1.},
                                 m_cfg->m_g29_speed, 10, 0.05, m_cfg->m_g28_min_hold, m_cfg->m_g28_max_hold);
    _gap_times(5, zero_z);
}

std::string cmd_NOZZLE_CLEAR_help = "Clear the nozzle on bed.";
void StrainGaugeWrapper::cmd_NOZZLE_CLEAR(GCodeCommand &gcmd)
{
    // Implement the NOZZLE_CLEAR command logic here
}

void StrainGaugeWrapper::change_hot_min_temp(double temp)
{
    m_cfg->m_hot_min_temp = temp;
}

bool StrainGaugeWrapper::wait_home()
{
    uint32_t loop = 0;
    while (1)
    {
        loop++;
        if (loop % 10000 == 0)
        {
            LOG_I("wait home trigger...\n");
        }
        // if (Printer::GetInstance()->m_probe->m_error) {
        //     LOG_I("probe error, break...\n");
        //     break;
        // }
        if (!(Printer::GetInstance()->m_hx711s->m_is_trigger > 0))
        {
            usleep(500);
            continue;
        }
        LOG_I("home trigger!!!!!!!!!!!! at:%f\n", get_monotonic());
        Printer::GetInstance()->m_tool_head->is_trigger = true;
        // // 停止检测
        Printer::GetInstance()->m_hx711s->ProbeCheckTriggerStop(true);
        // m_tri_val = fit_vals.tmp_vals[0];
        return true;
    }
}