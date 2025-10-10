/**
 * @file hx711.cpp
 * @author
 * @brief
 * @version 0.1
 * @date 2023-12-05
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "hx711s.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "usb_uart.h"
#include <unistd.h>
#include "klippy.h"
#include "hl_tpool.h"
#include "hl_assert.h"
#define LOG_TAG "HX711"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

#pragma pack(1)
struct hx711s_data_t
{
    uint8_t buf[28 + 2];
    int32_t header;
    uint32_t now_inter_ms;
    uint32_t now_tick;
    int32_t hx711s_outvals[3];
    uint32_t count;
};

#pragma pack()

static struct hx711s_data_t hx711s_data;
pthread_t usb_uart_process_tid;
static pthread_mutex_t ptlock;

// void *usb_uart_process(void *arg)
// {
//     usb_uart_config("/dev/ttyUSB0", 115200);
//     // std::cout << "---------------usb_uart_process  start" << std::endl;
//     // time_t rawtime;
//     // struct tm *info;
//     // char buffer[80];
//     // time(&rawtime);
//     // info = localtime(&rawtime);
//     // strftime(buffer, 80, "%Y%m%d_%H%M%S", info);
//     // printf("格式化的日期 & 时间 : |%s|\n", buffer);
//     // std::stringstream time;
//     // time << buffer;

//     // std::string fileName = "../nfs/XY_" + time.str() + ".csv";

//     // ofstream outFile;
//     // outFile.open(fileName, ios::out);
//     // for (int i = 1; i <= 3; i++)
//     static double last_read_time = 0;
//     while (1)
//     {
//         // while()
//         // {
//         //     usleep(1000);
//         // }
//         if (Printer::GetInstance()->m_hx711s->m_times_read == 0 || Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_need_wait)
//         {
//             usleep(100);
//         }
//         else
//         {
//             // double now = get_monotonic();
//             // std::cout << "read time : " << now - last_read_time << std::endl;
//             // last_read_time = now;
//             // std::cout << "read !!! " << std::endl;
//             pthread_mutex_lock(&ptlock);
//             // std::cout << "ptlock !!! " << std::endl;
//             usb_uart_recvdats(hx711s_data.buf, sizeof(hx711s_data.buf));
//             // std::cout << "usb_uart_recvdats !!! " << std::endl;
//             // printf("data_count = %d.\n",hx711s_data.count);
//             for (uint8_t i = 0; i < sizeof(hx711s_data.buf) - 3; i++)
//             {
//                 /* code */
//                 if (hx711s_data.buf[i] == 0xDD && hx711s_data.buf[i + 1] == 0xCC && hx711s_data.buf[i + 2] == 0xBB && hx711s_data.buf[i + 3] == 0xAA)
//                 {
//                     // printf("sizeof(buf) = %d.\n",sizeof(hx711s_data.buf));
//                     // printf(">>>>>>%x,%x,%x,%x\n",hx711s_data.buf[i+0],hx711s_data.buf[i+1],hx711s_data.buf[i+2],hx711s_data.buf[i+3]);
//                     // printf(">>>>>>%x,%x,%x,%x\n",hx711s_data.buf[i+4],hx711s_data.buf[i+5],hx711s_data.buf[i+6],hx711s_data.buf[i+7]);
//                     // printf(">>>>>>%x,%x,%x,%x\n",hx711s_data.buf[i+8],hx711s_data.buf[i+9],hx711s_data.buf[i+10],hx711s_data.buf[i+11]);
//                     // printf(">>>>>>%x,%x,%x,%x\n",hx711s_data.buf[i+12],hx711s_data.buf[i+13],hx711s_data.buf[i+14],hx711s_data.buf[i+15]);
//                     // printf(">>>>>>%x,%x,%x,%x\n",hx711s_data.buf[i+16],hx711s_data.buf[i+17],hx711s_data.buf[i+18],hx711s_data.buf[i+19]);
//                     // printf(">>>>>>%x,%x,%x,%x\n",hx711s_data.buf[i+20],hx711s_data.buf[i+21],hx711s_data.buf[i+22],hx711s_data.buf[i+23]);
//                     // printf(">>>>>>%x,%x,%x,%x\n",hx711s_data.buf[i+24],hx711s_data.buf[i+25],hx711s_data.buf[i+26],hx711s_data.buf[i+27]);
//                     // printf(">>>>>>%x,%x,%x,%x\n",hx711s_data.buf[i+28],hx711s_data.buf[i+29],hx711s_data.buf[i+30],hx711s_data.buf[i+31]);
//                     // printf(">>>>>>%x,%x\n",hx711s_data.buf[i+32],hx711s_data.buf[i+33]);

//                     hx711s_data.header = hx711s_data.buf[i + 3] << 24 | hx711s_data.buf[i + 2] << 16 | hx711s_data.buf[i + 1] << 8 | hx711s_data.buf[i];
//                     hx711s_data.now_inter_ms = hx711s_data.buf[i + 7] << 24 | hx711s_data.buf[i + 6] << 16 | hx711s_data.buf[i + 5] << 8 | hx711s_data.buf[i + 4];
//                     hx711s_data.now_tick = hx711s_data.buf[i + 11] << 24 | hx711s_data.buf[i + 10] << 16 | hx711s_data.buf[i + 9] << 8 | hx711s_data.buf[i + 8];
//                     for (uint8_t j = i + 12; j < sizeof(hx711s_data.buf) - 5; j += 4)
//                     {
//                         // uint8_t转uint32_t
//                         hx711s_data.hx711s_outvals[(j - i - 12) / 4] = hx711s_data.buf[j + 3] << 24 | hx711s_data.buf[j + 2] << 16 | hx711s_data.buf[j + 1] << 8 | hx711s_data.buf[j];
//                         // printf("hx711s_outvals[%d] = %x\n",(j- i - 12)/4,hx711s_data.hx711s_outvals[(j - i - 12)/4]);
//                     }
//                     usb_uart_buf_clear();
//                     memset(hx711s_data.buf, 0, sizeof(hx711s_data.buf));
//                     break;
//                 }
//             }
//             hx711s_data.count++;
//             Printer::GetInstance()->m_hx711s->m_times_read--;

//             strain_gauge_params data;
//             // data.hx711s_outvals[0] = hx711s_data.hx711s_outvals[0];
//             // data.hx711s_outvals[1] = hx711s_data.hx711s_outvals[1];
//             // data.hx711s_outvals[2] = hx711s_data.hx711s_outvals[2];
//             // data.hx711s_outvals[3] = hx711s_data.hx711s_outvals[3];
//             data.hx711s_outvals[0] = hx711s_data.hx711s_outvals[1];
//             data.hx711s_outvals[1] = hx711s_data.hx711s_outvals[2];
//             data.hx711s_outvals[2] = hx711s_data.hx711s_outvals[3];
//             data.now_inter_ms = hx711s_data.now_inter_ms;
//             data.now_tick = hx711s_data.now_tick;
//             data.time = get_monotonic() - 0.016;
//             if (abs(data.hx711s_outvals[0]) < 200 || abs(data.hx711s_outvals[1]) < 200 || abs(data.hx711s_outvals[2]) < 200 || abs(data.hx711s_outvals[0] + 65536) < 500 || abs(data.hx711s_outvals[1] + 65536) < 500 || abs(data.hx711s_outvals[2] + 65536) < 500)
//             {
//                 hx711s_data.count--;
//                 Printer::GetInstance()->m_hx711s->m_times_read++;
//                 usleep(15000); // 15ms接收一次
//                 pthread_mutex_unlock(&ptlock);
//                 continue;
//             }
//             if (Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[0].size())
//             {
//                 if (abs(data.hx711s_outvals[0] - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[0].back() - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_base_avgs[0]) > 100000 || abs(data.hx711s_outvals[1] - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[1].back() - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_base_avgs[1]) > 100000 || abs(data.hx711s_outvals[2] - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[2].back() - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_base_avgs[2]) > 100000)
//                 {
//                     data.hx711s_outvals[0] = Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[0].back() + Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_base_avgs[0];
//                     data.hx711s_outvals[1] = Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[1].back() + Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_base_avgs[1];
//                     data.hx711s_outvals[2] = Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[2].back() + Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_base_avgs[2];
//                     // std::cout << "data.hx711s_outvals[0] : " << data.hx711s_outvals[0] << std::endl;
//                     // std::cout << "m_all_vals[0].back() : " << Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[0].back() << std::endl;
//                     // std::cout << "data.hx711s_outvals[1] : " << data.hx711s_outvals[1] << std::endl;
//                     // std::cout << "m_all_vals[1].back() : " << Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[1].back() << std::endl;
//                     // std::cout << "data.hx711s_outvals[2] : " << data.hx711s_outvals[2] << std::endl;
//                     // std::cout << "m_all_vals[2].back() : " << Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[2].back() << std::endl;
//                 }
//             }
//             // add by huangyijunj 20231107
//             //-------------------------------------------------------------------------------
//             if (0)
//             {

//                 {
//                     // outFile << data.hx711s_outvals[0] << "," << data.hx711s_outvals[1] << ",";
//                     // outFile << data.hx711s_outvals[2] << "\n";
//                 }
//             }
//             Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.push_back(data);
//             for (int j = 0; j < Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_s_count; j++)
//             {
//                 // if (Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[j].size() > 16)
//                 // {
//                 //     if (fabs(data.hx711s_outvals[j] - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_base_avgs[j] - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[j].back()) > 100000)
//                 //     {
//                 //         std::cout << "data.hx711s_outvals[j] : " << data.hx711s_outvals[j] << std::endl;
//                 //         data.hx711s_outvals[j] = Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[j].back();
//                 //         std::cout << "data.hx711s_outvals[j] : " << data.hx711s_outvals[j] << std::endl;
//                 //     }
//                 // }
//                 Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[j].push_back(data.hx711s_outvals[j] - Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_base_avgs[j]);
//                 // std::cout << "hx711s_outvals[" << j << "]:" << data.hx711s_outvals[j] << std::endl;
//             }

//             if (Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.size() > Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_pi_count)
//             {
//                 // std::cout << "hx711s_outvals.size():" << Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.size() << std::endl;
//                 Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.erase(Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.begin());
//                 for (int i = 0; i < Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals.size(); i++)
//                 {
//                     Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[i].erase(Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals[i].begin());
//                 }
//                 // std::reverse(Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.begin(), Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.end());
//                 // std::reverse(Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals.begin(), Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals.end());
//                 // Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.pop_back();
//                 // Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals.pop_back();
//                 // std::reverse(Printer::GetInstance()->m_strain_gauge->m_ob                                                                                                                                               j->m_hx711s->m_all_params.begin(), Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_params.end());
//                 // std::reverse(Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals.begin(), Printer::GetInstance()->m_strain_gauge->m_obj->m_hx711s->m_all_vals.end());
//             }
//             usleep(15000); // 15ms接收一次
//             pthread_mutex_unlock(&ptlock);
//         }
//     }
//     // outFile.close();
// }

// void usb_uart_init(void)
// {
//     pthread_create(&usb_uart_process_tid, NULL, usb_uart_process, NULL);
//     pthread_mutex_init(&ptlock, NULL);
// }

// int32_t *get_hx711_val(void)
// {
//     return hx711s_data.hx711s_outvals;
// }

HX711S::HX711S(std::string section_name)
{
    m_enable_test = Printer::GetInstance()->m_pconfig->GetInt(section_name, "enable_test", 0);
    m_s_count = Printer::GetInstance()->m_pconfig->GetInt(section_name, "count", 3, 1, 4);
    m_rest_ticks = Printer::GetInstance()->m_pconfig->GetInt(section_name, "rest_ticks", 5000, 100, 1500000);
    m_install_dir = Printer::GetInstance()->m_pconfig->GetInt(section_name, "install_dir", 15, 0, 15);
    m_enable_hpf = Printer::GetInstance()->m_pconfig->GetInt(section_name, "enable_hpf", 1, 0, 15);
    m_find_index_mode = Printer::GetInstance()->m_pconfig->GetInt(section_name, "find_index_mode", 1, 0, 15);
    m_enable_shake_filter = Printer::GetInstance()->m_pconfig->GetInt(section_name, "enable_shake_filter", 1, 0, 1);
    m_enable_channels = Printer::GetInstance()->m_pconfig->GetInt(section_name, "enable_channels", 31, 0, 31);
    m_sg_mode = Printer::GetInstance()->m_pconfig->GetInt(section_name, "sg_mode", 4, 0, 16);
    m_th_k = Printer::GetInstance()->m_pconfig->GetInt(section_name, "th_k", 2000, -6000, 6000);   
    m_k_slope = Printer::GetInstance()->m_pconfig->GetInt(section_name, "k_slope", 1000, -10000, 10000);   
    m_bias_slope = Printer::GetInstance()->m_pconfig->GetInt(section_name, "bias_slope", 120, -10000, 10000);
       
    
    m_kalman_q = Printer::GetInstance()->m_pconfig->GetInt(section_name, "kalman_q", 1, -100000, 100000);
    m_kalman_r = Printer::GetInstance()->m_pconfig->GetInt(section_name, "kalman_r", 10, -100000, 100000);
    m_min_th = Printer::GetInstance()->m_pconfig->GetInt(section_name, "min_th", 2000, -20000, 20000);
    m_max_th = Printer::GetInstance()->m_pconfig->GetInt(section_name, "max_th", 6000, -60000, 60000);

    
    std::vector<double>(m_s_count, 0).swap(m_base_avgs);
    std::vector<std::vector<double>>(m_s_count, std::vector<double>()).swap(m_all_vals);
    m_del_dirty = false;
    m_index_dirty = 0;
    m_start_tick = 0;
    m_trigger_tick = 0;
    m_need_wait = false;
    for (int i = 0; i < m_s_count; i++)
    {
        stringstream clk_pin;
        stringstream sdo_pin;
        clk_pin << "sensor" << i << "_clk_pin";
        sdo_pin << "sensor" << i << "_sdo_pin";
        m_s_clk_pin.push_back(Printer::GetInstance()->m_pconfig->GetString(section_name, clk_pin.str()));
        m_s_sdo_pin.push_back(Printer::GetInstance()->m_pconfig->GetString(section_name, sdo_pin.str()));
    }
    m_mcu = get_printer_mcu(Printer::GetInstance(), Printer::GetInstance()->m_pconfig->GetString(section_name, "use_mcu"));
    m_oid = m_mcu->create_oid();
    m_mcu->register_config_callback(std::bind(&HX711S::_build_config, this, std::placeholders::_1));
    m_mcu->register_response(std::bind(&HX711S::_handle_debug_hx711s, this, std::placeholders::_1), "debug_hx711s", m_oid);
    m_mcu->register_response(std::bind(&HX711S::_handle_sg_resp, this, std::placeholders::_1), "sg_resp", m_oid);
    Printer::GetInstance()->register_event_handler("klippy:connect" + section_name, std::bind(&HX711S::_handle_mcu_identify, this));
    Printer::GetInstance()->register_event_handler("klippy:shutdown" + section_name, std::bind(&HX711S::_handle_shutdown, this));
    Printer::GetInstance()->register_event_handler("klippy:disconnect" + section_name, std::bind(&HX711S::_handle_disconnect, this));
    std::string cmd_READ_HX711_help = "Read hx711s vals";
    Printer::GetInstance()->m_gcode->register_command("READ_HX711", std::bind(&HX711S::cmd_READ_HX711, this, std::placeholders::_1), false, cmd_READ_HX711_help);
    m_queue = m_mcu->alloc_command_queue();
    // LOG_I("Instantiation StrainGaugeKalmanFilter\n");
    // strain_gauge_kalman_filter_ = std::make_shared<StrainGaugeKalmanFilter>();
    m_pi_count = int(0);
    m_show_msg = false;
    m_times_read = 0;
    m_mcu_freq = 84000000;
    m_is_shutdown = false;
    m_is_timeout = false;
    m_is_calibration = 0;
    m_is_trigger = 0;
}

HX711S::~HX711S()
{
}

void HX711S::_build_config(int para)
{
    if (para & 1)
    {
        int hx711_count = ((m_install_dir&0x0F) << 4) | (m_s_count&0x0F);                          // 低四位数量， 高四位安装方向
        uint32_t rest_ticks = ((m_enable_test&0x0F) << 28) | ((m_sg_mode&0x0F)<<24) | (m_rest_ticks&0xFFFFFF);
        m_enable_channels =  ((m_enable_shake_filter & 0x0F) << 16)| ((m_find_index_mode & 0x0F) << 12) | ((m_enable_hpf & 0x0F) << 8) |   (m_enable_channels & 0xFF);                         // 低八位通道，9-12位： 使能高通滤波  13-16: 回退点模式 17-20: 抖动滤波使能
        std::cout << "hx711_count:"<< hx711_count << " install_dir:" << m_install_dir<< " enable_channels:" << m_enable_channels << " rest_ticks:" << m_rest_ticks << " m_sg_mode:" << m_sg_mode << " m_enable_test:" << m_enable_test<< std::endl; // << " merge_rest_ticks:" << rest_ticks << std::endl;
        stringstream config_hx711s;
        
        int th_k_field = ((m_k_slope) << 16) | ((m_bias_slope & 0xFF) << 8) |   (m_th_k & 0xFF); 
        config_hx711s << "config_hx711s oid=" << m_oid << " hx711_count=" << hx711_count << " channels=" << m_enable_channels << " rest_ticks=" << rest_ticks << " kalman_q=" << m_kalman_q << " kalman_r=" << m_kalman_r << " max_th=" << m_max_th<< " min_th=" << m_min_th << " k="<< th_k_field;
        m_mcu->add_config_cmd(config_hx711s.str());

        std::cout << "**************config_hx711s:" << config_hx711s.str() << std::endl;
    
        for (int i = 0; i < m_s_count; i++)
        {
            pinParams *clk_pin_params = Printer::GetInstance()->m_ppins->lookup_pin(m_s_clk_pin[i]);
            pinParams *sdo_pin_params = Printer::GetInstance()->m_ppins->lookup_pin(m_s_sdo_pin[i]);
            stringstream add_hx711s;
            add_hx711s << "add_hx711s oid=" << m_oid << " index=" << i << " clk_pin=" << clk_pin_params->pin << " sdo_pin=" << sdo_pin_params->pin;
            m_mcu->add_config_cmd(add_hx711s.str());
            // usleep(1000*1000);
        }
        m_filter = (Filter *)Printer::GetInstance()->lookup_object("filter");
    }
    // m_mcu = m_mcu->get_constant_float("CLOCK_FREQ");
}

void HX711S::_handle_mcu_identify()
{
}

void HX711S::_handle_debug_hx711s(ParseResult &params)
{  
    m_mutex.lock(); 
    uint32_t oid = params.PT_uint32_outs.at("oid");
    uint32_t arg0 = params.PT_uint32_outs.at("arg[0]");
    uint32_t arg1 = params.PT_uint32_outs.at("arg[1]");
    m_probe_calibration_response = arg0;
    m_probe_cmd_response = arg1;
    LOG_I("sg cmd response, m_probe_calibration_response:%d m_probe_cmd_response:%d\n", (uint32_t)m_probe_calibration_response, (uint32_t)m_probe_cmd_response);
    m_mutex.unlock();
}

void HX711S::_handle_shutdown()
{
    m_is_shutdown = true;
}

void HX711S::_handle_disconnect()
{
    m_is_timeout = true;
}

#if ENABLE_MANUTEST
extern int sg_triggered;
#endif

void HX711S::_handle_sg_resp(ParseResult &params)
{
    static uint32_t loop = 0; 
    static double last_time_timestamp = 0; 
    m_mutex.lock();
    // 触发的时刻
    uint32_t it = params.PT_uint32_outs.at("it");
    uint32_t trigger_index = it;
    m_trigger_tick = static_cast<uint64_t>(params.PT_uint32_outs.at("nt"));     //下位机触发晶振时钟计数
    m_time_tick = static_cast<uint64_t>(params.PT_uint32_outs.at("tt"));        //下位机实时晶振时钟计数
    m_time_timestamp = m_mcu->clock_to_print_time(m_mcu->clock32_to_clock64(m_time_tick));
    m_trigger_timestamp = m_mcu->clock_to_print_time(m_mcu->clock32_to_clock64(m_trigger_tick));
    double sg_sensor_estimated_time = m_mcu->m_clocksync->estimated_print_time(get_monotonic());
    LOG_D("sg_sensor_estimated_time:%f m_time_timestamp:%f m_time_tick:%d\n", sg_sensor_estimated_time, m_time_timestamp, m_time_tick);
    LOG_D("m_trigger_timestamp:%f %d\n", m_trigger_timestamp , m_trigger_tick);

    double dirzctl_estimated_time = Printer::GetInstance()->m_strain_gauge->m_obj->m_dirzctl->m_mcu->m_clocksync->estimated_print_time(get_monotonic());
    double delta_estimated_time = dirzctl_estimated_time - sg_sensor_estimated_time;
    LOG_D("z轴当前预估时间:dirzctl_estimated_time:%f delta_estimated_time:%f\n", dirzctl_estimated_time,delta_estimated_time);
    if (std::fabs(delta_estimated_time) > 0.01) {
        LOG_W("time sync warnning: delta_estimated_time:%f\n",delta_estimated_time);
    }

    // 标定结果： bit  7      6     5     4        3              2            1             0
    //             标定0k                     sensor4 err   sensor3 err   sensor2 err   sensor1 err
    m_is_calibration = static_cast<uint8_t>(params.PT_uint32_outs.at("vd"));
    // if ( m_is_calibration & 0x01) {
    //     LOG_E("strain gauge 1 err\n");
    // }
    // if ( m_is_calibration & 0x02) {
    //     LOG_E("strain gauge 2 err\n");
    // }
    // if ( m_is_calibration & 0x04) {
    //     LOG_E("strain gauge 3 err\n");
    // }
    // if ( m_is_calibration & 0x08) {
    //     LOG_E("strain gauge 4 err\n");
    // }

    // 触发结果： >0 时数据有效
    m_is_trigger = static_cast<uint8_t>(params.PT_uint32_outs.at("r"));
    if (loop%1==0) {    
        std::cout << "sg_resp loop:"<< loop << " calibra:" << (int32_t)m_is_calibration <<  " trigger:" << (uint32_t)m_is_trigger  << std::endl;    
    }
    last_time_timestamp = m_time_timestamp;
    if (m_is_trigger > 0) {
        if (m_is_trigger&0x80) {
            uint32_t hx711_index = m_is_trigger&0x1F;
            //TODO: 应变片安装反向
            std::cout << "........please check "<< hx711_index << "sg install dirrection........." << std::endl;
#if ENABLE_MANUTEST
            sg_triggered = 0;
        } else {
            sg_triggered = 1;
#endif
        }
        LOG_I("........strain gauge TRIGGER: %02x trigger_index:%d at:%f\n", (uint32_t)m_is_trigger,trigger_index,get_monotonic()); 
    }
#if ENABLE_MANUTEST
    else
    {
        sg_triggered = 0;
        std::cout << "m_is_trigger <= 0 " << std::endl;
    }
#endif
    loop++;
    m_mutex.unlock();
}


std::vector<ParseResult> HX711S::get_params()
{
    m_mutex.lock();
    // m_need_wait = true;
    std::vector<ParseResult> tmps = m_all_params;
    // m_need_wait = false;
    m_mutex.unlock();
    return tmps;
}

std::vector<std::vector<double>> HX711S::get_vals()
{
    // m_need_wait = true;
    m_mutex.lock();
    std::vector<std::vector<double>> tmps(m_s_count, std::vector<double>());
    // std::cout << "m_all_vals size : " << m_all_vals.size() << std::endl;
    if (m_all_vals.size() == 0)
    {
        // m_need_wait = false;
        m_mutex.unlock();
        return tmps;
    }
    for (int i = 0; i < m_s_count; i++)
    {
        for (int j = 0; j < m_all_vals[i].size(); j++)
        {
            tmps[i].push_back(m_all_vals[i][j]);
        }
    }
    // m_need_wait = false;
    m_mutex.unlock();
    return tmps;
}


std::vector<double> HX711S::get_vals_new()
{
    // m_need_wait = true;
    m_mutex.lock();
    std::vector<double> tmps;
    // std::cout << "m_all_vals size : " << m_all_vals.size() << std::endl;
    if (m_all_vals.size() == 0)
    {
        // m_need_wait = false;
        m_mutex.unlock();
        return tmps;
    }
    for (int j = 0; j < m_fusion_vals.size(); j++)
    {
        tmps.push_back(m_fusion_vals[j]);
    }
    
    // m_need_wait = false;
    m_mutex.unlock();
    return tmps;
}

void HX711S::delay_s(double delay_s)
{
    // double eventtime = get_monotonic();
    // if (!Printer::GetInstance()->is_shutdown())
    // {
    //     Printer::GetInstance()->m_tool_head->get_last_move_time();
    //     eventtime = Printer::GetInstance()->m_reactor->pause(eventtime + delay_s);
    // }
}

void HX711S::query_start(int pi_count, int cycle_count, bool del_dirty, bool show_msg, bool is_ck_con)
{
    if (m_is_shutdown || m_is_timeout)
    {
        return;
    }

    if (cycle_count != 0)
    {
        m_mutex.lock();
        m_pi_count = pi_count;
        m_show_msg = show_msg;
        m_del_dirty = del_dirty;
        m_index_dirty = 0;
        std::vector<ParseResult>().swap(m_all_params);
        std::vector<std::vector<double>>(m_s_count, std::vector<double>()).swap(m_all_vals);
        m_fusion_vals.clear();
        m_mutex.unlock();
    }
    // m_cycle_count = cycle_count;
    // m_times_read = m_cycle_count;
    // HL_ASSERT(hl_tpool_create_thread(&usb_uart_process_tid, usb_uart_process, NULL, sizeof(disk_thread_msg_t), 32, 0, 0) == 0);
    // HL_ASSERT(hl_tpool_wait_started(usb_uart_process_tid, 0) == 1);
    // send_query_cmd(touch->query_cmd, touch->oid, cycle_count);
    stringstream query_hx711s;
    query_hx711s << "query_hx711s oid=" << m_oid << " times_read=" << cycle_count;
    m_mcu->m_serial->send(query_hx711s.str(), 0, 0, m_queue);
}

void HX711S::ProbeCheckTriggerStart(int x, int y, int z, bool ack)
{
    usleep(500);
    if (m_is_shutdown || m_is_timeout || !(m_is_calibration&0x80))
    {
        LOG_E("ProbeCheckTriggerStart Error: m_is_shutdown(%d) or m_is_timeout(%d) or m_is_calibration(%d)\n", (int32_t)m_is_shutdown, (int32_t)m_is_timeout, (uint32_t)m_is_calibration);
        return;
    }
    m_is_trigger = 0;
    m_trigger_timestamp = 0.f;
    m_probe_cmd_response = 0;
    // register_response
    // m_mcu->m_serial->send_with_response
    LOG_I("ProbeCheckTriggerStart(%d,%d,%d.%d) at:%f\n",x,y,z,1,get_monotonic());
    stringstream sg_probe_check;
    sg_probe_check << "sg_probe_check oid=" << m_oid << " x=" << x << " y=" << y << " z=" << z << " cmd=" << 1;
    m_mcu->m_serial->send(sg_probe_check.str(), 0, 0, m_queue);
    uint32_t wait_loop = 0;
    // 避免上一天消息导致的提前检测
    usleep(5 * 1000);
    int cnt = 0;
    int time_out = 50;
    while (1)
    {
        wait_loop++;
        if ( wait_loop % 2345 == 0 ) { 
            // 重发命令
            LOG_I("ProbeCheckTriggerStart wait cmd timeout resend cmd:%d\n",wait_loop);
            m_mcu->m_serial->send(sg_probe_check.str(), 0, 0, m_queue);
            cnt++;
            if (cnt > time_out)
            {
                throw MCUException(m_mcu->m_serial->m_name, "No response from MCU");
            }
        }      
        if (m_probe_cmd_response == 1){
            LOG_I("ProbeCheckTriggerStart cmd recevice response at: %f\n",get_monotonic());
            break;
        }
        usleep(1000);
    }

}


void HX711S::ProbeCheckTriggerStop(bool ack)
{
    if (m_is_shutdown || m_is_timeout)
    {
        LOG_E("ProbeCheckTriggerStop Error: m_is_shutdown(%d) or m_is_timeout(%d)\n", (int32_t)m_is_shutdown, (int32_t)m_is_timeout);
        return;
    }
    m_probe_cmd_response = 0;
    m_is_trigger = 0;
    LOG_I("ProbeCheckTriggerStop\n");
    stringstream sg_probe_check;
    sg_probe_check << "sg_probe_check oid=" << m_oid << " x=" << 0 << " y=" << 0 << " z=" << 0 << " cmd=" << 2;
    m_mcu->m_serial->send(sg_probe_check.str(), 0, 0, m_queue);
    int cnt = 0;
    int time_out = 50;
    if (ack) {
        uint32_t wait_loop = 0;
        while (1)
        {
            wait_loop++;
            if ( wait_loop % 2345 == 0 ) {
                // 重发命令
                LOG_I("ProbeCheckTriggerStop wait cmd timeout resend cmd:%d\n",wait_loop);
                m_mcu->m_serial->send(sg_probe_check.str(), 0, 0, m_queue);
                cnt++;
                if (cnt > time_out)
                {
                    throw MCUException(m_mcu->m_serial->m_name, "No response from MCU");
                }
            } 
            usleep(1 * 1000);
            if (m_probe_cmd_response == 2){
                LOG_I("ProbeCheckTriggerStop cmd recevice response at: %f\n",get_monotonic());
                break;
            }
        }
    }
}

bool HX711S::CalibrationStart(int times_read, bool is_check_err) {
    if (m_is_shutdown || m_is_timeout) {
        LOG_E("CalibrationStart Error: m_is_shutdown(%d) or m_is_timeout(%d)\n", (int32_t)m_is_shutdown, (int32_t)m_is_timeout);
        return false;
    }
    m_probe_calibration_response = 0;
    m_is_calibration = 0;
    m_is_trigger = 0;
    if (is_check_err) {
        times_read *= 5;
    }
    stringstream calibration_sample;
    calibration_sample << "calibration_sample oid=" << m_oid
                            << " times_read=" << times_read;

    LOG_I("CalibrationStart times_read:%d is_check_err:%d at:%f\n",times_read, (int32_t)is_check_err, get_monotonic());
    for (int i=0;i<1;i++) {
        m_mcu->m_serial->send(calibration_sample.str(), 0, 0, m_queue);
    }
    int wait_loop = 0;
    // 避免上一天消息导致的提前检测
    usleep(100 * 1000);
    int cnt = 0;
    int time_out = 50;
    while (1)
    {
        wait_loop++;
        if ( wait_loop % 2345 == 0 ) { 
            // 重发命令
            LOG_I("CalibrationStart wait cmd timeout resend cmd:%d\n",wait_loop);
            m_mcu->m_serial->send(calibration_sample.str(), 0, 0, m_queue);
            //TODO: timeout
            cnt++;
            if (cnt > time_out)
            {
                throw MCUException(m_mcu->m_serial->m_name, "No response from MCU");
            }
        }
        usleep(1 * 1000);
        if (m_probe_calibration_response){
            LOG_I("calibration cmd recevice response at: %f\n",get_monotonic());
            break;
        }
    }
    wait_loop = 0;
    cnt = 0;
    while (!(m_is_shutdown || m_is_timeout)) {
        wait_loop++;
        if ( wait_loop % 2345 == 0 ) {
            LOG_I("calibration wait result loop:%d\n",wait_loop);
        }
        usleep(1 * 1000);
        if ( m_is_calibration&0x80 ) {
            LOG_I("calibration ok! at: %f\n",get_monotonic());
            return true; 
        } 
        if ( wait_loop > 100000 ) {
            LOG_E("calibration failed, timeout! at: %f\n",get_monotonic());
            throw MCUException(m_mcu->m_serial->m_name, "No response from MCU");
            return false; 
        }
    }
}

// void pnt_msg(const char* message) {
//     // Implementation of pnt_msg function
// }

// void ftr_val(Filter* filter, double* values) {
//     // Implementation of ftr_val function
// }

std::vector<double> HX711S::read_base(int cnt, double max_hold, bool reset_zero)
{

    std::cout << "read_base cnt:" << cnt << " max_hold:" << max_hold << " reset_zero:"<< reset_zero<<  std::endl;
    std::vector<double> avgs(m_s_count, 0);
    std::vector<std::vector<double>> rvs(m_s_count, std::vector<double>());
    for (int i = 0; i < m_s_count; i++)
    {
        std::vector<double>(m_s_count, 0).swap(m_base_avgs);
        std::vector<double>(m_s_count, 0).swap(avgs);
        query_start(cnt, cnt*4, true, false);
        double t_last = get_monotonic();
        int wait_loop = 0;
        while (!(m_is_shutdown || m_is_timeout) && get_vals()[0].size() < cnt /*&& (get_monotonic() - t_last) < cnt * 0.010 * 15*/)
        {   
            wait_loop++;
            if (wait_loop%1000==0) {
                std::cout << "read_base wait data" <<  std::endl;
            }
            delay_s(0.010);
        }
        std::vector<std::vector<double>> vals = get_vals();
        if (vals[0].size() < cnt)
        {
            throw std::runtime_error("z-Touch::read_base: Can not read z-Touch data.");
        }
        // 去除前面一般数据
        for (int j = 0; j < m_s_count; j++)
        {
            vals[j].erase(vals[j].begin(), vals[j].begin() + vals[j].size() / 2);
        }
        // 去掉两个最大最小
        for (int j = 0; j < m_s_count; j++)
        {
            vals[j].erase(std::min_element(vals[j].begin(), vals[j].end()));
            vals[j].erase(std::min_element(vals[j].begin(), vals[j].end()));
            vals[j].erase(std::max_element(vals[j].begin(), vals[j].end()));
            vals[j].erase(std::max_element(vals[j].begin(), vals[j].end()));
        }
        std::vector<std::vector<double>> rvs(m_s_count, std::vector<double>());
        auto tf = m_filter->get_tft();
        auto lf = m_filter->get_lft(0.5);
        for (int j = 0; j < m_s_count; j++)
        {
            vals[j] = tf->ftr_val(vals[j]);
            vals[j] = lf->ftr_val(vals[j]);
            rvs[j].push_back(*std::min_element(vals[j].begin(), vals[j].end()));
            rvs[j].push_back(std::accumulate(vals[j].begin(), vals[j].end(), 0.0) / vals[j].size());
            rvs[j].push_back(*std::max_element(vals[j].begin(), vals[j].end()));
            avgs[j] = std::accumulate(vals[j].begin(), vals[j].end(), 0.0) / vals[j].size();
            // std::cout << "READ_BASE ch=" << j << " min=" << rvs[j][0] << " avg=" << avgs[j] << " max=" << rvs[j][2] << std::endl;
        }
        // double measuremen_value[4] = {0}; 
        // for(int i=0; i<vals[0].size(); i++){
        //     for(int j = 0; j < vals[0].size(); j++){
        //         measuremen_value[i] = vals.at(i).at(j); 
        //         strain_gauge_kalman_filter_->step(measuremen_value);
        //     }
        // }

        if (reset_zero)
        {
            m_base_avgs = avgs;
            // std::cout << "m_base_avgs=" << m_base_avgs[0] << " " << m_base_avgs[1] << " "
            //           << m_base_avgs[2] << std::endl;
        }
        double sum_max = 0;
        for (int j = 0; j < m_s_count; j++)
        {
            sum_max += std::fabs(rvs[j][2] - rvs[j][0]);
        }
        if (sum_max < max_hold * 2)
        {
            break;
        }
    }
    
    for (int j = 0; j < m_s_count; j++)
    {
        std::cout << "read_base result:" << j << " : " << avgs[j] << std::endl;
    }
    return avgs;
}

void HX711S::cmd_READ_HX711(GCodeCommand &gcmd)
{
    int cnt = gcmd.get_int("C", 1, 1, 9999);
    // int cnt = 1;
    query_start(cnt, cnt, false, false, false);
    delay_s(1.0);
    m_base_avgs[0] = 0;
    m_base_avgs[1] = 0;
    m_base_avgs[2] = 0;
    m_base_avgs[3] = 0;
    while (!(m_is_shutdown || m_is_timeout) && get_vals()[0].size() < cnt /*&& (get_monotonic() - t_last) < cnt * 0.010 * 15*/)
    {
        delay_s(0.010);
    }
    std::vector<std::vector<double>> vals = get_vals();
    for (int i = 0; i < m_s_count; i++)
    {
        // gcmd->respond_info("CH%d=", i);
        std::cout << "CH" << i << "=";
        std::string sv = "[";
        for (int j = 0; j < vals[i].size(); j++)
        {
            sv += to_string(vals[i][j]);
        }
        sv += "]";
        std::cout << sv << "   ";
        // gcmd->respond_info(sv);
    }
    std::cout << std::endl;
    read_base(40, 500000);
}
