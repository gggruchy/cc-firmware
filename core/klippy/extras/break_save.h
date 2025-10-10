#ifndef BREAK_SAVE_H
#define BREAK_SAVE_H
#include <iostream>
#include <string>
#include <string.h>
#include <map>
#include <regex>

#include "gcode_move.h"

#define EXTRUDE_NUM 1
#define HOT_NUM 1
#define MAX_FILE_NAME 100

typedef struct
{
	int magic_number;
	char selected_file[MAX_FILE_NAME];
	uint8_t b_disable_sound;  // 旧版表示取消声音，新版相反，表示有声音的声音占空比
	uint8_t b_disable_sound1; // 旧版表示取消声音，新版相反，表示有声音的声音占空比
	uint8_t b_disable_sound2; // 旧版表示取消声音，新版相反，表示有声音的声音占空比
	uint8_t b_disable_sound3; // 旧版表示取消声音，新版相反，表示有声音的声音占空比

	short bed_temp[1];			 // 热床加热温度
	short end_temp[EXTRUDE_NUM]; // 挤出头加热温度
	short hot_temp[HOT_NUM];	 // 其他加热温度
	int lang_en;				 // 英语使能
	bool wifi_autorun_enable;
	short b_SD_select;
	uint8_t use_wifi_ETH_select; // WIFI还是ETH以太网，切换网络时要注意调用ETH_Deinit释放资源

	uint8_t tp_int_pin_value;
	uint8_t machine_fan_state0;
	uint8_t filament_enable_state;
	uint8_t switch_e_state;
	uint8_t Reserved31_3;

	short bed2_temp;
	char last_file_is_print_type; // 上一次打印的文件是打印文件
} my_UI_info_t;

typedef struct
{
	bool network_state; //
} machine_status_info_t;

typedef struct
{
	char current_extrude;
	char b_print_pause; // 打印暂停 界面暂停打印，用来告诉用户当前打印状态，SD文件打印状态用来控制实际电机动作，比如预览和加热时在打印工作但电机不转，具体还要分析代码
	char b_print_stop;	//-G-G-----------打印停止，专门针对串口屏界面打印状态禁止操作有关
	char b_bed_heat;
	char b_end_heat[EXTRUDE_NUM];
	char b_fan_on[EXTRUDE_NUM];
	int end_temp[EXTRUDE_NUM];

} my_temp_UI_info_t;

#define PRINT_PRAR_MAGIC_NUM 0x78933456
typedef struct
{

	char file_name[96]; // 为了让8字节对齐
	int magic_num;
	float X;
	float Y;
	float Z;
	float save_x_stop;
	float save_y_stop;
	float save_z_stop;
	float E[EXTRUDE_NUM];
	int offset_size;
	float m_printer_fan_speed;
	float m_model_helper_fan_speed;
	float m_box_fan_speed;
	double last_position_x;
	double last_position_y;
	double last_position_z;
	double last_position_e;
	double homing_position_x;
	double homing_position_y;
	double homing_position_z;
	double homing_position_e;

	float speed_factor;
	float speed;

	bool absolute_coord;
	bool absolute_extrude;

	short bed_temp;
	short end_temp[EXTRUDE_NUM];

	short fan_pwm_value[EXTRUDE_NUM];
	short orig_feedrate;
	short filament_ratio;
	int power_off_save_breakpoint; // 断电保存断点，只有断电且才会为真
	char current_extrude;
	// char printer_software_vendor;		//默认为PRINTER_REPRAP
	bool write_to_spi_flash;
	bool read_from_compress_bin;
	bool is_z_homing;

	int X_max_step; // X最大步数
	int Y_max_step;
	int magic_num_reconfirm; //---G-G-2018-01-17---再次确认

	int preveiv_offset;			  // 打印预览在gcode中的长度
	unsigned int print_left_tick; //---G-G-2017-12-20---
	int print_time_tick;		  // 已经用掉了多少时间
	int current_lines;
	int current_layer;
	double alread_print_time;

	unsigned short curr_layer_num;	// p_printer->sd_cmd.curr_layer_num//------------------------------G-G---------------------
	unsigned short total_layer_num; // p_printer->sd_cmd.total_layer_num//------------------------------G-G---------------------
	double last_print_time;
	double filament_used;

	std::vector <double> m_limits_x;
	std::vector <double> m_limits_y;
	std::vector <double> m_limits_z;

#if BOARD_THREE_IN_ONE_OUT //-------------G-G-2018-01-10--三进一出---
	float multi_in_one_out_init_Z;
	float multi_in_one_out_E1_ratio;
	float multi_in_one_out_E1_init_ratio;
	float multi_in_one_out_E1_ratio_step;
	float multi_in_one_out_E2_ratio;
	float multi_in_one_out_E2_init_ratio;
	float multi_in_one_out_E2_ratio_step;
#else
	float dual_init_Z;
	float dual_init_ratio;
	float dual_ratio_step;
#endif

	int end_tem[1];
	std::string tlp_test_path;
} saved_print_para_t; // 最大不超过256Byte 18*4 + 100

class BreakSave
{
private:
	std::map<std::string, saved_print_para_t> active_pins;

public:
	BreakSave(std::string section_name);
	~BreakSave();
	void handle_connect();
	void save_current_run_status(double eventtime);
	void save_current_Z_status();
	bool Write_File(); // write ini
	void restore_run_status(Printer *p_printer);
	void cmd_BREAK_SAVE_STATUS(GCodeCommand &gcmd);
	void load_variables();
	int select_load_variables();
	void clear_variables();
	void save_to_pconfig(ConfigParser &m_pconfig);
	std::string get_save_file_name();
	void delete_save_files();

	void save_generic_status(double eventtime);
	void save_idle_timeout_info_status(double eventtime);
	void save_virtual_sd_stats_status(double eventtime);
	void save_PauseResume_status(double eventtime);
	void save_printstats_status(double eventtime);

	void save_tool_head_status(double eventtime);
	void save_gcode_status(double eventtime);
	void save_stepper_status(double eventtime);
	void save_printer_fan_status(double eventtime);
	void save_controller_fan_status(double eventtime);
	void save_generic_fans_status(double eventtime);

	void save_heater_fans_status(double eventtime);
	void save_heaters_status(double eventtime);

	void save_z_homing_stats_status(double eventtime);

	std::vector<std::string> m_gcode_cmds_before;
	std::vector<std::string> m_gcode_cmds_after;

	MCU *m_mcu;
    int m_oid;
    int m_pin;
    int m_pullup;
    int m_invert;

	pthread_mutex_t lock; // protects variables below  保护下面的变量
	pthread_cond_t cond;
	bool m_need_write;
	bool m_break_save_enable;
	bool m_break_save_flag;
	bool m_break_save_files_status;
	uint32_t m_break_save_name;
	uint32_t m_break_save_files_num;
	bool m_break_save_reserve;
	saved_print_para_t s_saved_print_para;
	my_temp_UI_info_t my_temp_UI_info;
	std::string m_break_save_path;
	uint32_t m_break_save_num;
	std::string m_save_reason;
	// std::map<std::string, std::string> m_allVariables;
	std::map<std::string, std::string> m_generic_Variables;
	std::map<std::string, std::string> m_idle_timeout_info_Variables;
	std::map<std::string, std::string> m_virtual_sd_stats_Variables;
	std::map<std::string, std::string> m_PauseResume_status_Variables;
	std::map<std::string, std::string> m_printstats_Variables;
	std::map<std::string, std::string> m_tool_head_status_Variables;
	std::map<std::string, std::string> m_gcode_status_Variables;
	std::map<std::string, std::string> m_stepper_status_Variables;
	std::map<std::string, std::string> m_printer_fan_status_Variables;
	std::map<std::string, std::string> m_model_helper_fan_status_Variables;
	std::map<std::string, std::string> m_box_fan_status_Variables;
	std::map<std::string, std::string> m_z_homing_stats_Variables;
	std::vector<std::map<std::string, std::string>> m_controller_fans_status_Variables;
	std::vector<std::map<std::string, std::string>> m_generic_fans_status_Variables;
	std::vector<std::map<std::string, std::string>> m_heater_fans_status_Variables;
	std::vector<std::map<std::string, std::string>> m_heaters_status_Variables;
};

void *break_save_task(void *arg);

#endif