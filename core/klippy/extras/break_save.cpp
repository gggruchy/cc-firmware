#include "break_save.h"
#include "klippy.h"
#include "my_string.h"
#include "Define_config_path.h"
#include "debug.h"
#include "gpio.h"
#include "app_print.h"
#define LOG_TAG "break_save"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

BreakSave::BreakSave(std::string section_name)
{
	// cmd_SAVE_GCODE_STATE
	Printer::GetInstance()->register_event_handler("klippy:connect:BreakSave", std::bind(&BreakSave::handle_connect, this));
	m_break_save_enable = true;
	m_break_save_files_num = Printer::GetInstance()->m_pconfig->GetInt(section_name, "save_files_num", 10, 2, 30);
	m_break_save_path = Printer::GetInstance()->m_pconfig->GetString(section_name, "break_save_path", BREAK_SAVE_PATH); // 头部应该被命令移动到的高度（以毫米为单位）就在开始探测操作之前。默认值为 5。
	std::string gcode_cmds_str = Printer::GetInstance()->m_pconfig->GetString(section_name, "gcode_cmds_before", "");	// 头部应该被命令移动到的高度（以毫米为单位）就在开始探测操作之前。默认值为 5。
	m_break_save_name = 0;
	m_break_save_flag = false;
	m_break_save_files_status = false;
	m_break_save_reserve = false;

	if (gpio_is_init(BREAK_DETECTION_GPIO) < 0)
	{
		gpio_init(BREAK_DETECTION_GPIO); // 断电续打检测IO初始化
		gpio_set_direction(BREAK_DETECTION_GPIO, GPIO_INPUT);
	}

	if (gpio_is_init(BREAK_CONTROL_GPIO) < 0)
	{
		gpio_init(BREAK_CONTROL_GPIO); // 断电续打充放电IO初始化
		gpio_set_direction(BREAK_CONTROL_GPIO, GPIO_OUTPUT);
		gpio_set_value(BREAK_CONTROL_GPIO, GPIO_LOW);
	}

	if (gpio_is_init(CC_EN_GPIO) < 0)
	{
		gpio_init(CC_EN_GPIO); // 断电续打充放电IO初始化
		gpio_set_direction(CC_EN_GPIO, GPIO_OUTPUT);
	}

	if (gcode_cmds_str != "")
	{
		m_gcode_cmds_before = split(gcode_cmds_str, ",");
	}
	gcode_cmds_str = Printer::GetInstance()->m_pconfig->GetString(section_name, "gcode_cmds_after", "");
	if (gcode_cmds_str != "")
	{
		m_gcode_cmds_after = split(gcode_cmds_str, ",");
	}
	// register gcodes
	std::string cmd_BREAK_SAVE_STATUS_help = "Break save run status";
	Printer::GetInstance()->m_gcode->register_command("BREAK_SAVE_STATUS", std::bind(&BreakSave::cmd_BREAK_SAVE_STATUS, this, std::placeholders::_1), false, cmd_BREAK_SAVE_STATUS_help);

	std::cout << "BreakSave m_break_save_path:  " << m_break_save_path << std::endl;
	if (!Printer::GetInstance()->m_pconfig->IsExistSection("save_variables")) // SaveVariables  保存不经常改变的量
	{
		Printer::GetInstance()->m_pconfig->SetValue("save_variables", "filename", m_break_save_path + "variables.cfg");
		Printer::GetInstance()->load_object("save_variables");
	}
	m_need_write = false;
	int ret = pthread_mutex_init(&lock, NULL);
	if (ret)
	{
		report_errno("BreakSave init", ret);
	}
	ret = pthread_cond_init(&cond, NULL);
}

BreakSave::~BreakSave() // SaveVariables
{
}
void BreakSave::handle_connect()
{
	// load_variables();
}
double GetDouble(const string &value, double def)
{
	if (value.empty())
	{
		return def;
	}
	double res;
	try
	{
		istringstream is(value);
		is >> res;
	}
	catch (...)
	{
		res = def;
	}
	return res;
}

void BreakSave::clear_variables()
{
	m_generic_Variables = std::map<std::string, std::string>();
	m_generic_Variables = std::map<std::string, std::string>();
	m_idle_timeout_info_Variables = std::map<std::string, std::string>();
	m_virtual_sd_stats_Variables = std::map<std::string, std::string>();
	m_z_homing_stats_Variables = std::map<std::string, std::string>();
	m_PauseResume_status_Variables = std::map<std::string, std::string>();
	m_printstats_Variables = std::map<std::string, std::string>();
	m_tool_head_status_Variables = std::map<std::string, std::string>();
	m_gcode_status_Variables = std::map<std::string, std::string>();
	m_stepper_status_Variables = std::map<std::string, std::string>();
	m_printer_fan_status_Variables = std::map<std::string, std::string>();
	m_controller_fans_status_Variables = std::vector<std::map<std::string, std::string>>();
	m_generic_fans_status_Variables = std::vector<std::map<std::string, std::string>>();
	m_heater_fans_status_Variables = std::vector<std::map<std::string, std::string>>();
	m_heaters_status_Variables = std::vector<std::map<std::string, std::string>>();
}
void BreakSave::delete_save_files()
{
	for (int i = 0; i < m_break_save_files_num; i++) //
	{
		string path = m_break_save_path + "break_save" + to_string(i) + ".gcode";
		char cmd[100];
		sprintf(cmd, "rm -f '%s'", path.c_str());
		system(cmd);
	}
	m_break_save_flag = false;
}

std::string BreakSave::get_save_file_name()
{
	std::string file_name = "";
	for (int i = 0; i < m_break_save_files_num; i++) // BREAK_SAVE_PATH
	{
		string path = m_break_save_path + "break_save" + to_string(i) + ".gcode";
		ConfigParser m_pconfig(path, false);
		if (!m_pconfig.ReadIni())
		{
			continue; // return -1;
		}
		if (m_pconfig.IsExistSection("virtual_sd_stats"))
		{
			if ("" != m_pconfig.GetString("virtual_sd_stats", "file_path", ""))
			{
				file_name = m_pconfig.GetString("virtual_sd_stats", "file_path", "");
				return file_name;
			}
		}
	}
	return file_name;
}
int BreakSave::select_load_variables()
{
	int break_save_num = -1;
	double max_eventtime = 0.0;
	std::string file_name = get_save_file_name();
	if (file_name == "")
	{
		m_break_save_flag = false;
		return -1;
	}
	for (int i = 0; i < m_break_save_files_num; i++) //
	{
		string path = m_break_save_path + "break_save" + to_string(i) + ".gcode";
		ConfigParser m_pconfig(path, false);
		if (!m_pconfig.ReadIni())
		{
			continue; // return -1;
		}
		if (!m_pconfig.IsExistSection("virtual_sd_stats")) // 不存在
		{
			continue;
		}
		if (file_name != m_pconfig.GetString("virtual_sd_stats", "file_path", ""))
		{
			continue;
		}
		if (m_pconfig.IsExistSection("generic"))
		{
			double eventtime = m_pconfig.GetDouble("generic", "get_monotonic", 0.0, 0.0);
			if (eventtime > max_eventtime)
			{
				max_eventtime = eventtime;
				break_save_num = i;
			}
		}
	}
	m_break_save_flag = true;
	return break_save_num;
}
void BreakSave::load_variables()
{
	string path = m_break_save_path + "break_save" + to_string(select_load_variables()) + ".gcode";
	std::cout << "load_variables from  " << path << std::endl;
	ConfigParser m_pconfig(path, false);
	m_break_save_num = 0;
	clear_variables();
	if (!m_pconfig.ReadIni())
	{
		return;
	}
	if (m_pconfig.IsExistSection("generic"))
	{
		std::vector<string> options = m_pconfig.get_all_options("generic");
		for (auto option : options)
		{
			m_generic_Variables[option] = m_pconfig.GetString("generic", option, " ");
		}
		if (m_pconfig.GetString("generic", "save_reason", " ") == "work_handler")
		{
			s_saved_print_para.power_off_save_breakpoint = 1;
		}
		else
		{
			s_saved_print_para.power_off_save_breakpoint = 0;
		}
		m_break_save_num = m_pconfig.GetInt("generic", "break_save_num", 0, 0);
	}
	if (m_pconfig.IsExistSection("idle_timeout_info"))
	{
		std::vector<string> options = m_pconfig.get_all_options("idle_timeout_info");
		for (auto option : options)
		{
			m_idle_timeout_info_Variables[option] = m_pconfig.GetString("idle_timeout_info", option, " ");
		}
	}
	if (m_pconfig.IsExistSection("virtual_sd_stats"))
	{
		std::vector<string> options = m_pconfig.get_all_options("virtual_sd_stats");
		for (auto option : options)
		{
			m_virtual_sd_stats_Variables[option] = m_pconfig.GetString("virtual_sd_stats", option, " ");
			std::cout << "option :" << option << "value :" << m_virtual_sd_stats_Variables[option] << std::endl;
		}
		s_saved_print_para.offset_size = m_pconfig.GetInt("virtual_sd_stats", "file_position", 0, 0);
		if ("" != m_pconfig.GetString("virtual_sd_stats", "tlp_test_path", ""))
		{
			s_saved_print_para.tlp_test_path = m_pconfig.GetString("virtual_sd_stats", "tlp_test_path", "");
		}
		// s_saved_print_para.current_layer = m_pconfig.GetInt("virtual_sd_stats", "current_layer", 0, 0);
		// s_saved_print_para.alread_print_time = m_pconfig.GetDouble("virtual_sd_stats", "alread_print_time", 0.0, 0.0);
	}
	if (m_pconfig.IsExistSection("PauseResume_status"))
	{
		std::vector<string> options = m_pconfig.get_all_options("PauseResume_status");
		for (auto option : options)
		{
			m_PauseResume_status_Variables[option] = m_pconfig.GetString("PauseResume_status", option, " ");
		}
	}
	if (m_pconfig.IsExistSection("printstats"))
	{
		std::vector<string> options = m_pconfig.get_all_options("printstats");
		for (auto option : options)
		{
			m_printstats_Variables[option] = m_pconfig.GetString("printstats", option, " ");
		}
		s_saved_print_para.curr_layer_num = m_pconfig.GetInt("printstats", "current_layer", 0, 0);
		s_saved_print_para.total_layer_num = m_pconfig.GetInt("printstats", "total_layers", 0, 0);
		s_saved_print_para.last_print_time = m_pconfig.GetDouble("printstats", "print_duration", 0);
		s_saved_print_para.filament_used = m_pconfig.GetDouble("printstats", "filament_used", 0);
	}
	if (m_pconfig.IsExistSection("tool_head_status"))
	{
		std::vector<string> options = m_pconfig.get_all_options("tool_head_status");
		for (auto option : options)
		{
			m_tool_head_status_Variables[option] = m_pconfig.GetString("tool_head_status", option, " ");
		}
	}
	if (m_pconfig.IsExistSection("gcode_status"))
	{
		std::vector<string> options = m_pconfig.get_all_options("gcode_status");
		for (auto option : options)
		{
			m_gcode_status_Variables[option] = m_pconfig.GetString("gcode_status", option, " ");
		}
		s_saved_print_para.X = m_pconfig.GetDouble("gcode_status", "base_position_x", 0);
		s_saved_print_para.Y = m_pconfig.GetDouble("gcode_status", "base_position_y", 0);
		s_saved_print_para.Z = m_pconfig.GetDouble("gcode_status", "base_position_z", 0);
		s_saved_print_para.E[0] = m_pconfig.GetDouble("gcode_status", "base_position_e", 0);
		s_saved_print_para.last_position_x = m_pconfig.GetDouble("gcode_status", "last_position_x", 0);
		s_saved_print_para.last_position_y = m_pconfig.GetDouble("gcode_status", "last_position_y", 0);
		s_saved_print_para.last_position_z = m_pconfig.GetDouble("gcode_status", "last_position_z", 0);
		s_saved_print_para.last_position_e = m_pconfig.GetDouble("gcode_status", "last_position_e", 0);
		s_saved_print_para.homing_position_x = m_pconfig.GetDouble("gcode_status", "homing_position_x", 0);
		s_saved_print_para.homing_position_y = m_pconfig.GetDouble("gcode_status", "homing_position_y", 0);
		s_saved_print_para.homing_position_z = m_pconfig.GetDouble("gcode_status", "homing_position_z", 0);
		s_saved_print_para.homing_position_e = m_pconfig.GetDouble("gcode_status", "homing_position_e", 0);
		s_saved_print_para.speed = m_pconfig.GetDouble("gcode_status", "speed", 0);
		s_saved_print_para.speed_factor = m_pconfig.GetDouble("gcode_status", "speed_factor", 0);
		s_saved_print_para.absolute_coord = m_pconfig.GetBool("gcode_status", "absolute_coord", true);
		s_saved_print_para.absolute_extrude = m_pconfig.GetBool("gcode_status", "absolute_extrude", true);
		s_saved_print_para.save_x_stop = m_pconfig.GetDouble("gcode_status", "base_position_x_stop", s_saved_print_para.X, 0);
		s_saved_print_para.save_y_stop = m_pconfig.GetDouble("gcode_status", "base_position_y_stop", s_saved_print_para.Y, 0);
		s_saved_print_para.save_z_stop = m_pconfig.GetDouble("gcode_status", "base_position_z_stop", s_saved_print_para.Z, 0);
	}
	if (m_pconfig.IsExistSection("stepper_status"))
	{
		s_saved_print_para.m_limits_x.push_back(m_pconfig.GetDouble("stepper_status", "m_limits_min_x", 0));
		s_saved_print_para.m_limits_x.push_back(m_pconfig.GetDouble("stepper_status", "m_limits_max_x", 0));
		s_saved_print_para.m_limits_y.push_back(m_pconfig.GetDouble("stepper_status", "m_limits_min_y", 0));
		s_saved_print_para.m_limits_y.push_back(m_pconfig.GetDouble("stepper_status", "m_limits_max_y", 0));
		s_saved_print_para.m_limits_z.push_back(m_pconfig.GetDouble("stepper_status", "m_limits_min_z", 0));
		s_saved_print_para.m_limits_z.push_back(m_pconfig.GetDouble("stepper_status", "m_limits_max_z", 0));
	}
	if (m_pconfig.IsExistSection("printer_fan_status"))
	{
		if (Printer::GetInstance()->m_printer_fan != nullptr)
		{
		}

		std::vector<string> options = m_pconfig.get_all_options("printer_fan_status");
		for (auto option : options)
		{
			m_printer_fan_status_Variables[option] = m_pconfig.GetString("printer_fan_status", option, " ");
		}
		s_saved_print_para.m_printer_fan_speed = 255 * m_pconfig.GetDouble("printer_fan_status", "speed", 1, 0, 1);
	}
	if (m_pconfig.IsExistSection("model_helper_fan_status"))
	{
		if (Printer::GetInstance()->m_printer_fans[1] != nullptr)
		{
		}

		std::vector<string> options = m_pconfig.get_all_options("model_helper_fan_status");
		for (auto option : options)
		{
			m_model_helper_fan_status_Variables[option] = m_pconfig.GetString("model_helper_fan_status", option, " ");
		}
		s_saved_print_para.m_model_helper_fan_speed = 255 * m_pconfig.GetDouble("model_helper_fan_status", "speed", 1, 0, 1);
	}
	if (m_pconfig.IsExistSection("box_fan_status"))
	{
		if (Printer::GetInstance()->m_printer_fans[2] != nullptr)
		{
		}

		std::vector<string> options = m_pconfig.get_all_options("box_fan_status");
		for (auto option : options)
		{
			m_box_fan_status_Variables[option] = m_pconfig.GetString("box_fan_status", option, " ");
		}
		s_saved_print_para.m_box_fan_speed = 255 * m_pconfig.GetDouble("box_fan_status", "speed", 1, 0, 1);
	}
	if (m_pconfig.IsExistSection("z_homing_stats"))
	{
		std::vector<string> options = m_pconfig.get_all_options("z_homing_stats");
		for (auto option : options)
		{
			m_z_homing_stats_Variables[option] = m_pconfig.GetString("z_homing_stats", option, " ");
		}
		s_saved_print_para.is_z_homing = m_pconfig.GetBool("z_homing_stats", "is_z_homing", false);
	}

	// for (auto controller_fan : Printer::GetInstance()->m_controller_fans) // 主板风扇 controller_fan    自动给主板降温
	// {
	// 	if (controller_fan != nullptr)
	// 	{
	// 		std::string controller_fan_name = controller_fan->m_section_name;
	// 		if (m_pconfig.IsExistSection(controller_fan_name))
	// 		{
	// 			std::map<std::string, std::string> m_controller_fan_status_Variables;
	// 			std::vector<string> options = m_pconfig.get_all_options(controller_fan_name);
	// 			for (auto option : options)
	// 			{
	// 				m_controller_fan_status_Variables[option] = m_pconfig.GetString(controller_fan_name, option, " ");
	// 			}
	// 			m_controller_fans_status_Variables.push_back(m_controller_fan_status_Variables);
	// 		}
	// 	}
	// }
	// for (auto generic_fan : Printer::GetInstance()->m_generic_fans) // 通用风扇 fan_generic
	// {
	// 	if (generic_fan != nullptr)
	// 	{
	// 		std::string generic_fan_name = "fan_generic " + generic_fan->m_fan_name;
	// 		if (m_pconfig.IsExistSection(generic_fan_name))
	// 		{
	// 			std::map<std::string, std::string> m_generic_fan_status_Variables;
	// 			std::vector<string> options = m_pconfig.get_all_options(generic_fan_name);
	// 			for (auto option : options)
	// 			{
	// 				m_generic_fan_status_Variables[option] = m_pconfig.GetString(generic_fan_name, option, " ");
	// 			}
	// 			m_generic_fans_status_Variables.push_back(m_generic_fan_status_Variables);
	// 		}
	// 	}
	// }
	// for (auto heater_fan : Printer::GetInstance()->m_heater_fans) // 挤出头喉管风扇 heater_fan  自动根据温度给喉管降温
	// {
	// 	if (heater_fan != nullptr)
	// 	{
	// 		std::string heater_fan_name = "heater_fan " + heater_fan->m_heater_name;
	// 		if (m_pconfig.IsExistSection(heater_fan_name))
	// 		{
	// 			std::map<std::string, std::string> m_heater_fan_status_Variables;
	// 			std::vector<string> options = m_pconfig.get_all_options(heater_fan_name);
	// 			for (auto option : options)
	// 			{
	// 				m_heater_fan_status_Variables[option] = m_pconfig.GetString(heater_fan_name, option, " ");
	// 			}
	// 			m_heater_fans_status_Variables.push_back(m_heater_fan_status_Variables);
	// 		}
	// 	}
	// }
	{
		struct Available_status heaters_status = Printer::GetInstance()->m_pheaters->get_status(0);
		for (auto heater_section_name : heaters_status.available_heaters)
		{
			if (m_pconfig.IsExistSection(heater_section_name))
			{
				std::map<std::string, std::string> m_heater_status_Variables;
				std::vector<string> options = m_pconfig.get_all_options(heater_section_name);
				for (auto option : options)
				{
					m_heater_status_Variables[option] = m_pconfig.GetString(heater_section_name, option, " ");
				}
				m_heaters_status_Variables.push_back(m_heater_status_Variables);
			}
		}
		s_saved_print_para.bed_temp = m_pconfig.GetDouble("heater_bed", "target_temp", 0, 0);
		s_saved_print_para.end_temp[0] = m_pconfig.GetDouble("extruder", "target_temp", 0, 0);
	}
}

void BreakSave::cmd_BREAK_SAVE_STATUS(GCodeCommand &gcmd)
{
	if (m_break_save_enable)
	{
		// save_current_run_status(); //
	}
}

void BreakSave::save_generic_status(double eventtime) //
{
	{
		m_generic_Variables["get_monotonic"] = to_string(eventtime);
		// m_generic_Variables["save_reason"] = (m_save_reason);
		m_generic_Variables["break_save_num"] = to_string(m_break_save_num);
	}
	// for (auto variable : m_generic_Variables)
	// {
	//     m_pconfig.SetValue("generic", variable.first, variable.second);
	// }
}
void BreakSave::save_idle_timeout_info_status(double eventtime) //
{
	{
		idle_timeout_stats_t idle_timeout_info = Printer::GetInstance()->m_idle_timeout->get_status(eventtime);
		m_idle_timeout_info_Variables["state"] = (idle_timeout_info.state);
		m_idle_timeout_info_Variables["last_print_start_systime"] = to_string(idle_timeout_info.last_print_start_systime);
	}
	// for (auto variable : m_idle_timeout_info_Variables)
	// {
	//     m_pconfig.SetValue("idle_timeout_info", variable.first, variable.second);
	// }
}
void BreakSave::save_virtual_sd_stats_status(double eventtime) //
{
	{
		// double alread_print_time = 0;
		// int current_layer = 0;
		// ui_cb[get_current_layer_cb](&current_layer);
		// ui_cb[get_alread_print_time_cb](&alread_print_time);
		virtual_sd_stats_t virtual_sd_stats = Printer::GetInstance()->m_virtual_sdcard->get_status(eventtime);
		m_virtual_sd_stats_Variables["file_path"] = (virtual_sd_stats.file_path);
		m_virtual_sd_stats_Variables["progress"] = to_string(virtual_sd_stats.progress);
		m_virtual_sd_stats_Variables["is_active"] = to_string(virtual_sd_stats.is_active);
		m_virtual_sd_stats_Variables["file_position"] = to_string(virtual_sd_stats.file_position); // offset_size
		m_virtual_sd_stats_Variables["file_size"] = to_string(virtual_sd_stats.file_size);
		if (strcmp(aic_print_info.tlp_test_path, "") != 0)
		{
			m_virtual_sd_stats_Variables["tlp_test_path"] = aic_print_info.tlp_test_path;
		}
		// m_virtual_sd_stats_Variables["current_layer"] = to_string(current_layer);
		// m_virtual_sd_stats_Variables["alread_print_time"] = to_string(alread_print_time);
	}
	// for (auto variable : m_virtual_sd_stats_Variables)
	// {
	//     m_pconfig.SetValue("virtual_sd_stats", variable.first, variable.second);
	// }
}
void BreakSave::save_z_homing_stats_status(double eventtime) //
{
	bool is_z_homing = false;
	m_z_homing_stats_Variables["is_z_homing"] = to_string(is_z_homing);

}
void BreakSave::save_PauseResume_status(double eventtime) //
{
	{
		PauseResume_status_t PauseResume_status = Printer::GetInstance()->m_pause_resume->get_status();
		m_PauseResume_status_Variables["is_paused"] = to_string(PauseResume_status.is_paused);
	}
	// for (auto variable : m_PauseResume_status_Variables)
	// {
	//     m_pconfig.SetValue("PauseResume_status", variable.first, variable.second);
	// }
}
void BreakSave::save_printstats_status(double eventtime) //
{
	{
		print_stats_t printstats = Printer::GetInstance()->m_print_stats->get_status(eventtime, NULL);
		m_printstats_Variables["filename"] = (printstats.filename);
		m_printstats_Variables["total_duration"] = to_string(printstats.total_duration);
		m_printstats_Variables["print_duration"] = to_string(printstats.print_duration);
		m_printstats_Variables["filament_used"] = to_string(printstats.filament_used);
		m_printstats_Variables["current_layer"] = to_string(printstats.current_layer);
		m_printstats_Variables["total_layers"] = to_string(printstats.total_layers);
		// m_printstats_Variables["state"] = (printstats.state);
		// m_printstats_Variables["message"] = (printstats.message);
	}
	// for (auto variable : m_printstats_Variables)
	// {
	//     m_pconfig.SetValue("printstats", variable.first, variable.second);
	// }
}

void BreakSave::save_tool_head_status(double eventtime) //
{
	{
		std::map<std::string, std::string> tool_head_status = Printer::GetInstance()->m_tool_head->get_status(eventtime);
		m_tool_head_status_Variables = tool_head_status;
	}
	// for (auto variable : m_tool_head_status_Variables)
	// {
	//     m_pconfig.SetValue("tool_head_status", variable.first, variable.second);
	// }
}
void BreakSave::save_gcode_status(double eventtime) //
{
	{
		gcode_move_state_t gcode_status;
		gcode_move_state_t pause_gcode_status;
		print_stats_t printstats = Printer::GetInstance()->m_print_stats->get_status(eventtime, NULL);
		gcode_status = Printer::GetInstance()->m_gcode_move->get_status(eventtime);

		m_gcode_status_Variables["absolute_coord"] = to_string(gcode_status.absolute_coord);	 // absolute_coord
		m_gcode_status_Variables["absolute_extrude"] = to_string(gcode_status.absolute_extrude); // absolute_extrude
		m_gcode_status_Variables["speed"] = to_string(gcode_status.speed);						 // speed
		m_gcode_status_Variables["speed_factor"] = to_string(gcode_status.speed_factor);		 // speed_factor
		m_gcode_status_Variables["extrude_factor"] = to_string(gcode_status.extrude_factor);
		m_gcode_status_Variables["last_position_x"] = to_string(gcode_status.last_position[0]) ;
		m_gcode_status_Variables["last_position_y"] = to_string(gcode_status.last_position[1]) ;
		m_gcode_status_Variables["last_position_z"] = to_string(gcode_status.last_position[2]) ;
		m_gcode_status_Variables["last_position_e"] = to_string(gcode_status.last_position[3]) ;
		m_gcode_status_Variables["homing_position_x"] = to_string(gcode_status.homing_position[0]) ;
		m_gcode_status_Variables["homing_position_y"] = to_string(gcode_status.homing_position[1]) ;
		m_gcode_status_Variables["homing_position_z"] = to_string(gcode_status.homing_position[2]) ;
		m_gcode_status_Variables["homing_position_e"] = to_string(gcode_status.homing_position[3]) ;
		if (printstats.state == PRINT_STATS_STATE_PAUSEING || printstats.state == PRINT_STATS_STATE_PAUSED)
		{
			pause_gcode_status = Printer::GetInstance()->m_gcode_move->get_gcode_state("PAUSE_STATE");
			if (!pause_gcode_status.last_position.empty())
			{
				m_gcode_status_Variables["base_position_x"] = to_string(pause_gcode_status.last_position[0]); // x
				m_gcode_status_Variables["base_position_y"] = to_string(pause_gcode_status.last_position[1]); //
				m_gcode_status_Variables["base_position_z"] = to_string(pause_gcode_status.last_position[2]); //
				m_gcode_status_Variables["base_position_e"] = to_string(pause_gcode_status.last_position[3]); // E
			}
			else
			{
				m_gcode_status_Variables["base_position_x"] = to_string(gcode_status.base_position[0]); // x
				m_gcode_status_Variables["base_position_y"] = to_string(gcode_status.base_position[1]); //
				m_gcode_status_Variables["base_position_z"] = to_string(gcode_status.base_position[2]); //
				m_gcode_status_Variables["base_position_e"] = to_string(gcode_status.base_position[3]); // E
			}
			
		}
		else
		{
			m_gcode_status_Variables["base_position_x"] = to_string(gcode_status.base_position[0]); // x
			m_gcode_status_Variables["base_position_y"] = to_string(gcode_status.base_position[1]); //
			m_gcode_status_Variables["base_position_z"] = to_string(gcode_status.base_position[2]); //
			m_gcode_status_Variables["base_position_e"] = to_string(gcode_status.base_position[3]); // E
		}
		// if (Printer::GetInstance()->m_bed_mesh != nullptr)
		// {
		// 	m_gcode_status_Variables["base_position_z"] = to_string(Printer::GetInstance()->m_bed_mesh->m_last_position[2]);
		// }
		// std::cout << "bed_mesh : " << Printer::GetInstance()->m_bed_mesh->m_last_position[0] << " " << Printer::GetInstance()->m_bed_mesh->m_last_position[1] << " " << Printer::GetInstance()->m_bed_mesh->m_last_position[2] << std::endl;
		// std::cout << "base_position : " << gcode_status.base_position[0] << " " << gcode_status.base_position[1] << " " << gcode_status.base_position[2] << std::endl;
		// std::cout << "last_position : " << gcode_status.last_position[0] << " " << gcode_status.last_position[1] << " " << gcode_status.last_position[2] << std::endl;
		// std::cout << "command_position : " << Printer::GetInstance()->m_tool_head->m_commanded_pos[0] << " " << Printer::GetInstance()->m_tool_head->m_commanded_pos[1] << " " << Printer::GetInstance()->m_tool_head->m_commanded_pos[2] << std::endl;
	}
	// for (auto variable : m_gcode_status_Variables)
	// {
	//     m_pconfig.SetValue("gcode_status", variable.first, variable.second);
	// }
}
void BreakSave::save_stepper_status(double eventtime) //
{
	{
		gcode_move_state_t gcode_status;
		gcode_status = Printer::GetInstance()->m_gcode_move->get_status(eventtime);

		m_stepper_status_Variables["m_limits_min_x"] = to_string(Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->get_range()[0]);
		m_stepper_status_Variables["m_limits_max_x"] = to_string(Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->get_range()[1]);
		m_stepper_status_Variables["m_limits_min_y"] = to_string(Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->get_range()[0]);
		m_stepper_status_Variables["m_limits_max_y"] = to_string(Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->get_range()[1]);
		m_stepper_status_Variables["m_limits_min_z"] = to_string(Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_range()[0]);
		m_stepper_status_Variables["m_limits_max_z"] = to_string(Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_range()[1]);
	}
	// for (auto variable : m_gcode_status_Variables)
	// {
	//     m_pconfig.SetValue("gcode_status", variable.first, variable.second);
	// }
}
void BreakSave::save_printer_fan_status(double eventtime) //
{
	if (Printer::GetInstance()->m_printer_fan != nullptr)
	{
		struct fan_state printer_fan_state = Printer::GetInstance()->m_printer_fan->get_status(eventtime); // fan 模型散热风扇 挤出头温度超过45度 就开始加热  M106 M107控制
		double startup_voltage = Printer::GetInstance()->m_printer_fan->m_fan->get_startup_voltage();
		double speed = (printer_fan_state.speed - startup_voltage) / (1 - startup_voltage);
		if (speed < 1e-9)
			speed = 0;
		m_printer_fan_status_Variables["speed"] = to_string(speed);					   // m_printer_fan_speed
		m_printer_fan_status_Variables["rpm"] = to_string(printer_fan_state.rpm);
	}

	if (Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN] != nullptr)
	{
		struct fan_state model_helper_fan_state = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->get_status(eventtime);
		double startup_voltage = Printer::GetInstance()->m_printer_fans[MODEL_HELPER_FAN]->m_fan->get_startup_voltage();
		double speed = (model_helper_fan_state.speed - startup_voltage) / (1 - startup_voltage);
		if (speed < 1e-9)
			speed = 0;
		m_model_helper_fan_status_Variables["speed"] = to_string(speed); // m_model_helper_fan_speed
		// m_model_helper_fan_status_Variables["rpm"] = to_string(model_helper_fan_state.rpm); 
	}

	if (Printer::GetInstance()->m_printer_fans[BOX_FAN] != nullptr)
	{
		struct fan_state box_fan_state = Printer::GetInstance()->m_printer_fans[BOX_FAN]->get_status(eventtime);
		m_box_fan_status_Variables["speed"] = to_string(box_fan_state.speed); // m_box_fan_speed
		// m_box_fan_status_Variables["rpm"] = to_string(box_fan_state.rpm); 
	}
	
}
void BreakSave::save_controller_fan_status(double eventtime) //
{
	// for (auto controller_fan : Printer::GetInstance()->m_controller_fans) // 通用风扇 fan_generic
	// {
	// 	if (controller_fan != nullptr)
	// 	{
	// 		struct fan_state controller_fan_state = controller_fan->get_status(eventtime);
	// 		std::map<std::string, std::string> m_controller_fan_status_Variables;
	// 		m_controller_fan_status_Variables["name"] = (controller_fan->m_section_name);
	// 		m_controller_fan_status_Variables["speed"] = to_string(controller_fan_state.speed);
	// 		m_controller_fan_status_Variables["rpm"] = to_string(controller_fan_state.rpm);
	// 		m_controller_fans_status_Variables.push_back(m_controller_fan_status_Variables);
	// 		// for (auto variable : m_generic_fan_status_Variables)
	// 		// {
	// 		// 	m_pconfig.SetValue(controller_fan->m_section_name, variable.first, variable.second);
	// 		// }
	// 	}
	// }
}

// void BreakSave::save_generic_fans_status(double eventtime) //
// {
// 	for (auto generic_fan : Printer::GetInstance()->m_generic_fans) // 通用风扇 fan_generic
// 	{
// 		if (generic_fan != nullptr)
// 		{
// 			std::string generic_fan_name = "fan_generic " + generic_fan->m_fan_name;
// 			struct fan_state generic_fan_state = generic_fan->get_status(eventtime);
// 			std::map<std::string, std::string> m_generic_fan_status_Variables;
// 			m_generic_fan_status_Variables["name"] = (generic_fan_name);
// 			m_generic_fan_status_Variables["speed"] = to_string(generic_fan_state.speed);
// 			m_generic_fan_status_Variables["rpm"] = to_string(generic_fan_state.rpm);
// 			m_generic_fans_status_Variables.push_back(m_generic_fan_status_Variables);
// 			// for (auto variable : m_generic_fan_status_Variables)
// 			// {
// 			// 	m_pconfig.SetValue(generic_fan_name, variable.first, variable.second);
// 			// }
// 		}
// 	}
// }
// void BreakSave::save_heater_fans_status(double eventtime) //
// {
// 	int i = 0;
// 	for (auto heater_fan : Printer::GetInstance()->m_heater_fans) // 喉管风扇  heater_fan
// 	{
// 		if (heater_fan != nullptr)
// 		{
// 			std::string heater_fan_name = "heater_fan " + heater_fan->m_heater_name;
// 			i++;
// 			struct fan_state heater_fan_state = heater_fan->get_status(eventtime);
// 			std::map<std::string, std::string> m_heater_fan_status_Variables;
// 			m_heater_fan_status_Variables["name"] = (heater_fan_name);
// 			m_heater_fan_status_Variables["speed"] = to_string(heater_fan_state.speed);
// 			m_heater_fan_status_Variables["rpm"] = to_string(heater_fan_state.rpm);
// 			m_heater_fans_status_Variables.push_back(m_heater_fan_status_Variables);
// 			// for (auto variable : m_heater_fan_status_Variables)
// 			// {
// 			// 	m_pconfig.SetValue(heater_fan_name, variable.first, variable.second);
// 			// }
// 		}
// 	}
// }
void BreakSave::save_heaters_status(double eventtime) //
{
	{ // 保存加热
		struct Available_status heaters_status = Printer::GetInstance()->m_pheaters->get_status(eventtime);
		for (auto heater_section_name : heaters_status.available_heaters)
		{
			std::string heater_name = split(heater_section_name, " ").front();
			Heater *heater = Printer::GetInstance()->m_pheaters->lookup_heater(heater_name);
			struct temp_state heater_state = heater->get_status(eventtime);
			std::map<std::string, std::string> m_heater_status_Variables;
			m_heater_status_Variables["name"] = (heater_section_name);
			// m_heater_status_Variables["smoothed_temp"] = to_string(heater_state.smoothed_temp) ;
			if (heater_section_name == "extruder" && Printer::GetInstance()->m_pause_resume->get_save_extruder_temp()  > 1e-15) //暂停时喷头温度读取暂停保存参数
			{
				m_heater_status_Variables["target_temp"] = to_string(Printer::GetInstance()->m_pause_resume->get_save_extruder_temp()); // bed_temp end_temp
			}
			else
			{
				m_heater_status_Variables["target_temp"] = to_string(heater_state.target_temp); // bed_temp end_temp
			}
			// m_heater_status_Variables["last_pwm_value"] = to_string(heater_state.last_pwm_value) ;
			m_heaters_status_Variables.push_back(m_heater_status_Variables);
			// for (auto variable : m_heater_status_Variables)
			// {
			// 	m_pconfig.SetValue(heater_section_name, variable.first, variable.second);
			// }
		}
	}
}

void BreakSave::save_to_pconfig(ConfigParser &m_pconfig) //
{
	for (auto variable : m_generic_Variables)
	{
		m_pconfig.SetValue("generic", variable.first, variable.second);
	}
	for (auto variable : m_idle_timeout_info_Variables)
	{
		m_pconfig.SetValue("idle_timeout_info", variable.first, variable.second);
	}
	for (auto variable : m_virtual_sd_stats_Variables)
	{
		m_pconfig.SetValue("virtual_sd_stats", variable.first, variable.second);
	}
	for (auto variable : m_PauseResume_status_Variables)
	{
		m_pconfig.SetValue("PauseResume_status", variable.first, variable.second);
	}
	for (auto variable : m_printstats_Variables)
	{
		m_pconfig.SetValue("printstats", variable.first, variable.second);
	}
	for (auto variable : m_tool_head_status_Variables)
	{
		m_pconfig.SetValue("tool_head_status", variable.first, variable.second);
	}
	for (auto variable : m_stepper_status_Variables)
	{
		m_pconfig.SetValue("stepper_status", variable.first, variable.second);
	}
	for (auto variable : m_gcode_status_Variables)
	{
		m_pconfig.SetValue("gcode_status", variable.first, variable.second);
	}
	for (auto variable : m_printer_fan_status_Variables)
	{
		m_pconfig.SetValue("printer_fan_status", variable.first, variable.second);
	}
	for (auto variable : m_model_helper_fan_status_Variables)
	{
		m_pconfig.SetValue("model_helper_fan_status", variable.first, variable.second);
	}
	for (auto variable : m_box_fan_status_Variables)
	{
		m_pconfig.SetValue("box_fan_status", variable.first, variable.second);
	}

	for (auto m_controller_fan_status_Variables : m_controller_fans_status_Variables)
	{
		for (auto variable : m_controller_fan_status_Variables)
		{
			m_pconfig.SetValue(m_controller_fan_status_Variables["name"], variable.first, variable.second);
		}
	}
	for (auto m_generic_fan_status_Variables : m_generic_fans_status_Variables)
	{
		for (auto variable : m_generic_fan_status_Variables)
		{
			m_pconfig.SetValue(m_generic_fan_status_Variables["name"], variable.first, variable.second);
		}
	}
	for (auto m_heater_fan_status_Variables : m_heater_fans_status_Variables)
	{
		for (auto variable : m_heater_fan_status_Variables)
		{
			m_pconfig.SetValue(m_heater_fan_status_Variables["name"], variable.first, variable.second);
		}
	}

	for (auto m_heater_status_Variables : m_heaters_status_Variables)
	{
		for (auto variable : m_heater_status_Variables)
		{
			m_pconfig.SetValue(m_heater_status_Variables["name"], variable.first, variable.second);
		}
	}

	for (auto variable : m_z_homing_stats_Variables)
	{
		m_pconfig.SetValue("z_homing_stats", variable.first, variable.second);
	}
}

void BreakSave::save_current_Z_status() // SDCARD_PRINT_FILE_BREAK_SAVE
{
	pthread_mutex_lock(&lock);

	double eventtime = get_monotonic();
	save_generic_status(eventtime);
	gcode_move_state_t gcode_status;
	gcode_status = Printer::GetInstance()->m_gcode_move->get_status(eventtime);

	double command_x = Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->get_steppers()[0]->get_commanded_position();
	double mcu_pos_x = Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->get_steppers()[0]->get_mcu_position();
	double mcu_pos_past_x = Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->get_steppers()[0]->get_past_mcu_position(Printer::GetInstance()->m_tool_head->m_mcu->m_clocksync->estimated_print_time(eventtime));
	double step_dist_x = Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->get_steppers()[0]->get_step_dist();
	// std::cout << "command_x: " << command_x << " mcu_pos_x: " << mcu_pos_x * step_dist_x << " mcu_pos_past_x: " << mcu_pos_past_x * step_dist_x << std::endl;

	double command_y = Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->get_steppers()[0]->get_commanded_position();
	double mcu_pos_y = Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->get_steppers()[0]->get_mcu_position();
	double mcu_pos_past_y = Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->get_steppers()[0]->get_past_mcu_position(Printer::GetInstance()->m_tool_head->m_mcu->m_clocksync->estimated_print_time(eventtime));
	double step_dist_y = Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->get_steppers()[0]->get_step_dist();
	// std::cout << "command_y: " << command_y << " mcu_pos_y: " << mcu_pos_y * step_dist_y << " mcu_pos_past_y: " << mcu_pos_past_y * step_dist_y << std::endl;

	double command_z = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_steppers()[0]->get_commanded_position();
	double mcu_pos_z = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_steppers()[0]->get_mcu_position();
	double mcu_pos_past_z = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_steppers()[0]->get_past_mcu_position(Printer::GetInstance()->m_tool_head->m_mcu->m_clocksync->estimated_print_time(eventtime));
	double step_dist_z = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->get_steppers()[0]->get_step_dist();
	// std::cout << "command_z: " << command_z << " mcu_pos_z: " << mcu_pos_z * step_dist_z << " mcu_pos_past_z: " << mcu_pos_past_z * step_dist_z << std::endl;



	// m_gcode_status_Variables["base_position_x_stop"] = to_string(command_x + (mcu_pos_x - mcu_pos_past_x) * step_dist_x); // save_x_stop
	// m_gcode_status_Variables["base_position_y_stop"] = to_string(command_y + (mcu_pos_y - mcu_pos_past_y) * step_dist_y); // save_y_stop
	// m_gcode_status_Variables["base_position_z_stop"] = to_string(command_z + (mcu_pos_z - mcu_pos_past_z) * step_dist_z); // save_z_stop
	m_gcode_status_Variables["base_position_x_stop"] = to_string(command_x); // save_x_stop
	m_gcode_status_Variables["base_position_y_stop"] = to_string(command_y); // save_y_stop
	m_gcode_status_Variables["base_position_z_stop"] = to_string(command_z); // save_z_stop
	m_need_write = true;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&lock);
}

void BreakSave::save_current_run_status(double eventtime) // SDCARD_PRINT_FILE_BREAK_SAVE
{
	if (m_break_save_enable)
	{
		clear_variables(); // 清除之前保存内容 尤其是 base_position_z_stop
		save_current_Z_status();

		pthread_mutex_lock(&lock);

		// double eventtime = get_monotonic();
		save_generic_status(eventtime);
		// save_idle_timeout_info_status(eventtime);
		save_virtual_sd_stats_status(eventtime);
		save_z_homing_stats_status(eventtime);
		// save_PauseResume_status(eventtime);
		save_printstats_status(eventtime);

		save_tool_head_status(eventtime);
		save_gcode_status(eventtime);
		save_stepper_status(eventtime);
		save_printer_fan_status(eventtime);
		// save_controller_fan_status(eventtime);
		// save_generic_fans_status(eventtime);

		// save_heater_fans_status(eventtime);
		save_heaters_status(eventtime);
		m_need_write = true;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&lock);
	}
}

void BreakSave::restore_run_status(Printer *p_printer)
{
}

bool BreakSave::Write_File() //
{
	printf("BreakSave Write_File  \n");
	double eventtime1 = get_monotonic();
	m_need_write = false;
	printf("BreakSave Write_File  m_need_write  %d\n", m_need_write);
	string path = m_break_save_path + "break_save" + to_string(m_break_save_name) + ".gcode";
	printf("BreakSave Write_File  path  %s\n", path.c_str());
	ConfigParser m_pconfig(path, false);
	save_to_pconfig(m_pconfig);
	pthread_mutex_unlock(&lock);
	double eventtime = get_monotonic();

	m_pconfig.SetValue("generic", "Write_File_monotonic", to_string(eventtime));
	if (!m_pconfig.WriteIni(path))
	{
		Printer::GetInstance()->m_gcode->respond_info("Unable to save variable");
		return false;
	}
	double eventtime2 = get_monotonic();
	system("sync");
	LOG_I("break_save write finish\n");
	usleep(10000);
	// std::cout << "Write_File    "  << "  save_num  " << m_pconfig.GetInt("generic","break_save_num",0,0) << "  wait  " << eventtime-eventtime1 << "  write  " << eventtime2-eventtime << "  to  " << path << std::endl;
	return true;
}

// bool break_save_Write_File()
// {
// 	static bool file_position_save_flag = false;
// 	if (Printer::GetInstance()->m_break_save != nullptr && Printer::GetInstance()->m_break_save->m_break_save_enable)
// 	{
// 		if (gpio_get_value(BREAK_DETECTION_GPIO) == BREAK_DETECTION_TRIGGER_LEVEL)
// 		{
// 			gpio_set_value(BREAK_CONTROL_GPIO, GPIO_HIGH);
// 			Printer::GetInstance()->set_break_save_state(true);
// 			bool m_print_busy = app_print_get_print_state();
// 			if (!file_position_save_flag && m_print_busy)
// 			{
// 				std::string cmd = "BREAK_SAVE_STATUS save_reason=cmd_PAUSE_end";
// 				Printer::GetInstance()->m_gcode->run_script(cmd);
// 				printf(">>>>>>>set_break_save_state  \n");
// 				file_position_save_flag = true;
// 			}
// 			Printer::GetInstance()->m_break_save->Write_File();
// 		}
// 	}
// 	// else
// 	// {
// 	usleep(1000);
// 	// }
// }

bool check_power_outages()
{
	if (gpio_get_value(BREAK_DETECTION_GPIO) == BREAK_DETECTION_TRIGGER_LEVEL)
	{
		LOG_I("check_power_outages\n");
		gpio_set_value(CC_EN_GPIO, GPIO_LOW);
		if (Printer::GetInstance()->m_break_save != nullptr && Printer::GetInstance()->m_break_save->m_break_save_enable)
		{
			return true;
		}
	}
	return false;
}

void *break_save_task(void *arg)
{
	if (gpio_is_init(CC_EN_GPIO) != 0)
	{
		gpio_init(CC_EN_GPIO); // 断电续打充放电IO初始化
		gpio_set_direction(CC_EN_GPIO, GPIO_OUTPUT);
	}
	gpio_set_value(CC_EN_GPIO, GPIO_HIGH);
	while (1)
	{
		if (check_power_outages())
		{
			gpio_set_value(CC_EN_GPIO, GPIO_LOW); /* stop power capacity charging */
			gpio_set_value(BREAK_CONTROL_GPIO, GPIO_HIGH); /* enable capacity power output */
            usleep(200000); /* 200 ms delay */
            if (!check_power_outages()) /* check again to filter interference signal */
            {
                LOG_W("[%s] power outages interference signal detected.\n", __FUNCTION__);
                gpio_set_value(BREAK_CONTROL_GPIO, GPIO_LOW); /* disable capacity power ouput */
                gpio_set_value(CC_EN_GPIO, GPIO_HIGH); /* start power capacity charging */
                continue;
            }

            LOG_W("[%s] power outages detected.\n", __FUNCTION__);
			// gpio_set_value(CC_EN_GPIO, GPIO_LOW); /* stop power capacity charging */
			if (!app_print_get_print_state() || Printer::GetInstance()->m_break_save->m_break_save_reserve) //非打印状态和断电续打预备状态不保存
			{
				gpio_set_value(BREAK_CONTROL_GPIO, GPIO_LOW);
				break;
			}
			double eventtime = get_monotonic();
			Printer::GetInstance()->set_break_save_state(true); // 关闭mcu 5v
			if (Printer::GetInstance()->m_pause_resume->is_sd_active()) // 停止读取gcode
			{
				Printer::GetInstance()->m_pause_resume->m_sd_paused = true;
				Printer::GetInstance()->m_pause_resume->m_v_sd->do_pause();
			}
			// std::string cmd = "BREAK_SAVE_STATUS save_reason=cmd_PAUSE_end";
			// Printer::GetInstance()->m_gcode->run_script(cmd);
			Printer::GetInstance()->m_break_save->save_current_run_status(eventtime);
			Printer::GetInstance()->m_break_save->Write_File();
			LOG_I("braek save finished\n");
			usleep(1000);
			gpio_set_value(BREAK_CONTROL_GPIO, GPIO_LOW); /* disable capacity power ouput */
			while(1){}
			break;
		}
		usleep(1000);
	}
}
