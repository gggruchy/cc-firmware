#include "serialhdl.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "string.h"
#include <atomic>
#include "klippy.h"
#include "Define.h"
#include "gpio.h"

#include "serial.h"
#include "debug.h"
#define LOG_TAG "serialhdl"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

Serialhdl::Serialhdl(int pin32, std::string name) //-3-MCU-G-G-2023-03-25--
{
    m_name = name;
    pins_per_bank = pin32;
    m_command_queue = alloc_command_queue();
    m_last_notify_id = 0;
    // m_msgparser = new MessageParser();
    m_msgparser = new MessageParser(pins_per_bank);
    m_is_active = true;
}

Serialhdl::~Serialhdl()
{
    if (m_command_queue != nullptr)
    {
        serialqueue_free_commandqueue(m_command_queue);
    }
    if (m_serialqueue != nullptr)
    {
        serialqueue_free(m_serialqueue);
    }
}

void bg_stm32led(gpio_state_t gpio_level){
    if(Printer::GetInstance()->m_usb_led != nullptr)
        gpio_set_value(Printer::GetInstance()->m_usb_led->get_lednum(), gpio_level);
}

void *bg_thread(void *arg)
{
    serialqueue *serial_queue = static_cast<serialqueue*>(arg);
    if (!serial_queue) {
        return nullptr;
    }
    
    Serialhdl *m_serial = static_cast<Serialhdl*>(serial_queue->m_serial);
    pull_queue_message response;
    std::vector<uint8_t> parse_msg_buffer;  // 预分配缓冲区
    parse_msg_buffer.reserve(256);  // 预分配合理大小，避免频繁重分配
    
    const bool is_stm32 = (m_serial->m_name == "stm32");  // 缓存比较结果
    
    while (true)
    {
        serialqueue_pull(serial_queue, &response);
        
        const int count = response.len;
        if (count < 0) {
            break;
        }
        
        // LED控制优化
        bg_stm32led(is_stm32 ? GPIO_LOW : GPIO_HIGH);
        
        // 处理通知消息
        if (response.notify_id != 0) {
            // 移除无效的空判断代码
            continue;
        }
        
        // 消息解析优化
        parse_msg_buffer.resize(count);
        memcpy(parse_msg_buffer.data(), response.msg, count);
        
        ParseResult result = m_serial->m_msgparser->parse(parse_msg_buffer.data());
        result.sent_time = response.sent_time;
        result.receive_time = response.receive_time;
        
        // 构建消息名称
        std::string msg_name = result.msg_name + std::to_string(result.PT_uint32_outs["oid"]);
        
        // 使用RAII方式管理锁
        {
            std::lock_guard<std::mutex> lock(m_serial->m_mtx);
            auto callback_it = m_serial->m_response_callbacks.find(msg_name);
            if (callback_it != m_serial->m_response_callbacks.end()) {
                callback_it->second(result);
            }
        }
    }
    
    return nullptr;
}

bool Serialhdl::get_active()
{
    return m_is_active;
}

void Serialhdl::register_response(std::function<void(ParseResult &)> response_callback, std::string name, int oid)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    // m_mtx.lock();
    std::string msg_name = name + std::to_string(oid);
    // std::cout << "register_response msg_name== " << msg_name << std::endl;
    m_response_callbacks[msg_name] = response_callback;
    // m_mtx.unlock();
}

command_queue *Serialhdl::alloc_command_queue()
{
    command_queue *commend_q = serialqueue_alloc_commandqueue();
    return commend_q;
    // serialqueue_free_commandqueue(command_queue * cq);
}

int Serialhdl::connect_uart(std::string serialport, uint32_t baud, uint32_t mcu_type, int max_pending_blocks) //-5-MCU-G-G-2023-03-25--//-3task-G-G- //-4task-G-G-     //-5task-G-G-
{
    // DisTraceMsg();
    char serial_fd_type;
    double start_time = get_monotonic();
    if (get_monotonic() > start_time + 90)
    {
        GAM_ERR_printf("unable to connect %f %f \n", (float)start_time, (float)get_monotonic());
    }

    // MCU_TYPE == 1 用于DSP
    if (mcu_type == 1)
    {
        serial_fd_type = 'm';
        constant_wapper.receive_window = 192;
        constant_wapper.clock_freq = 200000000;
        constant_wapper.serial_baud = 0;
        constant_wapper.pwm_max = 255;
        constant_wapper.adc_max = 4095;
        constant_wapper.stats_sumsq_base = 256;
    }
    else
    {
        serial_fd_type = 'u';
        constant_wapper.receive_window = 192;
        constant_wapper.clock_freq = 84000000;
        constant_wapper.serial_baud = baud;
        constant_wapper.pwm_max = 2000000;
        constant_wapper.adc_max = 4095;
        constant_wapper.stats_sumsq_base = 256;
    }

    m_serialqueue = serialqueue_alloc(serialport.c_str(), (void *)baud, serial_fd_type, 0, max_pending_blocks);
    if (m_serialqueue == NULL)
    {
        return -1;
    }
    m_serialqueue->m_serial = (void *)this;
    if (constant_wapper.serial_baud)
        serialqueue_set_wire_frequency(m_serialqueue, constant_wapper.serial_baud); // 0.00001
    if (constant_wapper.receive_window)
        serialqueue_set_receive_window(m_serialqueue, constant_wapper.receive_window);

    pthread_create(&serial_tid, NULL, bg_thread, m_serialqueue);
}

void Serialhdl::disconnect()
{
    if (m_serialqueue != nullptr)
    {
        serialqueue_exit(m_serialqueue);
        m_serialqueue = nullptr;
        int ret = pthread_join(serial_tid, NULL);
        if (ret)
            std::cout << "serial pthread_join fail! " << std::endl;
    }
    Printer::GetInstance()->m_mcu->m_clocksync->m_clock_restart_flag = true;
    int ret = pthread_join(Printer::GetInstance()->m_mcu->m_clocksync->clock_tid, NULL);
    if (ret)
        std::cout << "clock pthread_join fail! " << std::endl;
}

std::string Serialhdl::get_identify_data(double eventtime)
{
    std::string identify_data = ""; // std::cout << "identify_data: " << identify_data.size() << " :" << identify_data<< std::endl;
    int len = 0;
    while (1)
    {
        std::stringstream msg;
        msg << "identify offset=" << std::to_string(identify_data.size()) << " count=" << std::to_string(40);
        ParseResult result = send_with_response(msg.str(), "identify_response", m_command_queue);
        if (result.msg_name == "identify_response") //--getIdentifyData-G-G-2022-07-25----
        {
            if (result.PT_uint32_outs.at("offset") == (identify_data.size()))
            {
                std::string msgdata = result.PT_string_outs.at("data");
                if (!msgdata.size())
                {
                    return identify_data;
                }
                identify_data += msgdata;
            }
        }
    }
}

MessageParser *Serialhdl::get_msgparser()
{
    return m_msgparser;
}

ParseResult Serialhdl::send_with_response(std::string msg, std::string response_name, command_queue *cmd_queue, int oid, uint64_t minclock, uint64_t reqclock)
{
    //"get_uptime"  "get_clock"  "get_config" "identify"
    // std::cout << "response msg = " << msg << std::endl;
    if (!m_is_active)
    {
        return ParseResult();
    }
    std::vector<uint8_t> cmd = m_msgparser->create_command(msg, m_msgparser->m_namae_to_id);
    uint8_t cmd_uint8_t[cmd.size()];
    for (int i = 0; i < cmd.size(); i++)
    {
        cmd_uint8_t[i] = cmd[i];
    }
    int cmd_len = cmd.size();
    SerialRetryCommand src(this, response_name, oid);
    ParseResult result = src.get_response(msg, cmd_uint8_t, cmd_len, cmd_queue, minclock, reqclock);
    return result;
}

void Serialhdl::send(std::string msg, uint64_t minclock, uint64_t reqclock, command_queue *cmd_queue)
{
    // std::cout << "msg = " << msg << std::endl;
    std::vector<uint8_t> cmd = m_msgparser->create_command(msg, m_msgparser->m_namae_to_id);

    uint8_t cmd_uint8_t[cmd.size()];
    for (int i = 0; i < cmd.size(); i++)
    {
        cmd_uint8_t[i] = cmd[i];
    }
    int cmd_len = cmd.size();
    raw_send(cmd_uint8_t, cmd_len, minclock, reqclock, cmd_queue); //-2-5task-G-G---get_clock---
}

void Serialhdl::raw_send(uint8_t *cmd, int cmd_len, uint64_t minclock, uint64_t reqclock, command_queue *cmd_queue)
{
    if (!m_is_active)
    {
        return;
    }
    serialqueue_send(m_serialqueue, cmd_queue, cmd, cmd_len, minclock, reqclock, 0); //-3-5task-G-G---get_clock---
}

void Serialhdl::raw_send_wait_ack(uint8_t *cmd, int cmd_len, uint64_t minclock, uint64_t reqclock, command_queue *cmd_queue)
{
    if (!m_is_active)
    {
        return;
    }
    //"trsync_trigger"  "stepper_get_position"    "get_uptime"  "get_clock"  "get_config" "identify"
    m_last_notify_id += 1;
    uint64_t nid = m_last_notify_id;
    serialqueue_send(m_serialqueue, cmd_queue, cmd, cmd_len, minclock, reqclock, nid);
}

std::string Serialhdl::stats(double eventtime)
{
    char buf[4096];
    if (!m_serialqueue)
        return "";
    serialqueue_get_stats(m_serialqueue, buf, sizeof(buf));
    return std::string(buf);
}

SerialRetryCommand::SerialRetryCommand(Serialhdl *serial, std::string rerspose_name, int oid)
{
    m_serial = serial;
    m_name = rerspose_name;
    m_oid = oid;
    m_serial->register_response(std::bind(&SerialRetryCommand::handle_callback, this, std::placeholders::_1), m_name, oid);
    m_last_params.msg_name = "";
}

SerialRetryCommand::~SerialRetryCommand()
{
}

void SerialRetryCommand::handle_callback(ParseResult &params)
{
    // std::lock_guard<std::mutex> lock(m_mutex);
    m_last_params = params;
    // m_response_ready = true;
    // m_cv.notify_one();
}

ParseResult SerialRetryCommand::get_response(std::string &msg, uint8_t *cmd, int cmd_len, command_queue *command_queue, uint64_t minclock, uint64_t reqclock)
{ //"trsync_trigger"  "stepper_get_position"    "get_uptime"  "get_clock"  "get_config" "identify"
    int retries = 15;
    double retry_delay = 0.010;
    int debug_out = 1500;
    while (1)
    {
        m_serial->raw_send_wait_ack(cmd, cmd_len, minclock, reqclock, command_queue);
        usleep(7 * 1000);
        ParseResult local_params;
        bool has_response = false;
        std::lock_guard<std::mutex> lock(m_serial->m_mtx);
        if (m_last_params.msg_name != "")
        {
            // m_serial->m_mtx.lock();
            local_params = m_last_params;
            has_response = true;
            m_serial->m_response_callbacks.erase(m_last_params.msg_name + std::to_string(m_oid));
            // m_serial->m_mtx.unlock();
            // return m_last_params;
        }
        if (has_response)
        {
            return local_params;
        }

        if (retries <= 0) // 原版不会卡在这。。
        {
            if (debug_out == 0)
            {
                LOG_E("Error: No response from MCU %d pins_per_bank %d --- %s\n", m_serial->fd_serial, m_serial->pins_per_bank, msg.c_str());
                m_serial->m_is_active = false;
                throw MCUException(m_serial->m_name, "No response from MCU");
                // LOG_E("m_serial->m_name = %s\n", m_serial->m_name.c_str());
                
                // debug_out = 500;
                return m_last_params;
            }
            debug_out--;
            // break;
        }
        retries -= 1;
        retry_delay *= 2.0;
    }
}
