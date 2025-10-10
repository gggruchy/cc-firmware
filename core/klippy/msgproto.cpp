#include "msgproto.h"
#include "klippy.h"
#include "my_string.h"
#define LOG_TAG "msgproto"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define MESSAGE_HEADER_SIZE 2

#if 0
std::list<std::string> lookupParams(std::string msgformat, int enumerations)
{
    std::list<std::string> argparts;
    while (1)
    {
        int pos = msgformat.find(' ');
        if(pos > msgformat.size())
            break;
        argparts.push_back(msgformat.substr(0, pos));
        msgformat = msgformat.substr(pos + 1);
    }
    std::unordered_map<std::string, >
    for(int i = 0; i < argparts.size(); i++)
    {
        int pos = argparts[i].find('=');
        if(pos > argparts[i].size())
            continue;
        Msg temp;
        temp.name = argparts[i].substr(0, pos);
        temp.format = argparts[i].substr(pos + 1);
        v1.push_back(temp);
    }
    return v1;
}

void lookupOutputParams(std::string msgformat)
{
    std::list<std::string> param_types;
    std::string args = msgformat;
    while(1)
    {
        int pos = args.find('%');
        if(pos > args.size())
            break;
        if(pos + 1 >= args.size() || args[pos + 1] != '%')
        {
            for(int i = 0; i < 4; i++)
            {
                param_types.push_back('%u');
            }
        }
    }
    return param_types;
}
#endif

MessageParser::MessageParser(int pin32)
{
    pins_per_bank = pin32;
    init_pin_map(m_pinMap);
    init_message_name_toId(m_namae_to_id);
    init_messages_format_to_id_map_encode(m_format_to_id_encode);
    init_id_to_messages_format_map(m_id_to_format);
    init_messages_format_to_id_map(m_format_to_id);
    init_shutdown_info_map(m_shutdown_info);
}

MessageParser::~MessageParser()
{
}

std::vector<Msg> parse_msg(std::string msg)
{
    std::vector<std::string> argparts;
    std::string name;
    while (1)
    {
        int pos = msg.find(' ');
        if (pos > msg.size())
        {
            name = msg;
            argparts.push_back(msg);
            break;
        }
        argparts.push_back(msg.substr(0, pos));
        msg = msg.substr(pos + 1);
    }
    name = argparts[0];
    std::vector<Msg> items;
    for (int i = 0; i < argparts.size(); i++)
    {
        int pos = argparts[i].find('=');
        if (pos > argparts[i].size())
            continue;
        Msg temp;
        temp.name = argparts[i].substr(0, pos);
        temp.value = argparts[i].substr(pos + 1);
        items.push_back(temp);
    }
    return items;
}

struct command_encoder
{
    uint8_t msg_id, max_size, num_params;
    const uint8_t *param_types;
};
struct command_parser
{
    uint8_t msg_id;
    uint8_t num_args; // args int array size
    uint8_t flags;
    uint8_t num_params;         // args  num
    const uint8_t *param_types; //
    void (*func)(uint32_t *args);
};
enum
{
    PT_uint32,
    PT_int32,
    PT_uint16,
    PT_int16,
    PT_byte,
    PT_string,
    PT_progmem_buffer,
    PT_buffer,
};

uint8_t msg_para_type(std::string msg)
{
    if (msg == "%u")
        return PT_uint32;
    else if (msg == "%i")
        return PT_int32;
    else if (msg == "%hu")
        return PT_uint16;
    else if (msg == "%hi")
        return PT_int16;
    else if (msg == "%c")
        return PT_byte;
    else if (msg == "%s")
        return PT_string;
    else if (msg == "%.*s")
        return PT_progmem_buffer;
    else if (msg == "%*s")
        return PT_buffer;
    else
        return 20;
}
extern const struct command_encoder *ctr_lookup_encoder[];
void MessageParser::init_id_to_messages_format_map(std::map<int, std::string> &messages_format) //----G-G-2022-07-25---
{
    int i = 0;
    messages_format[0] = "identify_response offset=%u data=%.*s"; // N
    messages_format[1] = "identify offset=%u count=%c";           // N
    messages_format[2] = "finalize_config crc=%u";
    messages_format[3] = "config_trsync oid=%c";
    messages_format[4] = "config_buttons oid=%c button_count=%c"; // N
    messages_format[5] = "clear_shutdown ";                       // N
    messages_format[6] = "set_digital_out pin=%u value=%c";
    messages_format[7] = "buttons_ack oid=%c count=%c";                 // N
    messages_format[8] = "endstop_query_state oid=%c";                  // N
    messages_format[9] = "query_adxl345 oid=%c clock=%u rest_ticks=%u"; // N
    messages_format[10] = "trsync_set_timeout oid=%c clock=%u";         // 设置同步超时时间
    messages_format[11] = "config_spi oid=%c pin=%u";                   // N
    messages_format[12] = "get_uptime ";
    messages_format[13] = "endstop_home oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u pin_value=%c trsync_oid=%c trigger_reason=%c"; // 增加一个定时器 采样限位脚电平
    messages_format[14] = "stepper_stop_on_trigger oid=%c trsync_oid=%c";                                                                            // 电机停止信号和同步事件挂钩 增加同步信号链表，同步事件发生时停止电机运动
    messages_format[15] = "debug_ping data=%*s";                                                                                                     // N
    messages_format[16] = "buttons_query oid=%c clock=%u rest_ticks=%u retransmit_count=%c invert=%c";                                               // N
    messages_format[17] = "queue_step oid=%c interval=%u count=%hu add=%hi";
    messages_format[18] = "allocate_oids count=%c";
    messages_format[19] = "config_endstop oid=%c pin=%c pull_up=%c";
    messages_format[20] = "config_spi_shutdown oid=%c spi_oid=%c shutdown_msg=%*s"; // N
    messages_format[21] = "config_digital_out oid=%c pin=%u value=%c default_value=%c max_duration=%u";
    messages_format[22] = "stepper_get_position oid=%c";
    messages_format[23] = "st7920_send_data oid=%c data=%*s"; // N
    messages_format[24] = "query_analog_in oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u min_value=%hu max_value=%hu range_check_count=%c";
    messages_format[25] = "reset_step_clock oid=%c clock=%u";
    messages_format[26] = "i2c_modify_bits oid=%c reg=%*s clear_set_bits=%*s";                                            // N
    messages_format[27] = "config_adxl345 oid=%c spi_oid=%c";                                                             // N
    messages_format[28] = "hd44780_send_data oid=%c data=%*s";                                                            // N
    messages_format[29] = "buttons_add oid=%c pos=%c pin=%u pull_up=%c";                                                  // N
    messages_format[30] = "trsync_start oid=%c report_clock=%u report_ticks=%u expire_reason=%c";                         // 设置检测限位状态报告时间 报告间隔
    messages_format[31] = "config_thermocouple oid=%c spi_oid=%c thermocouple_type=%c";                                   // N
    messages_format[32] = "spi_transfer oid=%c data=%*s";                                                                 // N
    messages_format[33] = "i2c_write oid=%c data=%*s";                                                                    // N
    messages_format[34] = "reset ";                                                                                       // N
    messages_format[35] = "config_st7920 oid=%c cs_pin=%u sclk_pin=%u sid_pin=%u sync_delay_ticks=%u cmd_delay_ticks=%u"; // N
    messages_format[36] = "emergency_stop ";                                                                              // N
    messages_format[37] = "neopixel_send oid=%c";                                                                         // N
    messages_format[38] = "spi_set_bus oid=%c spi_bus=%u mode=%u rate=%u";                                                // N
    messages_format[39] = "config_counter oid=%c pin=%u pull_up=%c";                                                      // N
    messages_format[40] = "get_clock ";
    messages_format[41] = "tmcuart_send oid=%c write=%*s read=%c"; // N
    messages_format[42] = "config_stepper oid=%c step_pin=%c dir_pin=%c invert_step=%c step_pulse_ticks=%u";
    messages_format[43] = "set_digital_out_pwm_cycle oid=%c cycle_ticks=%u";
    messages_format[44] = "debug_write order=%c addr=%u val=%u";                                             // N
    messages_format[45] = "query_counter oid=%c clock=%u poll_ticks=%u sample_ticks=%u";                     // N
    messages_format[46] = "hd44780_send_cmds oid=%c cmds=%*s";                                               // N
    messages_format[47] = "spi_set_software_bus oid=%c miso_pin=%u mosi_pin=%u sclk_pin=%u mode=%u rate=%u"; // N
    messages_format[48] = "neopixel_update oid=%c pos=%hu data=%*s";                                         // N
    messages_format[49] = "debug_read order=%c addr=%u";                                                     // N
    messages_format[50] = "query_thermocouple oid=%c clock=%u rest_ticks=%u min_value=%u max_value=%u";      // N
    messages_format[51] = "config_analog_in oid=%c pin=%u";
    messages_format[52] = "config_spi_without_cs oid=%c";                                                                    // N
    messages_format[53] = "config_hd44780 oid=%c rs_pin=%u e_pin=%u d4_pin=%u d5_pin=%u d6_pin=%u d7_pin=%u delay_ticks=%u"; // N
    messages_format[54] = "config_neopixel oid=%c pin=%u data_size=%hu bit_max_ticks=%u reset_min_ticks=%u";                 // N
    messages_format[55] = "spi_send oid=%c data=%*s";                                                                        // N
    messages_format[56] = "i2c_read oid=%c reg=%*s read_len=%u";                                                             // N
    messages_format[57] = "trsync_trigger oid=%c reason=%c";
    messages_format[58] = "st7920_send_cmds oid=%c cmds=%*s"; // N
    messages_format[59] = "get_config ";
    messages_format[60] = "set_next_step_dir oid=%c dir=%c";
    messages_format[61] = "config_tmcuart oid=%c rx_pin=%u pull_up=%c tx_pin=%u bit_time=%u"; // N
    messages_format[62] = "update_digital_out oid=%c value=%c";
    messages_format[63] = "queue_digital_out oid=%c clock=%u on_ticks=%u";
    messages_format[64] = "config_i2c oid=%c i2c_bus=%u rate=%u address=%u"; // N
    messages_format[65] = "debug_nop ";                                      // N
    messages_format[66] = "starting ";
    messages_format[67] = "is_shutdown static_string_id=%hu";       // N
    messages_format[68] = "shutdown clock=%u static_string_id=%hu"; // N
    messages_format[69] = "stats count=%u sum=%u sumsq=%u";         // N
    messages_format[70] = "uptime high=%u clock=%u";
    messages_format[71] = "clock clock=%u";
    messages_format[72] = "config is_config=%c crc=%u move_count=%hu is_shutdown=%c";
    messages_format[73] = "pong data=%*s";       // N
    messages_format[74] = "debug_result val=%u"; // N
    messages_format[75] = "stepper_position oid=%c pos=%i";
    messages_format[76] = "endstop_state oid=%c homing=%c next_clock=%u pin_value=%c"; // N
    messages_format[77] = "trsync_state oid=%c can_trigger=%c trigger_reason=%c clock=%u";
    messages_format[78] = "analog_in_state oid=%c next_clock=%u value=%hu";
    messages_format[79] = "spi_transfer_response oid=%c response=%*s";                                   // N
    messages_format[80] = "thermocouple_result oid=%c next_clock=%u value=%u fault=%c";                  // N
    messages_format[81] = "i2c_read_response oid=%c response=%*s";                                       // N
    messages_format[82] = "adxl345_end oid=%c end1_clock=%u end2_clock=%u limit_count=%hu sequence=%hu"; // N
    messages_format[83] = "adxl345_start oid=%c start1_clock=%u start2_clock=%u";                        // N
    messages_format[84] = "adxl345_data oid=%c sequence=%hu data=%*s";                                   // N
    messages_format[85] = "buttons_state oid=%c ack_count=%c state=%*s";                                 // N
    messages_format[86] = "tmcuart_response oid=%c read=%*s";                                            // N
    messages_format[87] = "neopixel_result oid=%c success=%c";                                           // N
    messages_format[88] = "counter_state oid=%c next_clock=%u count=%u count_clock=%u";                  // N
    messages_format[89] = "set_pwm_out pin=%u cycle_ticks=%u value=%hu";
    messages_format[90] = "config_pwm_out oid=%c pin=%u cycle_ticks=%u value=%hu default_value=%hu max_duration=%u";
    messages_format[91] = "queue_pwm_out oid=%c clock=%u value=%hu";
    messages_format[92] = "query_lis2dw_status oid=%c";
    messages_format[93] = "query_lis2dw oid=%c clock=%u rest_ticks=%u";
    messages_format[94] = "config_lis2dw oid=%c spi_oid=%c";
    messages_format[95] = "lis2dw_status oid=%c clock=%u query_ticks=%u next_sequence=%hu buffered=%c fifo=%c limit_count=%hu";
    messages_format[96] = "lis2dw_data oid=%c sequence=%hu data=%*s";
    messages_format[97] = "debug_dirzctl oid=%c arg[0]=%u arg[1]=%u arg[2]=%u arg[3]=%u arg[4]=%u arg[5]=%u";
    messages_format[98] = "result_dirzctl oid=%c step=%u tick=%u";
    messages_format[99] = "fan_status oid=%c fan0_speed=%u fan1_speed=%u fan2_speed=%u fan3_speed=%u fan4_speed=%u";
    messages_format[100] = "debug_hx711s oid=%c arg[0]=%u arg[1]=%u arg[2]=%u arg[3]=%u";
    messages_format[101] = "sg_resp oid=%c vd=%c it=%c nt=%u r=%u tt=%u";
    messages_format[102] = "config_dirzctl oid=%c z_count=%c";
    messages_format[103] = "add_dirzctl oid=%c index=%c dir_pin=%u step_pin=%u dir_invert=%c step_invert=%c";
    messages_format[104] = "run_dirzctl oid=%c direct=%c step_us=%u step_cnt=%u";
    messages_format[105] = "config_fancheck oid=%c fan_num=%c fan0_pin=%c pull_up0=%c fan1_pin=%c pull_up1=%c fan2_pin=%c pull_up2=%c fan3_pin=%c pull_up3=%c fan4_pin=%c pull_up4=%c";
    messages_format[106] = "query_fancheck oid=%c which_fan=%c";
    messages_format[107] = "config_hx711s oid=%c hx711_count=%c channels=%u rest_ticks=%u kalman_q=%u kalman_r=%u max_th=%u min_th=%u k=%u";
    messages_format[108] = "add_hx711s oid=%c index=%c clk_pin=%u sdo_pin=%u";
    messages_format[109] = "query_hx711s oid=%c times_read=%hu";
    messages_format[110] = "config_hx711_sample oid=%c hx711_count=%c kalman_q=%u kalman_r=%u";
    messages_format[111] = "add_hx711_sample oid=%c index=%c clk_pin=%u sdo_pin=%u";
    messages_format[112] = "calibration_sample oid=%c times_read=%hu";
    messages_format[113] = "sg_probe_check oid=%c x=%i y=%i z=%i cmd=%i";
    messages_format[114] = "extruder_bootup_info oid=%c crash_flag=%c rest_cause=%c R0=%u R1=%u R2=%u R3=%u R12=%u LR=%u PC=%u xPSR=%u";
    
    

    for (; i < messages_format.size(); i++)
    {
        const struct command_encoder *command_encoder = ctr_lookup_encoder[i];
        if ((command_encoder != NULL))
        {
            std::vector<Msg> items = parse_msg(messages_format[i]);
            if (((items.size() != command_encoder->num_params)))
            {
                LOG_E("messages_format[%d] -> size : %d-%d\n", i, items.size(), command_encoder->num_params);
            }
            const uint8_t *param_types = command_encoder->param_types;
            if (command_encoder->num_params)
            {
                if (param_types)
                {
                    int t = command_encoder->num_params;
                    while (t)
                    {
                        t--;
                        if (msg_para_type(items[t].value) != param_types[t])
                        {
                            // printf("-gggg2--param_types-%d-%d--%d-----\n", i, t, param_types[t]);
                        }
                    }
                }
                else
                {
                    // printf("-gggg3--param_types-%d---%d-----\n", i, command_encoder->num_params);
                }
            }
        }
        else
        {
            // printf("-gggg4---%d-----\n", i);
        }
    }
}

#if 1
void MessageParser::init_messages_format_to_id_map(std::map<std::string, int> &messages_format)
{
    // std::map<std::string, int> messages_format;
    messages_format["i2c_write oid=%c data=%*s"] = 33;
    messages_format["thermocouple_result oid=%c next_clock=%u value=%u fault=%c"] = 80;
    messages_format["query_counter oid=%c clock=%u poll_ticks=%u sample_ticks=%u"] = 45;
    messages_format["config_tmcuart oid=%c rx_pin=%u pull_up=%c tx_pin=%u bit_time=%u"] = 61;
    messages_format["adxl345_data oid=%c sequence=%hu data=%*s"] = 84;
    messages_format["config_st7920 oid=%c cs_pin=%u sclk_pin=%u sid_pin=%u sync_delay_ticks=%u cmd_delay_ticks=%u"] = 35;
    messages_format["config_neopixel oid=%c pin=%u data_size=%hu bit_max_ticks=%u reset_min_ticks=%u"] = 54;
    messages_format["config is_config=%c crc=%u move_count=%hu is_shutdown=%c"] = 72;
    messages_format["config_hd44780 oid=%c rs_pin=%u e_pin=%u d4_pin=%u d5_pin=%u d6_pin=%u d7_pin=%u delay_ticks=%u"] = 53;
    messages_format["config_thermocouple oid=%c spi_oid=%c thermocouple_type=%c"] = 31;
    messages_format["spi_transfer_response oid=%c response=%*s"] = 79;
    messages_format["config_spi_without_cs oid=%c"] = 52;
    messages_format["stepper_stop_on_trigger oid=%c trsync_oid=%c"] = 14;
    messages_format["identify_response offset=%u data=%.*s"] = 0;
    messages_format["config_stepper oid=%c step_pin=%c dir_pin=%c invert_step=%c step_pulse_ticks=%u"] = 42;
    messages_format["query_thermocouple oid=%c clock=%u rest_ticks=%u min_value=%u max_value=%u"] = 50;
    messages_format["set_digital_out pin=%u value=%c"] = 6;
    messages_format["query_analog_in oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u min_value=%hu max_value=%hu range_check_count=%c"] = 24;
    messages_format["clock clock=%u"] = 71;
    messages_format["stats count=%u sum=%u sumsq=%u"] = 69;
    messages_format["stepper_get_position oid=%c"] = 22;
    messages_format["buttons_add oid=%c pos=%c pin=%u pull_up=%c"] = 29;
    messages_format["emergency_stop"] = 36;
    messages_format["endstop_home oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u pin_value=%c trsync_oid=%c trigger_reason=%c"] = 13;
    messages_format["config_analog_in oid=%c pin=%u"] = 51;
    messages_format["config_i2c oid=%c i2c_bus=%u rate=%u address=%u"] = 64;
    messages_format["analog_in_state oid=%c next_clock=%u value=%hu"] = 78;
    messages_format["config_trsync oid=%c"] = 3;
    messages_format["trsync_start oid=%c report_clock=%u report_ticks=%u expire_reason=%c"] = 30;
    messages_format["allocate_oids count=%c"] = 18;
    messages_format["i2c_read_response oid=%c response=%*s"] = 81;
    messages_format["config_counter oid=%c pin=%u pull_up=%c"] = 39;
    messages_format["buttons_query oid=%c clock=%u rest_ticks=%u retransmit_count=%c invert=%c"] = 16;
    messages_format["config_buttons oid=%c button_count=%c"] = 4;
    messages_format["is_shutdown static_string_id=%hu"] = 67;
    messages_format["reset"] = 34;
    messages_format["adxl345_start oid=%c start1_clock=%u start2_clock=%u"] = 83;
    messages_format["st7920_send_data oid=%c data=%*s"] = 23;
    messages_format["i2c_modify_bits oid=%c reg=%*s clear_set_bits=%*s"] = 26;
    messages_format["endstop_query_state oid=%c"] = 8;
    messages_format["hd44780_send_data oid=%c data=%*s"] = 28;
    messages_format["set_digital_out_pwm_cycle oid=%c cycle_ticks=%u"] = 43;
    messages_format["debug_ping data=%*s"] = 15;
    messages_format["get_uptime"] = 12;
    messages_format["pong data=%*s"] = 73;
    messages_format["clear_shutdown"] = 5;
    messages_format["tmcuart_send oid=%c write=%*s read=%c"] = 41;
    messages_format["debug_result val=%u"] = 74;
    messages_format["spi_set_software_bus oid=%c miso_pin=%u mosi_pin=%u sclk_pin=%u mode=%u rate=%u"] = 47;
    messages_format["config_spi_shutdown oid=%c spi_oid=%c shutdown_msg=%*s"] = 20;
    messages_format["hd44780_send_cmds oid=%c cmds=%*s"] = 46;
    messages_format["config_adxl345 oid=%c spi_oid=%c"] = 27;
    messages_format["config_digital_out oid=%c pin=%u value=%c default_value=%c max_duration=%u"] = 21;
    messages_format["debug_read order=%c addr=%u"] = 49;
    messages_format["buttons_ack oid=%c count=%c"] = 7;
    messages_format["neopixel_result oid=%c success=%c"] = 87;
    messages_format["buttons_state oid=%c ack_count=%c state=%*s"] = 85;
    messages_format["update_digital_out oid=%c value=%c"] = 62;
    messages_format["spi_send oid=%c data=%*s"] = 55;
    messages_format["config_endstop oid=%c pin=%c pull_up=%c"] = 19;
    messages_format["debug_write order=%c addr=%u val=%u"] = 44;
    messages_format["reset_step_clock oid=%c clock=%u"] = 25;
    messages_format["neopixel_send oid=%c"] = 37;
    messages_format["get_clock"] = 40;
    messages_format["set_next_step_dir oid=%c dir=%c"] = 60;
    messages_format["i2c_read oid=%c reg=%*s read_len=%u"] = 56;
    messages_format["config_spi oid=%c pin=%u"] = 11;
    messages_format["trsync_set_timeout oid=%c clock=%u"] = 10;
    messages_format["spi_set_bus oid=%c spi_bus=%u mode=%u rate=%u"] = 38;
    messages_format["trsync_state oid=%c can_trigger=%c trigger_reason=%c clock=%u"] = 77;
    messages_format["uptime high=%u clock=%u"] = 70;
    messages_format["get_config"] = 59;
    messages_format["st7920_send_cmds oid=%c cmds=%*s"] = 58;
    messages_format["spi_transfer oid=%c data=%*s"] = 32;
    messages_format["stepper_position oid=%c pos=%i"] = 75;
    messages_format["identify offset=%u count=%c"] = 1;
    messages_format["queue_step oid=%c interval=%u count=%hu add=%hi"] = 17;
    messages_format["endstop_state oid=%c homing=%c next_clock=%u pin_value=%c"] = 76;
    messages_format["counter_state oid=%c next_clock=%u count=%u count_clock=%u"] = 88;
    messages_format["queue_digital_out oid=%c clock=%u on_ticks=%u"] = 63;
    messages_format["tmcuart_response oid=%c read=%*s"] = 86;
    messages_format["shutdown clock=%u static_string_id=%hu"] = 68;
    messages_format["adxl345_end oid=%c end1_clock=%u end2_clock=%u limit_count=%hu sequence=%hu"] = 82;
    messages_format["neopixel_update oid=%c pos=%hu data=%*s"] = 48;
    messages_format["finalize_config crc=%u"] = 2;
    messages_format["trsync_trigger oid=%c reason=%c"] = 57;
    messages_format["query_adxl345 oid=%c clock=%u rest_ticks=%u"] = 9;
    messages_format["starting"] = 66;
    messages_format["debug_nop"] = 65;
    messages_format["set_pwm_out pin=%u cycle_ticks=%u value=%hu"] = 89;
    messages_format["config_pwm_out oid=%c pin=%u cycle_ticks=%u value=%hu default_value=%hu max_duration=%u"] = 90;
    messages_format["queue_pwm_out oid=%c clock=%u value=%hu"] = 91;

    messages_format["query_lis2dw_status oid=%c"] = 92;
    messages_format["query_lis2dw oid=%c clock=%u rest_ticks=%u"] = 93;
    messages_format["config_lis2dw oid=%c spi_oid=%c"] = 94;
    messages_format["lis2dw_status oid=%c clock=%u query_ticks=%u next_sequence=%hu buffered=%c fifo=%c limit_count=%hu"] = 95;
    messages_format["lis2dw_data oid=%c sequence=%hu data=%*s"] = 96;
    messages_format["debug_dirzctl oid=%c arg[0]=%u arg[1]=%u arg[2]=%u arg[3]=%u arg[4]=%u arg[5]=%u"] = 97;
    messages_format["result_dirzctl oid=%c step=%u tick=%u"] = 98;
    messages_format["fan_status oid=%c fan0_speed=%u fan1_speed=%u fan2_speed=%u fan3_speed=%u fan4_speed=%u"] = 99;
    messages_format["debug_hx711s oid=%c arg[0]=%u arg[1]=%u arg[2]=%u arg[3]=%u"] = 100;
    messages_format["sg_resp oid=%c vd=%c it=%c nt=%u r=%u tt=%u"] = 101;
    messages_format["config_dirzctl oid=%c z_count=%c"] = 102;
    messages_format["add_dirzctl oid=%c index=%c dir_pin=%u step_pin=%u dir_invert=%c step_invert=%c"] = 103;
    messages_format["run_dirzctl oid=%c direct=%c step_us=%u step_cnt=%u"] = 104;
    messages_format["config_fancheck oid=%c fan_num=%c fan0_pin=%c pull_up0=%c fan1_pin=%c pull_up1=%c fan2_pin=%c pull_up2=%c fan3_pin=%c pull_up3=%c fan4_pin=%c pull_up4=%c"] = 105;
    messages_format["query_fancheck oid=%c which_fan=%c"] = 106;
    messages_format["config_hx711s oid=%c hx711_count=%c channels=%u rest_ticks=%u kalman_q=%u kalman_r=%u max_th=%u min_th=%u k=%u"] = 107;
    messages_format["add_hx711s oid=%c index=%c clk_pin=%u sdo_pin=%u"] = 108;
    messages_format["query_hx711s oid=%c times_read=%hu"] = 109;
    messages_format["config_hx711_sample oid=%c hx711_count=%c  kalman_q=%u kalman_r=%u"] = 110;
    messages_format["add_hx711_sample oid=%c index=%c clk_pin=%u sdo_pin=%u"] = 111;
    messages_format["calibration_sample oid=%c times_read=%hu"] = 112;
    messages_format["sg_probe_check oid=%c x=%i y=%i z=%i cmd=%i"] = 113;
    messages_format["extruder_bootup_info oid=%c crash_flag=%c rest_cause=%c R0=%u R1=%u R2=%u R3=%u R12=%u LR=%u PC=%u xPSR=%u"] = 114;
}

extern "C" void GAM_printf_sendMSG(uint8_t *buf, uint32_t len, uint8_t msg_id) //---send-G-G-2022-07-25---
{
    printf(" len : %d : ", len);
    if (msg_id < 100)
    {
        std::string msgformat = Printer::GetInstance()->m_mcu->m_serial->m_msgparser->m_id_to_format.at(msg_id);
        std::string msg_name = "";
        for (int i = 0; i < msgformat.size(); i++)
        {
            if (msgformat[i] != ' ')
            {
                msg_name += msgformat[i];
            }
            else
            {
                break;
            }
        }
        printf("id : %d name : %s\t", msg_id, msg_name.c_str());
    }
    else
    {
        printf("id : %d :\t", msg_id);
    }
    int i = 0;
    for (i = 0; i < len; i++)
    {
        printf("%02x ", buf[i]);
    }
    printf("\n");
}

ParseResult MessageParser::parse(uint8_t *s)
{
    ParseResult parse_result;
    int msgid = s[MESSAGE_HEADER_SIZE];
    int pos = MESSAGE_HEADER_SIZE + 1; // 直接加1,避免后面再加
    
    const std::string& msgformat = m_id_to_format.at(msgid);
    
    // 提取消息名称优化
    size_t space_pos = msgformat.find(' ');
    parse_result.msg_name = msgformat.substr(0, space_pos);
    
    // 预分配vector容量
    std::vector<std::string> names;
    std::vector<std::string> parse_types;
    names.reserve(10); // 根据实际参数数量预分配
    parse_types.reserve(10);
    
    // 参数解析优化
    size_t msg_params_size = 0;
    size_t pos_eq;
    while((pos_eq = msgformat.find('=', space_pos)) != std::string::npos) {
        msg_params_size++;
        
        // 提取参数名
        size_t name_start = msgformat.rfind(' ', pos_eq) + 1;
        std::string name = msgformat.substr(name_start, pos_eq - name_start);
        
        // 提取类型
        size_t type_end = msgformat.find(' ', pos_eq);
        if(type_end == std::string::npos) type_end = msgformat.length();
        std::string parse_type = msgformat.substr(pos_eq + 1, type_end - pos_eq - 1);
        
        names.push_back(name);
        parse_types.push_back(parse_type);
        
        space_pos = type_end;
    }

    // 使用unordered_map替代map提高查找效率
    std::map<std::string, uint32_t> PT_uint32_outs;
    std::map<std::string, std::string> PT_string_outs;
    
    // 预分配map容量
    // PT_uint32_outs.reserve(msg_params_size);
    // PT_string_outs.reserve(msg_params_size);

    for (size_t i = 0; i < msg_params_size; i++) {
        const std::string& parse_type = parse_types[i];
        if (parse_type[0] == '%') {
            switch(parse_type[1]) {
                case 'u':
                case 'i':
                case 'h':
                case 'c':
                {
                    auto out = PT_uint32_parse(s, pos);
                    pos = out.pos;
                    PT_uint32_outs.emplace(names[i], out.v);
                    break;
                }
                case 's':
                case '*':
                case '.':
                {
                    auto out = PT_string_parse(s, pos);
                    pos = out.pos;
                    PT_string_outs.emplace(names[i], std::move(out.v));
                    break;
                }
            }
        }
    }

    parse_result.PT_uint32_outs = PT_uint32_outs;
    parse_result.PT_string_outs = PT_string_outs;
    return parse_result;
}
#endif

static uint32_t parse_int(uint8_t **pp)
{
    uint8_t *p = *pp, c = *p++;
    uint32_t v = c & 0x7f;
    if ((c & 0x60) == 0x60)
        v |= -0x20;
    while (c & 0x80)
    {
        c = *p++;
        v = (v << 7) | (c & 0x7f);
    }
    *pp = p;
    return v;
}

PT_uint32_OutParams MessageParser::PT_uint32_parse(uint8_t *s, int pos)
{
    uint32_t c = (uint32_t)(s[pos]);
    pos += 1;
    uint32_t v = c & 0x7f;
    if ((c & 0x60) == 0x60)
        v |= -0x20;
    while (c & 0x80)
    {
        c = (uint32_t)(s[pos]);
        pos += 1;
        v = (v << 7) | (c & 0x7f);
    }
    // if(!m_signed)
    //     v = (v & 0xffffffff);
    PT_uint32_OutParams out = {v, pos};
    return out;
}

PT_string_OutParams MessageParser::PT_string_parse(uint8_t *s, int pos)
{
    int l = s[pos];
    std::string params;
    // std::cout << "l = " << l << std::endl;
    for (int i = pos + 1; i < pos + l + 1; i++)
    {
        // printf("s[i] = %x\n", s[i]);
        params += s[i];
    }
    pos = pos + l + 1;
    PT_string_OutParams out = {params, pos};
    return out;
}

void MessageParser::init_messages_format_to_id_map_encode(std::map<std::string, int> &messages_format)
{
    // std::map<std::string, int> messages_format;
    messages_format["i2c_write oid=%c data=%*s"] = 33;
    messages_format["thermocouple_result oid=%c next_clock=%u value=%u fault=%c"] = 80;
    messages_format["query_counter oid=%c clock=%u poll_ticks=%u sample_ticks=%u"] = 45;
    messages_format["config_tmcuart oid=%c rx_pin=%u pull_up=%c tx_pin=%u bit_time=%u"] = 61;
    messages_format["adxl345_data oid=%c sequence=%hu data=%*s"] = 84;
    messages_format["config_st7920 oid=%c cs_pin=%u sclk_pin=%u sid_pin=%u sync_delay_ticks=%u cmd_delay_ticks=%u"] = 35;
    messages_format["config_neopixel oid=%c pin=%u data_size=%hu bit_max_ticks=%u reset_min_ticks=%u"] = 54;
    messages_format["config is_config=%c crc=%u move_count=%hu is_shutdown=%c"] = 72;
    messages_format["config_hd44780 oid=%c rs_pin=%u e_pin=%u d4_pin=%u d5_pin=%u d6_pin=%u d7_pin=%u delay_ticks=%u"] = 53;
    messages_format["config_thermocouple oid=%c spi_oid=%c thermocouple_type=%c"] = 31;
    messages_format["spi_transfer_response oid=%c response=%*s"] = 79;
    messages_format["config_spi_without_cs oid=%c"] = 52;
    messages_format["stepper_stop_on_trigger oid=%c trsync_oid=%c"] = 14;
    messages_format["identify_response offset=%u data=%.*s"] = 0;
    messages_format["config_stepper oid=%c step_pin=%c dir_pin=%c invert_step=%c step_pulse_ticks=%u"] = 42;
    messages_format["query_thermocouple oid=%c clock=%u rest_ticks=%u min_value=%u max_value=%u"] = 50;
    messages_format["set_digital_out pin=%u value=%c"] = 6;
    messages_format["query_analog_in oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u min_value=%hu max_value=%hu range_check_count=%c"] = 24;
    messages_format["clock clock=%u"] = 71;
    messages_format["stats count=%u sum=%u sumsq=%u"] = 69;
    messages_format["stepper_get_position oid=%c"] = 22;
    messages_format["buttons_add oid=%c pos=%c pin=%u pull_up=%c"] = 29;
    messages_format["emergency_stop"] = 36;
    messages_format["endstop_home oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u pin_value=%c trsync_oid=%c trigger_reason=%c"] = 13;
    messages_format["config_analog_in oid=%c pin=%u"] = 51;
    messages_format["config_i2c oid=%c i2c_bus=%u rate=%u address=%u"] = 64;
    messages_format["analog_in_state oid=%c next_clock=%u value=%hu"] = 78;
    messages_format["config_trsync oid=%c"] = 3;
    messages_format["trsync_start oid=%c report_clock=%u report_ticks=%u expire_reason=%c"] = 30;
    messages_format["allocate_oids count=%c"] = 18;
    messages_format["i2c_read_response oid=%c response=%*s"] = 81;
    messages_format["config_counter oid=%c pin=%u pull_up=%c"] = 39;
    messages_format["buttons_query oid=%c clock=%u rest_ticks=%u retransmit_count=%c invert=%c"] = 16;
    messages_format["config_buttons oid=%c button_count=%c"] = 4;
    messages_format["is_shutdown static_string_id=%hu"] = 67;
    messages_format["reset"] = 34;
    messages_format["adxl345_start oid=%c start1_clock=%u start2_clock=%u"] = 83;
    messages_format["st7920_send_data oid=%c data=%*s"] = 23;
    messages_format["i2c_modify_bits oid=%c reg=%*s clear_set_bits=%*s"] = 26;
    messages_format["endstop_query_state oid=%c"] = 8;
    messages_format["hd44780_send_data oid=%c data=%*s"] = 28;
    messages_format["set_digital_out_pwm_cycle oid=%c cycle_ticks=%u"] = 43;
    messages_format["debug_ping data=%*s"] = 15;
    messages_format["get_uptime"] = 12;
    messages_format["pong data=%*s"] = 73;
    messages_format["clear_shutdown"] = 5;
    messages_format["tmcuart_send oid=%c write=%*s read=%c"] = 41;
    messages_format["debug_result val=%u"] = 74;
    messages_format["spi_set_software_bus oid=%c miso_pin=%u mosi_pin=%u sclk_pin=%u mode=%u rate=%u"] = 47;
    messages_format["config_spi_shutdown oid=%c spi_oid=%c shutdown_msg=%*s"] = 20;
    messages_format["hd44780_send_cmds oid=%c cmds=%*s"] = 46;
    messages_format["config_adxl345 oid=%c spi_oid=%c"] = 27;
    messages_format["config_digital_out oid=%c pin=%u value=%c default_value=%c max_duration=%u"] = 21;
    messages_format["debug_read order=%c addr=%u"] = 49;
    messages_format["buttons_ack oid=%c count=%c"] = 7;
    messages_format["neopixel_result oid=%c success=%c"] = 87;
    messages_format["buttons_state oid=%c ack_count=%c state=%*s"] = 85;
    messages_format["update_digital_out oid=%c value=%c"] = 62;
    messages_format["spi_send oid=%c data=%*s"] = 55;
    messages_format["config_endstop oid=%c pin=%c pull_up=%c"] = 19;
    messages_format["debug_write order=%c addr=%u val=%u"] = 44;
    messages_format["reset_step_clock oid=%c clock=%u"] = 25;
    messages_format["neopixel_send oid=%c"] = 37;
    messages_format["get_clock"] = 40;
    messages_format["set_next_step_dir oid=%c dir=%c"] = 60;
    messages_format["i2c_read oid=%c reg=%*s read_len=%u"] = 56;
    messages_format["config_spi oid=%c pin=%u"] = 11;
    messages_format["trsync_set_timeout oid=%c clock=%u"] = 10;
    messages_format["spi_set_bus oid=%c spi_bus=%u mode=%u rate=%u"] = 38;
    messages_format["trsync_state oid=%c can_trigger=%c trigger_reason=%c clock=%u"] = 77;
    messages_format["uptime high=%u clock=%u"] = 70;
    messages_format["get_config"] = 59;
    messages_format["st7920_send_cmds oid=%c cmds=%*s"] = 58;
    messages_format["spi_transfer oid=%c data=%*s"] = 32;
    messages_format["stepper_position oid=%c pos=%i"] = 75;
    messages_format["identify offset=%u count=%c"] = 1;
    messages_format["queue_step oid=%c interval=%u count=%hu add=%hi"] = 17;
    messages_format["endstop_state oid=%c homing=%c next_clock=%u pin_value=%c"] = 76;
    messages_format["counter_state oid=%c next_clock=%u count=%u count_clock=%u"] = 88;
    messages_format["queue_digital_out oid=%c clock=%u on_ticks=%u"] = 63;
    messages_format["tmcuart_response oid=%c read=%*s"] = 86;
    messages_format["shutdown clock=%u static_string_id=%hu"] = 68;
    messages_format["adxl345_end oid=%c end1_clock=%u end2_clock=%u limit_count=%hu sequence=%hu"] = 82;
    messages_format["neopixel_update oid=%c pos=%hu data=%*s"] = 48;
    messages_format["finalize_config crc=%u"] = 2;
    messages_format["trsync_trigger oid=%c reason=%c"] = 57;
    messages_format["query_adxl345 oid=%c clock=%u rest_ticks=%u"] = 9;
    messages_format["starting"] = 66;
    messages_format["debug_nop"] = 65;
    messages_format["set_pwm_out pin=%u cycle_ticks=%u value=%hu"] = 89;
    messages_format["config_pwm_out oid=%c pin=%u cycle_ticks=%u value=%hu default_value=%hu max_duration=%u"] = 90;
    messages_format["queue_pwm_out oid=%c clock=%u value=%hu"] = 91;
    messages_format["query_lis2dw_status oid=%c"] = 92;
    messages_format["query_lis2dw oid=%c clock=%u rest_ticks=%u"] = 93;
    messages_format["config_lis2dw oid=%c spi_oid=%c"] = 94;
    messages_format["lis2dw_status oid=%c clock=%u query_ticks=%u next_sequence=%hu buffered=%c fifo=%c limit_count=%hu"] = 95;
    messages_format["lis2dw_data oid=%c sequence=%hu data=%*s"] = 96;
    messages_format["debug_dirzctl oid=%c arg[0]=%u arg[1]=%u arg[2]=%u arg[3]=%u arg[4]=%u arg[5]=%u"] = 97;
    messages_format["result_dirzctl oid=%c step=%u tick=%u"] = 98;
    messages_format["fan_status oid=%c fan0_speed=%u fan1_speed=%u fan2_speed=%u fan3_speed=%u fan4_speed=%u"] = 99;
    messages_format["debug_hx711s oid=%c arg[0]=%u arg[1]=%u arg[2]=%u arg[3]=%u"] = 100;
    messages_format["sg_resp oid=%c vd=%c it=%c nt=%u r=%u tt=%u"] = 101;
    messages_format["config_dirzctl oid=%c z_count=%c"] = 102;
    messages_format["add_dirzctl oid=%c index=%c dir_pin=%u step_pin=%u dir_invert=%c step_invert=%c"] = 103;
    messages_format["run_dirzctl oid=%c direct=%c step_us=%u step_cnt=%u"] = 104;
    messages_format["config_fancheck oid=%c fan_num=%c fan0_pin=%c pull_up0=%c fan1_pin=%c pull_up1=%c fan2_pin=%c pull_up2=%c fan3_pin=%c pull_up3=%c fan4_pin=%c pull_up4=%c"] = 105;
    messages_format["query_fancheck oid=%c which_fan=%c"] = 106;
    messages_format["config_hx711s oid=%c hx711_count=%c channels=%u rest_ticks=%u kalman_q=%u kalman_r=%u max_th=%u min_th=%u k=%u"] = 107;
    messages_format["add_hx711s oid=%c index=%c clk_pin=%u sdo_pin=%u"] = 108;
    messages_format["query_hx711s oid=%c times_read=%hu"] = 109;
    messages_format["config_hx711_sample oid=%c hx711_count=%c kalman_q=%u kalman_r=%u"] = 110;
    messages_format["add_hx711_sample oid=%c index=%c clk_pin=%u sdo_pin=%u"] = 111;
    messages_format["calibration_sample oid=%c times_read=%hu"] = 112;
    messages_format["sg_probe_check oid=%c x=%i y=%i z=%i cmd=%i"] = 113;
    messages_format["extruder_bootup_info oid=%c crash_flag=%c rest_cause=%c R0=%u R1=%u R2=%u R3=%u R12=%u LR=%u PC=%u xPSR=%u"] = 114;
}

void MessageParser::init_message_name_toId(std::map<std::string, int> &Message_by_name)
{
    Message_by_name["i2c_write"] = 33;
    Message_by_name["thermocouple_result"] = 80;
    Message_by_name["query_counter"] = 45;
    Message_by_name["config_tmcuart"] = 61;
    Message_by_name["adxl345_data"] = 84;
    Message_by_name["config_st7920"] = 35;
    Message_by_name["config_neopixel"] = 54;
    Message_by_name["config"] = 72;
    Message_by_name["config_hd44780"] = 53;
    Message_by_name["config_thermocouple"] = 31;
    Message_by_name["spi_transfer_response"] = 79;
    Message_by_name["config_spi_without_cs"] = 52;
    Message_by_name["stepper_stop_on_trigger"] = 14;
    Message_by_name["identify_response"] = 0;
    Message_by_name["config_stepper"] = 42;
    Message_by_name["query_thermocouple"] = 50;
    Message_by_name["set_digital_out"] = 6;
    Message_by_name["query_analog_in"] = 24;
    Message_by_name["clock"] = 71;
    Message_by_name["stats"] = 69;
    Message_by_name["stepper_get_position"] = 22;
    Message_by_name["buttons_add"] = 29;
    Message_by_name["emergency_stop"] = 36;
    Message_by_name["endstop_home"] = 13;
    Message_by_name["config_analog_in"] = 51;
    Message_by_name["config_i2c"] = 64;
    Message_by_name["analog_in_state"] = 78;
    Message_by_name["config_trsync"] = 3;
    Message_by_name["trsync_start"] = 30;
    Message_by_name["allocate_oids"] = 18;
    Message_by_name["i2c_read_response"] = 81;
    Message_by_name["config_counter"] = 39;
    Message_by_name["buttons_query"] = 16;
    Message_by_name["config_buttons"] = 4;
    Message_by_name["is_shutdown"] = 67;
    Message_by_name["reset"] = 34;
    Message_by_name["adxl345_start"] = 83;
    Message_by_name["st7920_send_data"] = 23;
    Message_by_name["i2c_modify_bits"] = 26;
    Message_by_name["endstop_query_state"] = 8;
    Message_by_name["hd44780_send_data"] = 28;
    Message_by_name["set_digital_out_pwm_cycle"] = 43;
    Message_by_name["debug_ping"] = 15;
    Message_by_name["get_uptime"] = 12;
    Message_by_name["pong"] = 73;
    Message_by_name["clear_shutdown"] = 5;
    Message_by_name["tmcuart_send"] = 41;
    Message_by_name["debug_result"] = 74;
    Message_by_name["spi_set_software_bus"] = 47;
    Message_by_name["config_spi_shutdown"] = 20;
    Message_by_name["hd44780_send_cmds"] = 46;
    Message_by_name["config_adxl345"] = 27;
    Message_by_name["config_digital_out"] = 21;
    Message_by_name["debug_read"] = 49;
    Message_by_name["buttons_ack"] = 7;
    Message_by_name["neopixel_result"] = 87;
    Message_by_name["buttons_state"] = 85;
    Message_by_name["update_digital_out"] = 62;
    Message_by_name["spi_send"] = 55;
    Message_by_name["config_endstop"] = 19;
    Message_by_name["debug_write"] = 44;
    Message_by_name["reset_step_clock"] = 25;
    Message_by_name["neopixel_send"] = 37;
    Message_by_name["get_clock"] = 40;
    Message_by_name["set_next_step_dir"] = 60;
    Message_by_name["i2c_read"] = 56;
    Message_by_name["config_spi"] = 11;
    Message_by_name["trsync_set_timeout"] = 10;
    Message_by_name["spi_set_bus"] = 38;
    Message_by_name["trsync_state"] = 77;
    Message_by_name["uptime"] = 70;
    Message_by_name["get_config"] = 59;
    Message_by_name["st7920_send_cmds"] = 58;
    Message_by_name["spi_transfer"] = 32;
    Message_by_name["stepper_position"] = 75;
    Message_by_name["identify"] = 1;
    Message_by_name["queue_step"] = 17;
    Message_by_name["endstop_state"] = 76;
    Message_by_name["counter_state"] = 88;
    Message_by_name["queue_digital_out"] = 63;
    Message_by_name["tmcuart_response"] = 86;
    Message_by_name["shutdown"] = 68;
    Message_by_name["adxl345_end"] = 82;
    Message_by_name["neopixel_update"] = 48;
    Message_by_name["finalize_config"] = 2;
    Message_by_name["trsync_trigger"] = 57;
    Message_by_name["query_adxl345"] = 9;
    Message_by_name["starting"] = 66;
    Message_by_name["debug_nop"] = 65;
    Message_by_name["set_pwm_out"] = 89;
    Message_by_name["config_pwm_out"] = 90;
    Message_by_name["queue_pwm_out"] = 91;
    Message_by_name["query_lis2dw_status"] = 92;
    Message_by_name["query_lis2dw"] = 93;
    Message_by_name["config_lis2dw"] = 94;
    Message_by_name["lis2dw_status"] = 95;
    Message_by_name["lis2dw_data"] = 96;
    Message_by_name["debug_dirzctl"] = 97;
    Message_by_name["result_dirzctl"] = 98;
    Message_by_name["fan_status"] = 99;
    Message_by_name["debug_hx711s"] = 100;
    Message_by_name["sg_resp"] = 101;
    Message_by_name["config_dirzctl"] = 102;
    Message_by_name["add_dirzctl"] = 103;
    Message_by_name["run_dirzctl"] = 104;
    Message_by_name["config_fancheck"] = 105;
    Message_by_name["query_fancheck"] = 106;
    Message_by_name["config_hx711s"] = 107;
    Message_by_name["add_hx711s"] = 108;
    Message_by_name["query_hx711s"] = 109;
    Message_by_name["config_hx711_sample"] = 110;
    Message_by_name["add_hx711_sample"] = 111;
    Message_by_name["calibration_sample"] = 112;
    Message_by_name["sg_probe_check"] = 113; 
    Message_by_name["extruder_bootup_info"] = 114; 
}

// 初始化IO引进名称与数值的映射关系
void MessageParser::init_pin_map(std::map<std::string, int> &pinMap)
{
    int i = 0;
    pinMap["PA0"] = i + 0;
    pinMap["PA1"] = i + 1;
    pinMap["PA2"] = i + 2;
    pinMap["PA3"] = i + 3;
    pinMap["PA4"] = i + 4;
    pinMap["PA5"] = i + 5;
    pinMap["PA6"] = i + 6;
    pinMap["PA7"] = i + 7;
    pinMap["PA8"] = i + 8;
    pinMap["PA9"] = i + 9;
    pinMap["PA10"] = i + 10;
    pinMap["PA11"] = i + 11;
    pinMap["PA12"] = i + 12;
    pinMap["PA13"] = i + 13;
    pinMap["PA14"] = i + 14;
    pinMap["PA15"] = i + 15;
    if (pins_per_bank > 16)
    {
        pinMap["PA16"] = i + 16;
        pinMap["PA17"] = i + 17;
        pinMap["PA18"] = i + 18;
        pinMap["PA19"] = i + 19;
        pinMap["PA20"] = i + 20;
        pinMap["PA21"] = i + 21;
        pinMap["PA22"] = i + 22;
        pinMap["PA23"] = i + 23;
        pinMap["PA24"] = i + 24;
        pinMap["PA25"] = i + 25;
        pinMap["PA26"] = i + 26;
        pinMap["PA27"] = i + 27;
        pinMap["PA28"] = i + 28;
        pinMap["PA29"] = i + 29;
        pinMap["PA30"] = i + 30;
        pinMap["PA31"] = i + 31;
    }
    i += pins_per_bank;

    pinMap["PB0"] = i + 0;
    pinMap["PB1"] = i + 1;
    pinMap["PB2"] = i + 2;
    pinMap["PB3"] = i + 3;
    pinMap["PB4"] = i + 4;
    pinMap["PB5"] = i + 5;
    pinMap["PB6"] = i + 6;
    pinMap["PB7"] = i + 7;
    pinMap["PB8"] = i + 8;
    pinMap["PB9"] = i + 9;
    pinMap["PB10"] = i + 10;
    pinMap["PB11"] = i + 11;
    pinMap["PB12"] = i + 12;
    pinMap["PB13"] = i + 13;
    pinMap["PB14"] = i + 14;
    pinMap["PB15"] = i + 15;
    if (pins_per_bank > 16)
    {
        pinMap["PB16"] = i + 16;
        pinMap["PB17"] = i + 17;
        pinMap["PB18"] = i + 18;
        pinMap["PB19"] = i + 19;
        pinMap["PB20"] = i + 20;
        pinMap["PB21"] = i + 21;
        pinMap["PB22"] = i + 22;
        pinMap["PB23"] = i + 23;
        pinMap["PB24"] = i + 24;
        pinMap["PB25"] = i + 25;
        pinMap["PB26"] = i + 26;
        pinMap["PB27"] = i + 27;
        pinMap["PB28"] = i + 28;
        pinMap["PB29"] = i + 29;
        pinMap["PB30"] = i + 30;
        pinMap["PB31"] = i + 31;
    }
    i += pins_per_bank;

    pinMap["PC0"] = i + 0;
    pinMap["PC1"] = i + 1;
    pinMap["PC2"] = i + 2;
    pinMap["PC3"] = i + 3;
    pinMap["PC4"] = i + 4;
    pinMap["PC5"] = i + 5;
    pinMap["PC6"] = i + 6;
    pinMap["PC7"] = i + 7;
    pinMap["PC8"] = i + 8;
    pinMap["PC9"] = i + 9;
    pinMap["PC10"] = i + 10;
    pinMap["PC11"] = i + 11;
    pinMap["PC12"] = i + 12;
    pinMap["PC13"] = i + 13;
    pinMap["PC14"] = i + 14;
    pinMap["PC15"] = i + 15;
    if (pins_per_bank > 16)
    {
        pinMap["PC16"] = i + 16;
        pinMap["PC17"] = i + 17;
        pinMap["PC18"] = i + 18;
        pinMap["PC19"] = i + 19;
        pinMap["PC20"] = i + 20;
        pinMap["PC21"] = i + 21;
        pinMap["PC22"] = i + 22;
        pinMap["PC23"] = i + 23;
        pinMap["PC24"] = i + 24;
        pinMap["PC25"] = i + 25;
        pinMap["PC26"] = i + 26;
        pinMap["PC27"] = i + 27;
        pinMap["PC28"] = i + 28;
        pinMap["PC29"] = i + 29;
        pinMap["PC30"] = i + 30;
        pinMap["PC31"] = i + 31;
    }
    i += pins_per_bank;

    pinMap["PD0"] = i + 0;
    pinMap["PD1"] = i + 1;
    pinMap["PD2"] = i + 2;
    pinMap["PD3"] = i + 3;
    pinMap["PD4"] = i + 4;
    pinMap["PD5"] = i + 5;
    pinMap["PD6"] = i + 6;
    pinMap["PD7"] = i + 7;
    pinMap["PD8"] = i + 8;
    pinMap["PD9"] = i + 9;
    pinMap["PD10"] = i + 10;
    pinMap["PD11"] = i + 11;
    pinMap["PD12"] = i + 12;
    pinMap["PD13"] = i + 13;
    pinMap["PD14"] = i + 14;
    pinMap["PD15"] = i + 15;
    if (pins_per_bank > 16)
    {
        pinMap["PD16"] = i + 16;
        pinMap["PD17"] = i + 17;
        pinMap["PD18"] = i + 18;
        pinMap["PD19"] = i + 19;
        pinMap["PD20"] = i + 20;
        pinMap["PD21"] = i + 21;
        pinMap["PD22"] = i + 22;
        pinMap["PD23"] = i + 23;
        pinMap["PD24"] = i + 24;
        pinMap["PD25"] = i + 25;
        pinMap["PD26"] = i + 26;
        pinMap["PD27"] = i + 27;
        pinMap["PD28"] = i + 28;
        pinMap["PD29"] = i + 29;
        pinMap["PD30"] = i + 30;
        pinMap["PD31"] = i + 31;
    }
    i += pins_per_bank;

    pinMap["PE0"] = i + 0;
    pinMap["PE1"] = i + 1;
    pinMap["PE2"] = i + 2;
    pinMap["PE3"] = i + 3;
    pinMap["PE4"] = i + 4;
    pinMap["PE5"] = i + 5;
    pinMap["PE6"] = i + 6;
    pinMap["PE7"] = i + 7;
    pinMap["PE8"] = i + 8;
    pinMap["PE9"] = i + 9;
    pinMap["PE10"] = i + 10;
    pinMap["PE11"] = i + 11;
    pinMap["PE12"] = i + 12;
    pinMap["PE13"] = i + 13;
    pinMap["PE14"] = i + 14;
    pinMap["PE15"] = i + 15;
    if (pins_per_bank > 16)
    {
        pinMap["PE16"] = i + 16;
        pinMap["PE17"] = i + 17;
        pinMap["PE18"] = i + 18;
        pinMap["PE19"] = i + 19;
        pinMap["PE20"] = i + 20;
        pinMap["PE21"] = i + 21;
        pinMap["PE22"] = i + 22;
        pinMap["PE23"] = i + 23;
        pinMap["PE24"] = i + 24;
        pinMap["PE25"] = i + 25;
        pinMap["PE26"] = i + 26;
        pinMap["PE27"] = i + 27;
        pinMap["PE28"] = i + 28;
        pinMap["PE29"] = i + 29;
        pinMap["PE30"] = i + 30;
        pinMap["PE31"] = i + 31;
    }
    i += pins_per_bank;

    pinMap["PF0"] = i + 0;
    pinMap["PF1"] = i + 1;
    pinMap["PF2"] = i + 2;
    pinMap["PF3"] = i + 3;
    pinMap["PF4"] = i + 4;
    pinMap["PF5"] = i + 5;
    pinMap["PF6"] = i + 6;
    pinMap["PF7"] = i + 7;
    pinMap["PF8"] = i + 8;
    pinMap["PF9"] = i + 9;
    pinMap["PF10"] = i + 10;
    pinMap["PF11"] = i + 11;
    pinMap["PF12"] = i + 12;
    pinMap["PF13"] = i + 13;
    pinMap["PF14"] = i + 14;
    pinMap["PF15"] = i + 15;
    if (pins_per_bank > 16)
    {
        pinMap["PF16"] = i + 16;
        pinMap["PF17"] = i + 17;
        pinMap["PF18"] = i + 18;
        pinMap["PF19"] = i + 19;
        pinMap["PF20"] = i + 20;
        pinMap["PF21"] = i + 21;
        pinMap["PF22"] = i + 22;
        pinMap["PF23"] = i + 23;
        pinMap["PF24"] = i + 24;
        pinMap["PF25"] = i + 25;
        pinMap["PF26"] = i + 26;
        pinMap["PF27"] = i + 27;
        pinMap["PF28"] = i + 28;
        pinMap["PF29"] = i + 29;
        pinMap["PF30"] = i + 30;
        pinMap["PF31"] = i + 31;
    }
    i += pins_per_bank;

    pinMap["PG0"] = i + 0;
    pinMap["PG1"] = i + 1;
    pinMap["PG2"] = i + 2;
    pinMap["PG3"] = i + 3;
    pinMap["PG4"] = i + 4;
    pinMap["PG5"] = i + 5;
    pinMap["PG6"] = i + 6;
    pinMap["PG7"] = i + 7;
    pinMap["PG8"] = i + 8;
    pinMap["PG9"] = i + 9;
    pinMap["PG10"] = i + 10;
    pinMap["PG11"] = i + 11;
    pinMap["PG12"] = i + 12;
    pinMap["PG13"] = i + 13;
    pinMap["PG14"] = i + 14;
    pinMap["PG15"] = i + 15;
    if (pins_per_bank > 16)
    {
        pinMap["PG16"] = i + 16;
        pinMap["PG17"] = i + 17;
        pinMap["PG18"] = i + 18;
        pinMap["PG19"] = i + 19;
        pinMap["PG20"] = i + 20;
        pinMap["PG21"] = i + 21;
        pinMap["PG22"] = i + 22;
        pinMap["PG23"] = i + 23;
        pinMap["PG24"] = i + 24;
        pinMap["PG25"] = i + 25;
        pinMap["PG26"] = i + 26;
        pinMap["PG27"] = i + 27;
        pinMap["PG28"] = i + 28;
        pinMap["PG29"] = i + 29;
        pinMap["PG30"] = i + 30;
        pinMap["PG31"] = i + 31;
    }
    i += pins_per_bank;

    pinMap["PH0"] = i + 0;
    pinMap["PH1"] = i + 1;
    pinMap["PH2"] = i + 2;
    pinMap["PH3"] = i + 3;
    pinMap["PH4"] = i + 4;
    pinMap["PH5"] = i + 5;
    pinMap["PH6"] = i + 6;
    pinMap["PH7"] = i + 7;
    pinMap["PH8"] = i + 8;
    pinMap["PH9"] = i + 9;
    pinMap["PH10"] = i + 10;
    pinMap["PH11"] = i + 11;
    pinMap["PH12"] = i + 12;
    pinMap["PH13"] = i + 13;
    pinMap["PH14"] = i + 14;
    pinMap["PH15"] = i + 15;
    if (pins_per_bank > 16)
    {
        pinMap["PH16"] = i + 16;
        pinMap["PH17"] = i + 17;
        pinMap["PH18"] = i + 18;
        pinMap["PH19"] = i + 19;
        pinMap["PH20"] = i + 20;
        pinMap["PH21"] = i + 21;
        pinMap["PH22"] = i + 22;
        pinMap["PH23"] = i + 23;
        pinMap["PH24"] = i + 24;
        pinMap["PH25"] = i + 25;
        pinMap["PH26"] = i + 26;
        pinMap["PH27"] = i + 27;
        pinMap["PH28"] = i + 28;
        pinMap["PH29"] = i + 29;
        pinMap["PH30"] = i + 30;
        pinMap["PH31"] = i + 31;
    }
    i += pins_per_bank;

    pinMap["ADC_TEMPERATURE"] = 254;
}

void MessageParser::init_shutdown_info_map(std::map<int, std::string> &shutdown_info)
{
    shutdown_info[2] = "Shutdown cleared when not shutdown";
    shutdown_info[3] = "Timer too close";
    shutdown_info[4] = "sentinel timer called";
    shutdown_info[5] = "Invalid command";
    shutdown_info[6] = "Message encode error";
    shutdown_info[7] = "Command parser error";
    shutdown_info[8] = "Command request";
    shutdown_info[9] = "config_reset only available when shutdown";
    shutdown_info[10] = "oids already allocated";
    shutdown_info[11] = "Can't assign oid";
    shutdown_info[12] = "Invalid oid type";
    shutdown_info[13] = "Already finalized";
    shutdown_info[14] = "Invalid move request size";
    shutdown_info[15] = "Move queue overflow";
    shutdown_info[16] = "alloc_chunks failed";
    shutdown_info[17] = "alloc_chunk failed";
    shutdown_info[18] = "update_digital_out not valid with active queue";
    shutdown_info[19] = "Scheduled digital out event will exceed max_duration";
    shutdown_info[20] = "Can not set soft pwm cycle ticks while updates pending";
    shutdown_info[21] = "Missed scheduling of next digital out event";
    shutdown_info[22] = "Can't reset time when stepper active";
    shutdown_info[23] = "Invalid count parameter";
    shutdown_info[24] = "Stepper too far in past";
    shutdown_info[25] = "Can't add signal that is already active";
    shutdown_info[26] = "ADC out of range";
    shutdown_info[27] = "Invalid spi config";
    shutdown_info[28] = "Thermocouple reader fault";
    shutdown_info[29] = "Thermocouple ADC out of range";
    shutdown_info[30] = "Invalid thermocouple chip type";
    shutdown_info[31] = "i2c_modify_bits: Odd number of bits!";
    shutdown_info[32] = "Invalid buttons retransmit count";
    shutdown_info[33] = "Set button past maximum button count";
    shutdown_info[34] = "Max of 8 buttons";
    shutdown_info[35] = "tmcuart data too large";
    shutdown_info[36] = "Invalid neopixel update command";
    shutdown_info[37] = "Invalid neopixel data_size";
    shutdown_info[38] = "Not a valid input pin";
    shutdown_info[39] = "Not an output pin";
    shutdown_info[40] = "Rescheduled timer in the past";
    shutdown_info[41] = "Not a valid ADC pin";
    shutdown_info[42] = "Failed to send i2c addr";
    shutdown_info[43] = "i2c timeout";
    shutdown_info[44] = "Unsupported i2c bus";
    shutdown_info[45] = "Invalid spi bus";
    shutdown_info[46] = "reset";
    shutdown_info[47] = "Max of 4 Z AXIS";
    shutdown_info[48] = "Set direct z ctl maximum count";
    shutdown_info[49] = "The fan number is more than USR_EXTRAS_FAN_MAX";
    shutdown_info[50] = "args[1] is invalid";
    shutdown_info[51] = "Max of 4 hx711";
    shutdown_info[52] = "Set hx711 past maximum count";
    shutdown_info[0xff] = "";
}

#define PROGMEM

static const uint8_t cmd_para_u16[] = {PT_uint16};
static const uint8_t cmd_para_u32_u16[] = {PT_uint32, PT_uint16};
static const uint8_t cmd_para_u32_ppb[] = {PT_uint32, PT_progmem_buffer};
static const uint8_t cmd_para_u32_u32_u32[] = {PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_u32_u32[] = {PT_uint32, PT_uint32};
static const uint8_t cmd_para_u32[] = {PT_uint32};
static const uint8_t cmd_para_c8_u32_u16_c8[] = {PT_byte, PT_uint32, PT_uint16, PT_byte};
static const uint8_t cmd_para_ptb[] = {PT_buffer};
static const uint8_t cmd_para_c8_i32[] = {PT_byte, PT_int32};
static const uint8_t cmd_para_c8_c8_u32_c8[] = {PT_byte, PT_byte, PT_uint32, PT_byte};
static const uint8_t cmd_para_c8_c8_c8_u32[] = {PT_byte, PT_byte, PT_byte, PT_uint32};
static const uint8_t cmd_para_c8_u32_u16[] = {PT_byte, PT_uint32, PT_uint16};
static const uint8_t cmd_para_c8_ptb[] = {PT_byte, PT_buffer};
static const uint8_t cmd_para_c8_u32_u32_c8[] = {PT_byte, PT_uint32, PT_uint32, PT_byte};
static const uint8_t cmd_para_c8_u32_u32_u16_u16[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_uint16, PT_uint16};
static const uint8_t cmd_para_c8_u32_u32[] PROGMEM = {PT_byte, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_u16_ptb[] PROGMEM = {PT_byte, PT_uint16, PT_buffer};
static const uint8_t cmd_para_c8_c8_ptb[] PROGMEM = {PT_byte, PT_byte, PT_buffer};
static const uint8_t cmd_para_c8_c8[] PROGMEM = {PT_byte, PT_byte};
static const uint8_t cmd_para_c8_c8_u32[] PROGMEM = {PT_byte, PT_byte, PT_uint32};
static const uint8_t cmd_para_c8_u32_u32_u32[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_u32_c8[] PROGMEM = {PT_uint32, PT_byte};
static const uint8_t cmd_para_c8[] PROGMEM = {PT_byte};
static const uint8_t cmd_para_c8_u32[] PROGMEM = {PT_byte, PT_uint32};
static const uint8_t cmd_para_c8_u32_u32_c8_u32_c8_c8_c8[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_byte, PT_uint32, PT_byte, PT_byte, PT_byte};
static const uint8_t cmd_para_c8_u32_u32_c8_c8[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_byte, PT_byte};
static const uint8_t cmd_para_c8_u32_u16_i16[] PROGMEM = {PT_byte, PT_uint32, PT_uint16, PT_int16};
static const uint8_t cmd_para_c8_c8_c8[] PROGMEM = {PT_byte, PT_byte, PT_byte};
static const uint8_t cmd_para_c8_u32_c8_c8_u32[] PROGMEM = {PT_byte, PT_uint32, PT_byte, PT_byte, PT_uint32};
static const uint8_t cmd_para_c8_u32_u32_c8_u32_u16_u16_c8[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_byte, PT_uint32, PT_uint16, PT_uint16, PT_byte};
static const uint8_t cmd_para_c8_ptb_ptb[] PROGMEM = {PT_byte, PT_buffer, PT_buffer};
static const uint8_t cmd_para_c8_u32_u32_u32_u32_u32[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_u32_c8[] PROGMEM = {PT_byte, PT_uint32, PT_byte};
static const uint8_t cmd_para_c8_ptb_c8[] PROGMEM = {PT_byte, PT_buffer, PT_byte};
static const uint8_t cmd_para_c8_c8_c8_c8[] PROGMEM = {PT_byte, PT_byte, PT_byte, PT_byte};
static const uint8_t cmd_para_c8_u32_u32_u32_u32[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_u32_u32_u32_u32_u32_u32_u32[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_u32_u16_u32_u32[] PROGMEM = {PT_byte, PT_uint32, PT_uint16, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_ptb_u32[] PROGMEM = {PT_byte, PT_buffer, PT_uint32};
static const uint8_t cmd_para_c8_u32_c8_u32_u32[] PROGMEM = {PT_byte, PT_uint32, PT_byte, PT_uint32, PT_uint32};
static const uint8_t cmd_para_u32_u32_u16[] PROGMEM = {PT_uint32, PT_uint32, PT_uint16};
static const uint8_t cmd_para_c8_u32_u32_u16_u16_u32[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_uint16, PT_uint16, PT_uint32};
static const uint8_t cmd_para_c8_u32_u32_u16_c8_c8_u16[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_uint16, PT_byte, PT_byte, PT_uint16};
static const uint8_t cmd_para_c8_u32_u32_u32_u32_u32_u32[] PROGMEM = {PT_byte, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_c8_u32_u32_u32_u32_u32[] PROGMEM = {PT_byte, PT_byte, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_c8_u32_u32_u32_u32_u32_u32[] PROGMEM = {PT_byte, PT_byte, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_c8_u32_u32_u32_u32_u32_u32_u32[] PROGMEM = {PT_byte, PT_byte, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_c8_c8_u16_u32_i32_i32_i32_i32[] PROGMEM = {PT_byte, PT_byte, PT_byte, PT_uint16, PT_uint32, PT_int32, PT_int32, PT_int32, PT_int32};
static const uint8_t cmd_para_c8_c8_c8_u16_u32_i32_i32_i32_i32_i32[] PROGMEM = {PT_byte, PT_byte, PT_byte, PT_uint16, PT_uint32, PT_int32, PT_int32, PT_int32, PT_int32, PT_int32};
static const uint8_t cmd_para_c8_c8_u32_u32_c8_c8[] PROGMEM = {PT_byte, PT_byte, PT_uint32, PT_uint32, PT_byte, PT_byte};
static const uint8_t cmd_para_c8_c8_u32_u32[] PROGMEM = {PT_byte, PT_byte, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_c8_c8_c8_c8_c8_c8_c8_c8_c8_c8_c8[] PROGMEM = {PT_byte, PT_byte, PT_byte, PT_byte, PT_byte, PT_byte, PT_byte, PT_byte, PT_byte, PT_byte, PT_byte, PT_byte};
static const uint8_t cmd_para_c8_u16[] PROGMEM = {PT_byte, PT_uint16};
static const uint8_t cmd_para_c8_c8_c8_c8_u32[] PROGMEM = {PT_byte, PT_byte, PT_byte, PT_byte, PT_uint32};
static const uint8_t cmd_para_c8_c8_c8_u32_u32[] PROGMEM = {PT_byte, PT_byte, PT_byte, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_i32_i32_i32_i32[] PROGMEM = {PT_byte, PT_int32, PT_int32, PT_int32, PT_int32};
static const uint8_t cmd_para_c8_c8_c8_u32_u32_u32[] PROGMEM = {PT_byte, PT_byte, PT_byte, PT_uint32, PT_uint32, PT_uint32};
static const uint8_t cmd_para_c8_c8_c8_u32_u32_u32_u32_u32_u32_u32_u32[] PROGMEM = {PT_byte, PT_byte, PT_byte, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32, PT_uint32};


// %.*s ppb
// %*s ptb
const struct command_encoder command_encoder_0 PROGMEM = {0, 64, 2, cmd_para_u32_ppb};                                  // messages_format[0] = "identify_response offset=%u data=%.*s";
const struct command_encoder command_encoder_1 PROGMEM = {1, 64, 2, cmd_para_u32_c8};                                   // messages_format[1] = "identify offset=%u count=%c";
const struct command_encoder command_encoder_2 PROGMEM = {2, 64, 1, cmd_para_u32};                                      // messages_format[2] = "finalize_config crc=%u";
const struct command_encoder command_encoder_3 PROGMEM = {3, 64, 1, cmd_para_c8};                                       // messages_format[3] = "config_trsync oid=%c";
const struct command_encoder command_encoder_4 PROGMEM = {4, 64, 2, cmd_para_c8_c8};                                    // messages_format[4] = "config_buttons oid=%c button_count=%c";
const struct command_encoder command_encoder_5 PROGMEM = {5, 64, 0, NULL};                                              // messages_format[5] = "clear_shutdown ";
const struct command_encoder command_encoder_6 PROGMEM = {6, 64, 2, cmd_para_u32_c8};                                   // messages_format[6] = "set_digital_out pin=%u value=%c";
const struct command_encoder command_encoder_7 PROGMEM = {7, 64, 2, cmd_para_c8_c8};                                    // messages_format[7] = "buttons_ack oid=%c count=%c";
const struct command_encoder command_encoder_8 PROGMEM = {8, 64, 1, cmd_para_c8};                                       // messages_format[8] = "endstop_query_state oid=%c";
const struct command_encoder command_encoder_9 PROGMEM = {9, 64, 3, cmd_para_c8_u32_u32};                               // messages_format[9] = "query_adxl345 oid=%c clock=%u rest_ticks=%u";
const struct command_encoder command_encoder_10 PROGMEM = {10, 64, 2, cmd_para_c8_u32};                                 // messages_format[10] = "trsync_set_timeout oid=%c clock=%u";
const struct command_encoder command_encoder_11 PROGMEM = {11, 64, 2, cmd_para_c8_u32};                                 // messages_format[11] = "config_spi oid=%c pin=%u";
const struct command_encoder command_encoder_12 PROGMEM = {12, 64, 0, NULL};                                            // messages_format[12] = "get_uptime ";
const struct command_encoder command_encoder_13 PROGMEM = {13, 64, 8, cmd_para_c8_u32_u32_c8_u32_c8_c8_c8};             // messages_format[13] = "endstop_home oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u pin_value=%c trsync_oid=%c trigger_reason=%c";
const struct command_encoder command_encoder_14 PROGMEM = {14, 64, 2, cmd_para_c8_c8};                                  // messages_format[14] = "stepper_stop_on_trigger oid=%c trsync_oid=%c";
const struct command_encoder command_encoder_15 PROGMEM = {15, 64, 1, cmd_para_ptb};                                    // messages_format[15] = "debug_ping data=%*s";
const struct command_encoder command_encoder_16 PROGMEM = {16, 64, 5, cmd_para_c8_u32_u32_c8_c8};                       // messages_format[16] = "buttons_query oid=%c clock=%u rest_ticks=%u retransmit_count=%c invert=%c";
const struct command_encoder command_encoder_17 PROGMEM = {17, 64, 4, cmd_para_c8_u32_u16_i16};                         // messages_format[17] = "queue_step oid=%c interval=%u count=%hu add=%hi";
const struct command_encoder command_encoder_18 PROGMEM = {18, 64, 1, cmd_para_c8};                                     // messages_format[18] = "allocate_oids count=%c";
const struct command_encoder command_encoder_19 PROGMEM = {19, 64, 3, cmd_para_c8_c8_c8};                               // messages_format[19] = "config_endstop oid=%c pin=%c pull_up=%c";
const struct command_encoder command_encoder_20 PROGMEM = {20, 64, 3, cmd_para_c8_c8_ptb};                              // messages_format[20] = "config_spi_shutdown oid=%c spi_oid=%c shutdown_msg=%*s";
const struct command_encoder command_encoder_21 PROGMEM = {21, 64, 5, cmd_para_c8_u32_c8_c8_u32};                       // messages_format[21] = "config_digital_out oid=%c pin=%u value=%c default_value=%c max_duration=%u";
const struct command_encoder command_encoder_22 PROGMEM = {22, 64, 1, cmd_para_c8};                                     // messages_format[22] = "stepper_get_position oid=%c";
const struct command_encoder command_encoder_23 PROGMEM = {23, 64, 2, cmd_para_c8_ptb};                                 // messages_format[23] = "st7920_send_data oid=%c data=%*s";
const struct command_encoder command_encoder_24 PROGMEM = {24, 64, 8, cmd_para_c8_u32_u32_c8_u32_u16_u16_c8};           // messages_format[24] = "query_analog_in oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u min_value=%hu max_value=%hu range_check_count=%c";
const struct command_encoder command_encoder_25 PROGMEM = {25, 64, 2, cmd_para_c8_u32};                                 // messages_format[25] = "reset_step_clock oid=%c clock=%u";
const struct command_encoder command_encoder_26 PROGMEM = {26, 64, 3, cmd_para_c8_ptb_ptb};                             // messages_format[26] = "i2c_modify_bits oid=%c reg=%*s clear_set_bits=%*s";
const struct command_encoder command_encoder_27 PROGMEM = {27, 64, 2, cmd_para_c8_c8};                                  // messages_format[27] = "config_adxl345 oid=%c spi_oid=%c";
const struct command_encoder command_encoder_28 PROGMEM = {28, 64, 2, cmd_para_c8_ptb};                                 // messages_format[28] = "hd44780_send_data oid=%c data=%*s";
const struct command_encoder command_encoder_29 PROGMEM = {29, 64, 4, cmd_para_c8_c8_u32_c8};                           // messages_format[29] = "buttons_add oid=%c pos=%c pin=%u pull_up=%c";
const struct command_encoder command_encoder_30 PROGMEM = {30, 64, 4, cmd_para_c8_u32_u32_c8};                          // messages_format[30] = "trsync_start oid=%c report_clock=%u report_ticks=%u expire_reason=%c";
const struct command_encoder command_encoder_31 PROGMEM = {31, 64, 3, cmd_para_c8_c8_c8};                               // messages_format[31] = "config_thermocouple oid=%c spi_oid=%c thermocouple_type=%c";
const struct command_encoder command_encoder_32 PROGMEM = {32, 64, 2, cmd_para_c8_ptb};                                 // messages_format[32] = "spi_transfer oid=%c data=%*s";
const struct command_encoder command_encoder_33 PROGMEM = {33, 64, 2, cmd_para_c8_ptb};                                 // messages_format[33] = "i2c_write oid=%c data=%*s";
const struct command_encoder command_encoder_34 PROGMEM = {34, 64, 0, NULL};                                            // messages_format[34] = "reset ";
const struct command_encoder command_encoder_35 PROGMEM = {35, 64, 6, cmd_para_c8_u32_u32_u32_u32_u32};                 // messages_format[35] = "config_st7920 oid=%c cs_pin=%u sclk_pin=%u sid_pin=%u sync_delay_ticks=%u cmd_delay_ticks=%u";
const struct command_encoder command_encoder_36 PROGMEM = {36, 64, 0, NULL};                                            // messages_format[36] = "emergency_stop ";
const struct command_encoder command_encoder_37 PROGMEM = {37, 64, 1, cmd_para_c8};                                     // messages_format[37] = "neopixel_send oid=%c";
const struct command_encoder command_encoder_38 PROGMEM = {38, 64, 4, cmd_para_c8_u32_u32_u32};                         // messages_format[38] = "spi_set_bus oid=%c spi_bus=%u mode=%u rate=%u";
const struct command_encoder command_encoder_39 PROGMEM = {39, 64, 3, cmd_para_c8_u32_c8};                              // messages_format[39] = "config_counter oid=%c pin=%u pull_up=%c";
const struct command_encoder command_encoder_40 PROGMEM = {40, 64, 0, NULL};                                            // messages_format[40] = "get_clock ";
const struct command_encoder command_encoder_41 PROGMEM = {41, 64, 3, cmd_para_c8_ptb_c8};                              // messages_format[41] = "tmcuart_send oid=%c write=%*s read=%c";
const struct command_encoder command_encoder_42 PROGMEM = {42, 64, 5, cmd_para_c8_c8_c8_c8_u32};                        // messages_format[42] = "config_stepper oid=%c step_pin=%c dir_pin=%c invert_step=%c step_pulse_ticks=%u";
const struct command_encoder command_encoder_43 PROGMEM = {43, 64, 2, cmd_para_c8_u32};                                 // messages_format[43] = "set_digital_out_pwm_cycle oid=%c cycle_ticks=%u";
const struct command_encoder command_encoder_44 PROGMEM = {44, 64, 3, cmd_para_c8_u32_u32};                             // messages_format[44] = "debug_write order=%c addr=%u val=%u";
const struct command_encoder command_encoder_45 PROGMEM = {45, 64, 4, cmd_para_c8_u32_u32_u32};                         // messages_format[45] = "query_counter oid=%c clock=%u poll_ticks=%u sample_ticks=%u";
const struct command_encoder command_encoder_46 PROGMEM = {46, 64, 2, cmd_para_c8_ptb};                                 // messages_format[46] = "hd44780_send_cmds oid=%c cmds=%*s";
const struct command_encoder command_encoder_47 PROGMEM = {47, 64, 6, cmd_para_c8_u32_u32_u32_u32_u32};                 // messages_format[47] = "spi_set_software_bus oid=%c miso_pin=%u mosi_pin=%u sclk_pin=%u mode=%u rate=%u";
const struct command_encoder command_encoder_48 PROGMEM = {48, 64, 3, cmd_para_c8_u16_ptb};                             // messages_format[48] = "neopixel_update oid=%c pos=%hu data=%*s";
const struct command_encoder command_encoder_49 PROGMEM = {49, 64, 2, cmd_para_c8_u32};                                 // messages_format[49] = "debug_read order=%c addr=%u";
const struct command_encoder command_encoder_50 PROGMEM = {50, 64, 5, cmd_para_c8_u32_u32_u32_u32};                     // messages_format[50] = "query_thermocouple oid=%c clock=%u rest_ticks=%u min_value=%u max_value=%u";
const struct command_encoder command_encoder_51 PROGMEM = {51, 64, 2, cmd_para_c8_u32};                                 // messages_format[51] = "config_analog_in oid=%c pin=%u";
const struct command_encoder command_encoder_52 PROGMEM = {52, 64, 1, cmd_para_c8};                                     // messages_format[52] = "config_spi_without_cs oid=%c";
const struct command_encoder command_encoder_53 PROGMEM = {53, 64, 8, cmd_para_c8_u32_u32_u32_u32_u32_u32_u32};         // messages_format[53] = "config_hd44780 oid=%c rs_pin=%u e_pin=%u d4_pin=%u d5_pin=%u d6_pin=%u d7_pin=%u delay_ticks=%u";
const struct command_encoder command_encoder_54 PROGMEM = {54, 64, 5, cmd_para_c8_u32_u16_u32_u32};                     // messages_format[54] = "config_neopixel oid=%c pin=%u data_size=%hu bit_max_ticks=%u reset_min_ticks=%u";
const struct command_encoder command_encoder_55 PROGMEM = {55, 64, 2, cmd_para_c8_ptb};                                 // messages_format[55] = "spi_send oid=%c data=%*s";
const struct command_encoder command_encoder_56 PROGMEM = {56, 64, 3, cmd_para_c8_ptb_u32};                             // messages_format[56] = "i2c_read oid=%c reg=%*s read_len=%u";
const struct command_encoder command_encoder_57 PROGMEM = {57, 64, 2, cmd_para_c8_c8};                                  // messages_format[57] = "trsync_trigger oid=%c reason=%c";
const struct command_encoder command_encoder_58 PROGMEM = {58, 64, 2, cmd_para_c8_ptb};                                 // messages_format[58] = "st7920_send_cmds oid=%c cmds=%*s";
const struct command_encoder command_encoder_59 PROGMEM = {59, 64, 0, NULL};                                            // messages_format[59] = "get_config ";
const struct command_encoder command_encoder_60 PROGMEM = {60, 64, 2, cmd_para_c8_c8};                                  // messages_format[60] = "set_next_step_dir oid=%c dir=%c";
const struct command_encoder command_encoder_61 PROGMEM = {61, 64, 5, cmd_para_c8_u32_c8_u32_u32};                      // messages_format[61] = "config_tmcuart oid=%c rx_pin=%u pull_up=%c tx_pin=%u bit_time=%u";
const struct command_encoder command_encoder_62 PROGMEM = {62, 64, 2, cmd_para_c8_c8};                                  // messages_format[62] = "update_digital_out oid=%c value=%c";
const struct command_encoder command_encoder_63 PROGMEM = {63, 64, 3, cmd_para_c8_u32_u32};                             // messages_format[63] = "queue_digital_out oid=%c clock=%u on_ticks=%u";
const struct command_encoder command_encoder_64 PROGMEM = {64, 64, 4, cmd_para_c8_u32_u32_u32};                         // messages_format[64] = "config_i2c oid=%c i2c_bus=%u rate=%u address=%u";
const struct command_encoder command_encoder_65 PROGMEM = {65, 64, 0, NULL};                                            // messages_format[65] = "debug_nop ";
const struct command_encoder command_encoder_66 PROGMEM = {66, 6, 0, NULL};                                             // messages_format[66] = "starting ";
const struct command_encoder command_encoder_67 PROGMEM = {67, 9, 1, cmd_para_u16};                                     // messages_format[67] = "is_shutdown static_string_id=%hu";
const struct command_encoder command_encoder_68 PROGMEM = {68, 14, 2, cmd_para_u32_u16};                                // messages_format[68] = "shutdown clock=%u static_string_id=%hu";
const struct command_encoder command_encoder_69 PROGMEM = {69, 21, 3, cmd_para_u32_u32_u32};                            // messages_format[69] = "stats count=%u sum=%u sumsq=%u";
const struct command_encoder command_encoder_70 PROGMEM = {70, 16, 2, cmd_para_u32_u32};                                // messages_format[70] = "uptime high=%u clock=%u";
const struct command_encoder command_encoder_71 PROGMEM = {71, 11, 1, cmd_para_u32};                                    // messages_format[71] = "clock clock=%u";
const struct command_encoder command_encoder_72 PROGMEM = {72, 18, 4, cmd_para_c8_u32_u16_c8};                          // messages_format[72] = "config is_config=%c crc=%u move_count=%hu is_shutdown=%c";
const struct command_encoder command_encoder_73 PROGMEM = {73, 64, 1, cmd_para_ptb};                                    // messages_format[73] = "pong data=%*s";
const struct command_encoder command_encoder_74 PROGMEM = {74, 11, 1, cmd_para_u32};                                    // messages_format[74] = "debug_result val=%u";
const struct command_encoder command_encoder_75 PROGMEM = {75, 13, 2, cmd_para_c8_i32};                                 // messages_format[75] = "stepper_position oid=%c pos=%i";
const struct command_encoder command_encoder_76 PROGMEM = {76, 17, 4, cmd_para_c8_c8_u32_c8};                           // messages_format[76] = "endstop_state oid=%c homing=%c next_clock=%u pin_value=%c";
const struct command_encoder command_encoder_77 PROGMEM = {77, 17, 4, cmd_para_c8_c8_c8_u32};                           // messages_format[77] = "trsync_state oid=%c can_trigger=%c trigger_reason=%c clock=%u";
const struct command_encoder command_encoder_78 PROGMEM = {78, 16, 3, cmd_para_c8_u32_u16};                             // messages_format[78] = "analog_in_state oid=%c next_clock=%u value=%hu";
const struct command_encoder command_encoder_79 PROGMEM = {79, 64, 2, cmd_para_c8_ptb};                                 // messages_format[79] = "spi_transfer_response oid=%c response=%*s";
const struct command_encoder command_encoder_80 PROGMEM = {80, 20, 4, cmd_para_c8_u32_u32_c8};                          // messages_format[80] = "thermocouple_result oid=%c next_clock=%u value=%u fault=%c";
const struct command_encoder command_encoder_81 PROGMEM = {81, 64, 2, cmd_para_c8_ptb};                                 // messages_format[81] = "i2c_read_response oid=%c response=%*s";
const struct command_encoder command_encoder_82 PROGMEM = {82, 24, 5, cmd_para_c8_u32_u32_u16_u16};                     // messages_format[82] = "adxl345_end oid=%c end1_clock=%u end2_clock=%u limit_count=%hu sequence=%hu";
const struct command_encoder command_encoder_83 PROGMEM = {83, 18, 3, cmd_para_c8_u32_u32};                             // messages_format[83] = "adxl345_start oid=%c start1_clock=%u start2_clock=%u";
const struct command_encoder command_encoder_84 PROGMEM = {84, 64, 3, cmd_para_c8_u16_ptb};                             // messages_format[84] = "adxl345_data oid=%c sequence=%hu data=%*s";
const struct command_encoder command_encoder_85 PROGMEM = {85, 64, 3, cmd_para_c8_c8_ptb};                              // messages_format[85] = "buttons_state oid=%c ack_count=%c state=%*s";
const struct command_encoder command_encoder_86 PROGMEM = {86, 64, 2, cmd_para_c8_ptb};                                 // messages_format[86] = "tmcuart_response oid=%c read=%*s";
const struct command_encoder command_encoder_87 PROGMEM = {87, 10, 2, cmd_para_c8_c8};                                  // messages_format[87] = "neopixel_result oid=%c success=%c";
const struct command_encoder command_encoder_88 PROGMEM = {88, 23, 4, cmd_para_c8_u32_u32_u32};                         // messages_format[88] = "counter_state oid=%c next_clock=%u count=%u count_clock=%u";
const struct command_encoder command_encoder_89 PROGMEM = {89, 64, 3, cmd_para_u32_u32_u16};                            // messages_format[89] = "set_pwm_out pin=%u cycle_ticks=%u value=%hu"
const struct command_encoder command_encoder_90 PROGMEM = {90, 64, 6, cmd_para_c8_u32_u32_u16_u16_u32};                 // messages_format[90] = "config_pwm_out oid=%c pin=%u cycle_ticks=%u value=%hu default_value=%hu max_duration=%u"
const struct command_encoder command_encoder_91 PROGMEM = {91, 64, 3, cmd_para_c8_u32_u16};                             // messages_format[91] = "queue_pwm_out oid=%c clock=%u value=%hu"
const struct command_encoder command_encoder_92 PROGMEM = {92, 64, 1, cmd_para_c8};                                     // messages_format[92] = "query_lis2dw_status oid=%c";
const struct command_encoder command_encoder_93 PROGMEM = {93, 64, 3, cmd_para_c8_u32_u32};                             // messages_format[93] = "query_lis2dw oid=%c clock=%u rest_ticks=%u";
const struct command_encoder command_encoder_94 PROGMEM = {94, 64, 2, cmd_para_c8_c8};                                  // messages_format[94] = "config_lis2dw oid=%c spi_oid=%c";
const struct command_encoder command_encoder_95 PROGMEM = {95, 64, 7, cmd_para_c8_u32_u32_u16_c8_c8_u16};               // messages_format[95] = "lis2dw_status oid=%c clock=%u query_ticks=%u next_sequence=%hu buffered=%c fifo=%c limit_count=%hu";
const struct command_encoder command_encoder_96 PROGMEM = {96, 64, 3, cmd_para_c8_u16_ptb};                             // messages_format[96] = "lis2dw_data oid=%c sequence=%hu data=%*s";
const struct command_encoder command_encoder_97 PROGMEM = {97, 38, 7, cmd_para_c8_u32_u32_u32_u32_u32_u32};             // messages_format[97] = "debug_dirzctl oid=%c arg[0]=%u arg[1]=%u arg[2]=%u arg[3]=%u arg[4]=%u arg[5]=%u"
const struct command_encoder command_encoder_98 PROGMEM = {98, 18, 3, cmd_para_c8_u32_u32};                             // messages_format[98] = "result_dirzctl oid=%c step=%u tick=%u"
const struct command_encoder command_encoder_99 PROGMEM = {99, 33, 6, cmd_para_c8_c8_u32_u32_u32_u32_u32};              // messages_format[99] = "fan_status oid=%c fan0_speed=%u fan1_speed=%u fan2_speed=%u fan3_speed=%u fan4_speed=%u"
const struct command_encoder command_encoder_100 PROGMEM = {100, 28, 5, cmd_para_c8_u32_u32_u32_u32};                   // messages_format[100] = "debug_hx711s oid=%c arg[0]=%u arg[1]=%u arg[2]=%u arg[3]=%u"
const struct command_encoder command_encoder_101 PROGMEM = {101, 64, 6, cmd_para_c8_c8_c8_u32_u32_u32};                 // messages_format[101] = "sg_resp oid=%c vd=%c it=%c nt=%u r=%u tt=%u"
const struct command_encoder command_encoder_102 PROGMEM = {102, 64, 2, cmd_para_c8_c8};                                // messages_format[102] = "config_dirzctl oid=%c z_count=%c"
const struct command_encoder command_encoder_103 PROGMEM = {103, 64, 6, cmd_para_c8_c8_u32_u32_c8_c8};                  // messages_format[103] = "add_dirzctl oid=%c index=%c dir_pin=%u step_pin=%u dir_invert=%c step_invert=%c"
const struct command_encoder command_encoder_104 PROGMEM = {104, 64, 4, cmd_para_c8_c8_u32_u32};                        // messages_format[104] = "run_dirzctl oid=%c direct=%c step_us=%u step_cnt=%u"
const struct command_encoder command_encoder_105 PROGMEM = {105, 64, 12, cmd_para_c8_c8_c8_c8_c8_c8_c8_c8_c8_c8_c8_c8}; // messages_format[105] = "config_fancheck oid=%c fan_num=%c fan0_pin=%c pull_up0=%c fan1_pin=%c pull_up1=%c fan2_pin=%c pull_up2=%c fan3_pin=%c pull_up3=%c fan4_pin=%c pull_up4=%c"
const struct command_encoder command_encoder_106 PROGMEM = {106, 64, 2, cmd_para_c8_c8};                                // messages_format[106] = "query_fancheck oid=%c which_fan=%c"
const struct command_encoder command_encoder_107 PROGMEM = {107, 90, 9, cmd_para_c8_c8_u32_u32_u32_u32_u32_u32_u32};    // messages_format[107] = "config_hx711s oid=%c hx711_count=%c channels=%u rest_ticks=%u kalman_q=%u kalman_r=%u max_th=%u min_th=%u k=%u"
const struct command_encoder command_encoder_108 PROGMEM = {108, 64, 4, cmd_para_c8_c8_u32_u32};                        // messages_format[108] = "add_hx711s oid=%c index=%c clk_pin=%u sdo_pin=%u"
const struct command_encoder command_encoder_109 PROGMEM = {109, 64, 2, cmd_para_c8_u16};                               // messages_format[109] = "query_hx711s oid=%c times_read=%hu"
const struct command_encoder command_encoder_110 PROGMEM = {110, 64, 4, cmd_para_c8_c8_u32_u32};                        // messages_format[110] = "config_hx711_sample oid=%c hx711_count=%c kalman_q=%u kalman_r=%u"
const struct command_encoder command_encoder_111 PROGMEM = {111, 64, 4, cmd_para_c8_c8_u32_u32};                        // messages_format[111] = "add_hx711_sample oid=%c index=%c clk_pin=%u sdo_pin=%u"
const struct command_encoder command_encoder_112 PROGMEM = {112, 64, 2, cmd_para_c8_u16};                               // messages_format[112] = "calibration_sample oid=%c times_read=%hu"
const struct command_encoder command_encoder_113 PROGMEM = {113, 64, 5, cmd_para_c8_i32_i32_i32_i32};                   // messages_format[113] = "sg_probe_check oid=%c x=%i y=%i z=%i cmd=%i"
const struct command_encoder command_encoder_114 PROGMEM = {114, 64, 11, cmd_para_c8_c8_c8_u32_u32_u32_u32_u32_u32_u32_u32};                   // messages_format[114] = "extruder_bootup_info oid=%c crash_flag=%c rest_cause=%c R0=%u R1=%u R2=%u R3=%u R12=%u LR=%u PC=%u xPSR=%u"


const struct command_encoder *ctr_lookup_encoder[] //-CMD-G-G-2022-07-21---
    {
        &command_encoder_0,   // msg_id=0,
        &command_encoder_1,   // msg_id=1
        &command_encoder_2,   // msg_id=2
        &command_encoder_3,   // msg_id=3
        &command_encoder_4,   // msg_id=4
        &command_encoder_5,   // msg_id=5
        &command_encoder_6,   // msg_id=6
        &command_encoder_7,   // msg_id=7
        &command_encoder_8,   // msg_id=8
        &command_encoder_9,   // msg_id=9
        &command_encoder_10,  // msg_id=10
        &command_encoder_11,  // msg_id=11
        &command_encoder_12,  // msg_id=12
        &command_encoder_13,  // msg_id=13
        &command_encoder_14,  // msg_id=14
        &command_encoder_15,  // msg_id=15
        &command_encoder_16,  // msg_id=16
        &command_encoder_17,  // msg_id=17
        &command_encoder_18,  // msg_id=18
        &command_encoder_19,  // msg_id=19
        &command_encoder_20,  // msg_id=20
        &command_encoder_21,  // msg_id=21
        &command_encoder_22,  // msg_id=22
        &command_encoder_23,  // msg_id=23
        &command_encoder_24,  // msg_id=24
        &command_encoder_25,  // msg_id=25
        &command_encoder_26,  // msg_id=26
        &command_encoder_27,  // msg_id=27
        &command_encoder_28,  // msg_id=28
        &command_encoder_29,  // msg_id=29
        &command_encoder_30,  // msg_id=30
        &command_encoder_31,  // msg_id=31
        &command_encoder_32,  // msg_id=32
        &command_encoder_33,  // msg_id=33
        &command_encoder_34,  // msg_id=34
        &command_encoder_35,  // msg_id=35
        &command_encoder_36,  // msg_id=36
        &command_encoder_37,  // msg_id=37
        &command_encoder_38,  // msg_id=38
        &command_encoder_39,  // msg_id=39
        &command_encoder_40,  // msg_id=40
        &command_encoder_41,  // msg_id=41
        &command_encoder_42,  // msg_id=42
        &command_encoder_43,  // msg_id=43
        &command_encoder_44,  // msg_id=44
        &command_encoder_45,  // msg_id=45
        &command_encoder_46,  // msg_id=46
        &command_encoder_47,  // msg_id=47
        &command_encoder_48,  // msg_id=48
        &command_encoder_49,  // msg_id=49
        &command_encoder_50,  // msg_id=50
        &command_encoder_51,  // msg_id=51
        &command_encoder_52,  // msg_id=52
        &command_encoder_53,  // msg_id=53
        &command_encoder_54,  // msg_id=54
        &command_encoder_55,  // msg_id=55
        &command_encoder_56,  // msg_id=56
        &command_encoder_57,  // msg_id=57
        &command_encoder_58,  // msg_id=58
        &command_encoder_59,  // msg_id=59
        &command_encoder_60,  // msg_id=60
        &command_encoder_61,  // msg_id=61
        &command_encoder_62,  // msg_id=62
        &command_encoder_63,  // msg_id=63
        &command_encoder_64,  // msg_id=64
        &command_encoder_65,  // msg_id=65
        &command_encoder_66,  // msg_id=66
        &command_encoder_67,  // msg_id=67
        &command_encoder_68,  // msg_id=68
        &command_encoder_69,  // msg_id=69
        &command_encoder_70,  // msg_id=70
        &command_encoder_71,  // msg_id=71
        &command_encoder_72,  // msg_id=72
        &command_encoder_73,  // msg_id=73
        &command_encoder_74,  // msg_id=74
        &command_encoder_75,  // msg_id=75
        &command_encoder_76,  // msg_id=76
        &command_encoder_77,  // msg_id=77
        &command_encoder_78,  // msg_id=78
        &command_encoder_79,  // msg_id=79
        &command_encoder_80,  // msg_id=80
        &command_encoder_81,  // msg_id=81
        &command_encoder_82,  // msg_id=82
        &command_encoder_83,  // msg_id=83
        &command_encoder_84,  // msg_id=84
        &command_encoder_85,  // msg_id=85
        &command_encoder_86,  // msg_id=86
        &command_encoder_87,  // msg_id=87
        &command_encoder_88,  // msg_id=88
        &command_encoder_89,  // msg_id=89
        &command_encoder_90,  // msg_id=90
        &command_encoder_91,  // msg_id=91
        &command_encoder_92,  // msg_id=92
        &command_encoder_93,  // msg_id=93
        &command_encoder_94,  // msg_id=94
        &command_encoder_95,  // msg_id=95
        &command_encoder_96,  // msg_id=96
        &command_encoder_97,  // msg_id=97
        &command_encoder_98,  // msg_id=98
        &command_encoder_99,  // msg_id=99
        &command_encoder_100, // msg_id=100
        &command_encoder_101, // msg_id=101
        &command_encoder_102, // msg_id=102
        &command_encoder_103, // msg_id=103
        &command_encoder_104, // msg_id=104
        &command_encoder_105, // msg_id=105
        &command_encoder_106, // msg_id=106
        &command_encoder_107, // msg_id=107
        &command_encoder_108, // msg_id=108
        &command_encoder_109, // msg_id=109
        &command_encoder_110, // msg_id=110
        &command_encoder_111, // msg_id=111
        &command_encoder_112, // msg_id=112
        &command_encoder_113, // msg_id=113
        &command_encoder_114, // msg_id=114
    };

void MessageParser::encode_int(std::vector<uint8_t> &out, uint32_t v)
{
    int32_t sv = v;
    if (sv < (3L << 5) && sv >= -(1L << 5))
        goto f4; //[-20,60)                          [0-60)                                      [ffff ffe0 ffff ffff]
    if (sv < (3L << 12) && sv >= -(1L << 12))
        goto f3; //[-1000,3000)                   [60-3000)                               [ffff f000 ffff ffe0)
    if (sv < (3L << 19) && sv >= -(1L << 19))
        goto f2; //[-8 0000,18 0000)            [3000-18 0000)                  [fff8 0000 ffff f000)
    if (sv < (3L << 26) && sv >= -(1L << 26))
        goto f1;                     //[-400 0000,c00 0000)     [18 0000-c00 0000)           [fc00 0000 fff8 0000)
    out.push_back((v >> 28) | 0x80); // 31-28
f1:
    out.push_back((v >> 21) & 0x7f | 0x80); // 27-21
f2:
    out.push_back((v >> 14) & 0x7f | 0x80); // 20-14
f3:
    out.push_back((v >> 7) & 0x7f | 0x80); // 13-7
f4:
    out.push_back((v) & 0x7f); // 6-0

    // printf("-gggg--%x--%x----%d--%d----",sv,v,sv,v );

    // if(v >= 0xc000000 || v < -0x4000000)
    //     out.push_back((v>>28) & 0x7f | 0x80);       //31-28
    // if(v >= 0x180000 || v < -0x80000)
    //     out.push_back((v>>21) & 0x7f | 0x80);     //27-21
    // if(v >= 0x3000 || v < -0x1000)
    //     out.push_back((v>>14) & 0x7f | 0x80);  //20-14
    // if(v >= 0x60 || v < -0x20)
    //     out.push_back((v>>7) & 0x7f | 0x80);    //13-7
    // out.push_back(v & 0x7f);  //6-0
}

void encode_str(std::vector<uint8_t> &out, std::string v)
{
    out.push_back(v.length());
}

std::vector<uint8_t> MessageParser::create_command(std::string msg, std::map<std::string, int> message_by_name)
{
    std::vector<Msg> items; // = parse_msg(msg);
    std::vector<std::string> argparts;
    std::string name;
    // send config:config_lis2dw oid=2 spi_oid=1
    // printf("create_command msg:%s\n", msg.c_str());
    while (1)
    {
        int pos = msg.find(' ');
        if (pos > msg.size())
        {
            name = msg;
            argparts.push_back(msg);
            break;
        }
        argparts.push_back(msg.substr(0, pos));
        msg = msg.substr(pos + 1);
    }
    name = argparts[0];
    // printf("create_command name:%s size %d\n", name.c_str(), argparts.size());

    uint8_t value = message_by_name[name];

    const struct command_encoder *command_encoder = ctr_lookup_encoder[value];

    if (argparts.size() > (1 + command_encoder->num_params)) // 有多余的空格
    {
        std::string msgformat = Printer::GetInstance()->m_mcu->m_serial->m_msgparser->m_id_to_format.at(value);
        std::vector<std::string> msgformat_parts = split(msgformat, " ");
        // printf("-gggg--%s--%d---%d----%d----\n",name.c_str(),value,argparts.size(),msgformat_parts.size());
        int j = 1;
        int pos1 = 0;
        Msg last;
        last.value = "";
        for (int i = 1; i < msgformat_parts.size(); i++)
        {
            std::string temp_name;
            int pos = msgformat_parts[i].find('=');
            if (pos > msgformat_parts[i].size())
            {
                printf("-!!1errer----%d--%d----\n", pos, msgformat_parts[i].size());
                continue;
            }
            temp_name = msgformat_parts[i].substr(0, pos);
            while (j < argparts.size())
            {
                pos1 = argparts[j].find('=');
                if (pos1 <= argparts[j].size())
                {
                    if (temp_name == argparts[j].substr(0, pos1))
                    {
                        break;
                    }
                }
                last.value = last.value + " " + argparts[j];
                // printf("-gggg---last.value.length-%d-%d--%d------\n",j,last.value.length(),argparts[j].length() );
                j++;
            }
            if (i > 1)
            {
                items.push_back(last);
                // printf("-gggg---last.value.length--%d-------\n",last.value.length() );
            }

            last.name = temp_name;
            if (j < argparts.size()) // 满足这个条件，pos1范围就正常
                last.value = argparts[j].substr(pos1 + 1);
            else
            {
                last.value = "";
            }
            j++;
        }
        for (; j < argparts.size(); j++)
        {
            last.value = last.value + " " + argparts[j];
        }
        items.push_back(last);
        // printf("-gggg---last.value.length--%d-------\n",last.value.length() );
    }
    else
    { // 没有多余的空格，即便有多余的 = 也没关系，以空格后的第一个=为准
        for (int i = 1; i < argparts.size(); i++)
        {
            Msg temp;
            int pos = argparts[i].find('=');
            if (pos > argparts[i].size())
            {
                continue;
            }
            temp.name = argparts[i].substr(0, pos);
            temp.value = argparts[i].substr(pos + 1);
            items.push_back(temp);
        }
    }

    std::vector<uint8_t> out;

    if ((command_encoder == NULL))
    {
        printf("-gggg--%s--%d------\n", name.c_str(), value);
        return out;
    }
    if ((items.size()) && ((items.size() != command_encoder->num_params)))
    {
        printf("-gggg--%s--%d---%d----\n", name.c_str(), value, items.size());
        return out;
    }

    out.push_back(value);

    for (int i = 0; i < items.size(); i++)
    {
        uint8_t t = command_encoder->param_types[i];
        switch (t)
        {
        case PT_uint32:
        case PT_int32:
        case PT_uint16:
        case PT_int16:
        case PT_byte:
        {
            uint64_t num = 0;
            if (items[i].value.length() > 6)
            {
                std::string first = items[i].value.substr(0, items[i].value.length() - 6);
                std::string second = items[i].value.substr(items[i].value.length() - 6, items[i].value.length());
                uint64_t num1 = strtoul(first.c_str(), NULL, 0);
                uint64_t num2 = strtoul(second.c_str(), NULL, 0);
                num = (num1 * 1e6 + num2);
            }
            else
            {
                num = strtoul(items[i].value.c_str(), NULL, 0);
            }

            if (num == 0 && items[i].value != "0")
            {
                num = m_pinMap[items[i].value];
            }
            encode_int(out, num);
            break;
        }
        case PT_string:
        case PT_progmem_buffer:
        case PT_buffer:
        {
            uint8_t count = items[i].value.length();
            // printf("PT_buffer count = %x\n", count);
            uint8_t *s = (uint8_t *)items[i].value.c_str();
            out.push_back(count);
            while (count)
            {
                // printf("PT_buffer data = %x\n", *s);
                out.push_back(*s++);
                count--;
            }
            break;
        }
        // case PT_progmem_buffer:
        // case PT_buffer:
        // {
        //     v = va_arg(args, int);
        //     if (v > maxend-p)
        //         v = maxend-p;
        //     out.push_back(v);
        //     uint8_t *s = va_arg(args, uint8_t*);
        //     if (t == PT_progmem_buffer)
        //         memcpy_P(p, s, v);
        //     else
        //         memcpy(p, s, v);
        //     p += v;
        //     break;
        // }
        default:
            printf("-gggg--%s--%d----%d-----\n", name.c_str(), i, t);
        }
    }
    return out;
}
