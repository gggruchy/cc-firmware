#include "mcu.h"
#include "klippy.h"
#include "Define.h"
#include "my_string.h"
#include "debug.h"
#include "gpio.h"
#include "ymodem.h"
#include "config.h"
#define LOG_TAG "mcu"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
#define CHIP_NAME "mcu"

std::string error_help(std::string msg)
{
    if (startswith(msg, "Timer too close"))
    {
        return "This often indicates the host computer is overloaded. Check\
                for other processes consuming excessive CPU time, high swap\
                usage, disk errors, overheating, unstable voltage, or\
                similar system problems on the host computer.";
    }
    else if (startswith(msg, "Missed scheduling of next "))
    {
        return "This is generally indicative of an intermittent\
                communication failure between micro-controller and host.";
    }
    else if (startswith(msg, "ADC out of range"))
    {
        return "This generally occurs when a heater temperature exceeds\
                its configured min_temp or max_temp.";
    }
    else if (startswith(msg, "Rescheduled timer in the past"))
    {
        return "This generally occurs when the micro-controller has been\
                requested to step at a rate higher than it is capable of\
                obtaining.";
    }
    else if (startswith(msg, "MCU_stepper too far in past"))
    {
        return "This generally occurs when the micro-controller has been\
                requested to step at a rate higher than it is capable of\
                obtaining.";
    }
    else if (startswith(msg, "Command request"))
    {
        return "This generally occurs in response to an M112 G-Code command\
                or in response to an internal error in the host software.";
    }
    else
    {
        return "";
    }
}

MCU *gp_mcu_main = nullptr; // 方便调试
MCU *gp_mcu_last = nullptr;
MCU::MCU(std::string section_name, ClockSync *clocksync) //-2-MCU-G-G-2023-03-25--
{
    m_clocksync = clocksync;
    // m_reactor = printer.get_reactor();
    m_name = section_name;
    mcu_type = 1;
    if (startswith(m_name, "mcu "))
    {
        mcu_type = 2;
        m_name = m_name.substr(4);
    }

    LOG_I("MCU %s\n", m_name.c_str());

    if (mcu_type == 1)
    {
        gp_mcu_main = this;
    }
    else
    {
        gp_mcu_last = this;
    }

    int pins_per_bank = 32;
    m_baud = 0;
    use_mcu_spi = 1;
    std::string canbus_uuid = Printer::GetInstance()->m_pconfig->GetString(section_name, "canbus_uuid", "");
    std::string Dspsharespace = Printer::GetInstance()->m_pconfig->GetString(section_name, "mem_interface", "");
    std::string serial_ID = Printer::GetInstance()->m_pconfig->GetString(section_name, "serial", "");
    m_max_pending_blocks = Printer::GetInstance()->m_pconfig->GetInt(section_name, "max_pending_blocks", 12);
    // CAN
    if (canbus_uuid != "")
    {
        m_serialport = canbus_uuid;
        mcu_type = 2;
        m_canbus_iface = Printer::GetInstance()->m_pconfig->GetString(section_name, "canbus_interface", "can0");
        Printer::GetInstance()->load_object("canbus_ids");
        Printer::GetInstance()->m_cbid->add_uuid(canbus_uuid, m_canbus_iface);
    }
    // DSP RPBUF
    else if (Dspsharespace != "")
    {
        m_serialport = Dspsharespace;
        mcu_type = 1;
        use_mcu_spi = 1;
        pins_per_bank = 32;
    }
    // SERIAL
    else if (serial_ID != "")
    {
        m_serialport = serial_ID;
        mcu_type = 2;
        pins_per_bank = 16;
        if (!(startswith(m_serialport, "/dev/rpmsg_") || startswith(m_serialport, "/tmp/klipper_host_")))
        {
            m_baud = Printer::GetInstance()->m_pconfig->GetInt(section_name, "baud", 250000, 2400);
        }
    }
    else
    {
        pins_per_bank = 32;
        mcu_type = 2;
        m_serialport = Printer::GetInstance()->m_pconfig->GetString(section_name, "serial");
        if (!(startswith(m_serialport, "/dev/rpmsg_") || startswith(m_serialport, "/tmp/klipper_host_")))
        {
            m_baud = Printer::GetInstance()->m_pconfig->GetInt(section_name, "baud", 250000, 2400);
        }
    }

    // Restarts
    std::vector<std::string> restart_methods = {"", "arduino", "cheetah", "command", "rpi_usb", "power"};
    m_restart_method = "command";
    m_serial = new Serialhdl(pins_per_bank, m_name); // serial port

    if (m_baud)
    {
        std::string get_method = Printer::GetInstance()->m_pconfig->GetString(section_name, "restart_method");
        bool exist = false;
        for (auto method : restart_methods)
        {
            if (get_method == method)
            {
                exist = true;
                m_restart_method = get_method;
            }
        }
        if (exist == false)
            LOG_E("method is not in restart_methods\n");
    }

    if (m_restart_method == "power")
    {
        m_power_pin.clear();
        std::string power_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "power_pin", "");
        if (power_pin != "")
        {
            m_power_pin.push_back(power_pin);
        }
        std::string chip_power_pin = Printer::GetInstance()->m_pconfig->GetString(section_name, "chip_power_pin", "");
        if (chip_power_pin != "")
        {
            m_power_pin.push_back(chip_power_pin);
        }
    }

    LOG_I("MCU %s m_restart_method %s\n", m_name.c_str(), m_restart_method.c_str());

    m_reset_cmd = nullptr;
    m_config_reset_cmd = nullptr;
    m_is_mcu_bridge = false;
    m_emergency_stop_cmd = nullptr;
    m_steppersync = nullptr;
    m_is_shutdown = false;
    m_is_keepalive = true;
    m_is_timeout = false;
    m_shutdown_clock = 0;
    m_shutdown_msg = -1;
    // Printer building
    m_oid_count = 0;
    m_mcu_freq = 0;
    // Move command queuing
    m_max_stepper_error = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "max_stepper_error", 0.000025, 0.);
    m_reserved_move_slots = 0;
    // Stats
    m_get_status_info;
    m_stats_sumsq_base = 0.;
    m_mcu_tick_avg = 0.;
    m_mcu_tick_stddev = 0.;
    m_mcu_tick_awake = 0.;
    Printer::GetInstance()->m_ppins->register_chip(m_name, this);
    // Register handlers
    Printer::GetInstance()->register_event_handler("klippy:mcu_identify:" + section_name, std::bind(&MCU::mcu_identify, this));
    Printer::GetInstance()->register_event_handler("klippy:connect:" + section_name, std::bind(&MCU::connect, this));
    Printer::GetInstance()->register_event_bool_handler("klippy:shutdown:" + section_name, std::bind(&MCU::shutdown, this, std::placeholders::_1));
    Printer::GetInstance()->register_event_handler("klippy:disconnect:" + section_name, std::bind(&MCU::disconnect, this));
    Printer::GetInstance()->register_event_handler("klippy:break_connection:" + section_name, std::bind(&MCU::break_connection, this));
}

MCU::~MCU()
{
    if (m_serial != nullptr)
    {
        delete m_serial;
    }
    if (m_steppersync != nullptr)
    {
        steppersync_free(m_steppersync);
    }
}

void MCU::handle_mcu_stats(ParseResult params)
{
    uint32_t count = params.PT_uint32_outs["count"];
    uint32_t tick_sum = params.PT_uint32_outs["sum"];

    // if(count == 5)      //-CS-G-G-2023-03-27-------
    // {
    //     uint32_t rea = params.PT_uint32_outs.at("count");
    //     uint32_t req_clock = params.PT_uint32_outs.at("sum");
    //     uint32_t cur_clock = params.PT_uint32_outs.at("sumsq");
    //     std::cout << "rea: " << rea  << " req_clock: " << req_clock  << " cur_clock: " << cur_clock << std::endl;
    //     return;
    // }
    double c = 1.0f / (count * m_mcu_freq);
    m_mcu_tick_avg = tick_sum * c;
    double tick_sumsq = params.PT_uint32_outs["sumsq"] * m_stats_sumsq_base;
    double diff = count * tick_sumsq - tick_sum * tick_sum;
    m_mcu_tick_stddev = c * sqrt(std::max(0., diff));
    m_mcu_tick_awake = tick_sum / m_mcu_freq;
}

void MCU::handle_shutdown(ParseResult params)
{
    if (m_is_shutdown)
    {
        return;
    }
    m_is_shutdown = true;
    uint32_t clock = 0;
    if (params.PT_uint32_outs.find("clock") != params.PT_uint32_outs.end())
    {
        clock = params.PT_uint32_outs["clock"];
        m_shutdown_clock = this->clock32_to_clock64(clock);
    }
    int msg = params.PT_uint32_outs["static_string_id"];
    m_shutdown_msg = msg;
    auto iter = m_serial->m_msgparser->m_shutdown_info.find(m_shutdown_msg);
    if (iter != m_serial->m_msgparser->m_shutdown_info.end())
    {
        std::string reason = iter->second;
        LOG_E("mcu : %s !!!Shutdown reason is '%d' %s c=%x %llx\n", m_name.c_str(), m_shutdown_msg, reason.c_str(), clock, m_shutdown_clock);
        utils_vfork_system("free -h >> %s", LOG_FILE_PATH);
        utils_vfork_system("top -b -n 1 >> %s", LOG_FILE_PATH);
        utils_vfork_system("dmesg >> %s", LOG_FILE_PATH);
        // std::cout << m_name << mcu_type << "!!!Shutdown reason is " << m_shutdown_msg << " " << reason << " c=" << clock<< " " << m_shutdown_clock << std::endl;
    }
    //---??---
    // logging.info("MCU '%s' %s: %s\n%s\n%s", self._name, params['#name'],
    //                  self._shutdown_msg, self._clocksync.dump_debug(),
    //                  self._serial.dump_debug());
    std::string prefix = "MCU '" + m_name + "' shutdown: ";
    if (params.msg_name == "is_shutdown")
    {
        prefix = "Previous MCU '" + m_name + "' shutdown: ";
    }
    Printer::GetInstance()->invoke_async_shutdown(prefix);
}

void MCU::handle_starting(ParseResult params)
{
    if (!m_is_shutdown)
    {
        // Printer::GetInstance()->invoke_async_shutdown("MCU '" + m_name + "' spontaneous restart");
    }
}

void MCU::check_restart(std::string reason)
{
    std::string start_reason = Printer::GetInstance()->get_start_args("start_reason");
    if (start_reason == "firmware_restart")
        return;
    //---??---
    // logging.info("Attempting automated MCU '%s' restart: %s",
    //                  self._name, reason)
    // Printer::GetInstance()->request_exit("firmware_restart");
    //---??---
    // m_reactor.pause(self._reactor.monotonic() + 2.000)
    // raise error("Attempt MCU '%s' restart failed" % (self._name,))
}

void MCU::connect_file(bool pace)
{ //---??---
    // In a debugging mode.  Open debug output file and read data dictionary
    std::string out_fname, dict_fname;
    if (m_name == "mcu")
    {
        out_fname = Printer::GetInstance()->get_start_args("debugoutput");
        dict_fname = Printer::GetInstance()->get_start_args("dictionary");
    }
    else
    {
        out_fname = Printer::GetInstance()->get_start_args("debugoutput") + "-" + m_name;
        dict_fname = Printer::GetInstance()->get_start_args("dictionary_" + m_name);
    }
    //---??---
    // outfile = open(out_fname, 'wb')
    // dfile = open(dict_fname, 'rb')
    // dict_data = dfile.read()
    // dfile.close()
    // self._serial.connect_file(outfile, dict_data)
    // self._clocksync.connect_file(self._serial, pace);
    // Handle pacing
    // m_pace = pace;
}

void MCU::send_config() //-12-MCU-G-G-2023-03-25--
{
    LOG_I("MCU %s send_config start\n", m_name.c_str());
    std::string cmd = "get_config";
    ParseResult config_response = m_serial->send_with_response(cmd, "config", m_serial->m_command_queue);
    auto it = config_response.PT_uint32_outs.find("is_config");

    if (it == config_response.PT_uint32_outs.end())
    {
        LOG_E("MCU '%s' already configured\n", m_name.c_str());
        // return;
    }

    // 各个模块配置回调,收集所有配置信息
    for (int i = 0; i < m_config_callbacks.size(); i++)
        m_config_callbacks[i](1);

    std::stringstream allocate_oids;
    allocate_oids << "allocate_oids count=" << m_oid_count;
    m_serial->send(allocate_oids.str(), 0, 0, m_serial->m_command_queue);
    usleep(10);

    // 发送配置
    for (int i = 0; i < m_config_cmds.size(); i++) //
    {
        m_serial->send(m_config_cmds[i], 0, 0, m_serial->m_command_queue);
        usleep(10); //  sleep(1);
    }

    m_serial->send("finalize_config crc=1464193749", 0, 0, m_serial->m_command_queue);
    config_response = m_serial->send_with_response(cmd, "config", m_serial->m_command_queue);

    if (it == config_response.PT_uint32_outs.end())
        LOG_I("MCU %s send_config config_err\n", m_name.c_str());
    else
        LOG_I("MCU %s send_config config_ok\n", m_name.c_str());

    for (int i = 0; i < m_config_callbacks.size(); i++)
    {
        m_config_callbacks[i](4);
    }

    for (int i = 0; i < m_restart_cmds.size(); i++)
    {
        m_serial->send(m_restart_cmds[i], 0, 0, m_serial->m_command_queue);
    }

    for (int i = 0; i < m_config_callbacks.size(); i++)
    {
        m_config_callbacks[i](2);
    }

    for (int i = 0; i < m_init_cmds.size(); i++) //---??---  风扇和加热器的pwm,adc ５条初始化命令，会出现下位机shutdown
    {
        m_serial->send(m_init_cmds[i], 0, 0, m_serial->m_command_queue);
    }

    LOG_I("MCU %s send_config end\n", m_name.c_str());
}

std::string MCU::log_info()
{
    //---??---
}

int debug_rx_on = 0;

//   stepcompress* stepqueues[10];
void MCU::connect() //-11-MCU-G-G-2023-03-25--
{
    send_config();

    if (m_serial->pins_per_bank == 16)
        debug_rx_on = 0;

    LOG_I("MCU %s get_config start\n", m_name.c_str());
    std::string cmd = "get_config";
    ParseResult config_response = m_serial->send_with_response(cmd, "config", m_serial->m_command_queue);
    LOG_I("MCU %s get_config end\n", m_name.c_str());

    int move_count = config_response.PT_uint32_outs["move_count"];

    // std::cout << "move_count =" << move_count << std::endl;
    move_count = 1024;
    stepcompress *stepqueues[m_stepqueues.size()];
    for (int i = 0; i < m_stepqueues.size(); i++)
        stepqueues[i] = m_stepqueues[i];
    m_steppersync = steppersync_alloc(m_serial->m_serialqueue, stepqueues, m_stepqueues.size(), move_count - m_reserved_move_slots);
    steppersync_set_time(m_steppersync, 0.0, m_mcu_freq);
    init_config_ok = true;

    LOG_I("MCU %s connect done\n", m_name.c_str());
}

void MCU::mcu_identify() //-4-MCU-G-G-2023-03-25--
{
    int retry = 0;
#define RETRY_TIMES 5
    while (retry < RETRY_TIMES)
    {
        // 打开STM32电源
        if (m_restart_method == "power")
        {
            char pin[16] = {0};
            std::vector<int> pin_num(m_power_pin.size());

            for (uint8_t i = 0; i < m_power_pin.size(); i++)
            {
                strncpy(pin, m_power_pin[i].c_str(), sizeof(pin));
                int port = pin[1] - 'A';
                int num = atoi(&pin[2]);
                pin_num[i] = port * 32 + num;
                gpio_init(pin_num[i]);
                gpio_set_direction(pin_num[i], GPIO_OUTPUT);
                gpio_set_value(pin_num[i], GPIO_LOW);
            }

            usleep(1 * 1000 * 1000);
            for (uint8_t i = 0; i < m_power_pin.size(); i++)
            {
                gpio_set_value(pin_num[i], GPIO_HIGH);
                usleep(500);
            }

            usleep(2 * 1000 * 1000); // 2s
        }
#if MCU_UPDATE
        if (mcu_type != 1) // 非共享内存 下位机升级
        {
            LOG_D("mcu_name : %s\n", m_name.c_str());
            user_ymodem_ctx_t mcu_upgrade_ctx;
            int ret = 0;
            if (strstr(m_name.c_str(), "strain_gauge_mcu"))
                ret = ymodem_init(&mcu_upgrade_ctx, m_serialport.c_str(), STRAIN_GAUGE_MCU_UPDATE_PATH, m_baud);
            else if (strstr(m_name.c_str(), "stm32"))
                ret = ymodem_init(&mcu_upgrade_ctx, m_serialport.c_str(), EXTRUDER_MCU_UPDATE_PATH, m_baud);
            LOG_D("ymodem_init %d \n", ret);
            if (ret == 0)
            {
                ret = ymodem_transmit(&mcu_upgrade_ctx);
                LOG_D("ymodem_transmit %d \n", ret);
                ymodem_deint(&mcu_upgrade_ctx);
                LOG_D("ymodem_deint %d \n", ret);
                if (ret == 0x02) // ymodem错误0x02
                {
                    retry++;
                    LOG_E("ymodem failed. retry %d\n", retry);
                    continue;
                    // throw MCUException(m_serial->m_name, "No response from MCU");
                }
            }
            if (strstr(m_name.c_str(), "stm32"))
            {
                // wait port ready
                LOG_D("wait Secondary mcu ready\n");
                usleep(3 * 1000 * 1000); // close端口后，下位机复位，如果马上打开端口   1：有可能失败。2：端口号已变化。
            }
        }
#endif
        int ret = m_serial->connect_uart(m_serialport, m_baud, mcu_type, m_max_pending_blocks);
        if (ret != 0)
        {
            retry++;
            LOG_E("connect_uart failed. retry %d\n", retry);
        }
        else
        {
            break;
        }
    }
    if (retry >= RETRY_TIMES)
    {
        LOG_E("connect_uart failed.\n");
        throw MCUException(m_serial->m_name, "connect_uart failed");
    }
    m_mcu_freq = m_serial->get_constant_wapper()->clock_freq;
    m_stats_sumsq_base = m_serial->get_constant_wapper()->stats_sumsq_base;
    m_baud = m_serial->get_constant_wapper()->serial_baud; // 通信之后再次确认波特率
    m_clocksync->connect(m_serial);
    m_clocksync->creat_get_clock_pthread();
    m_serial->register_response(std::bind(&MCU::handle_shutdown, this, std::placeholders::_1), "shutdown");
    m_serial->register_response(std::bind(&MCU::handle_shutdown, this, std::placeholders::_1), "is_shutdown");
    m_serial->register_response(std::bind(&MCU::handle_mcu_stats, this, std::placeholders::_1), "stats");
    LOG_I("MCU %s mcu_identify done\n", m_name.c_str());
}

int MCU::create_oid()
{
    int ret = m_oid_count;
    m_oid_count += 1;
    return ret;
}

void MCU::register_config_callback(std::function<void(int)> callback)
{
    m_config_callbacks.push_back(callback);
}

void MCU::add_config_cmd(std::string cmd, bool is_init, bool on_restart)
{
    if (is_init)
    {
        m_init_cmds.push_back(cmd);
    }
    else if (on_restart)
    {
        m_restart_cmds.push_back(cmd);
    }
    else
    {
        m_config_cmds.push_back(cmd);
    }
}

uint64_t MCU::get_query_slot(int oid)
{
    uint64_t slot = seconds_to_clock(oid * 0.01);
    double t = (int)(m_clocksync->estimated_print_time(get_monotonic()) + 1.5);
    uint64_t ret = print_time_to_clock(t) + slot;
    return ret;
}

void MCU::register_stepqueue(stepcompress *stepqueue)
{
    m_stepqueues.push_back(stepqueue);
}

void MCU::request_move_queue_slot()
{
    m_reserved_move_slots += 1;
}

uint64_t MCU::seconds_to_clock(double time)
{
    return (uint64_t)(time * m_mcu_freq);
}

std::string MCU::get_name()
{
    return m_name;
}

CommandWrapper::CommandWrapper(Serialhdl *serial, std::string msgformat, command_queue *cmd_queue)
{
    // m_serial = serial
    // m_cmd = serial->get_msgparser()->lookup_command(msgformat)
    // if (cmd_queue == nullptr)
    //     cmd_queue = serial->get_default_command_queue()
    // m_cmd_queue = cmd_queue
}
void CommandWrapper::send(uint64_t minclock, uint64_t reqclock)
{
    // cmd = m_cmd->encode(data)
    // m_serial->raw_send(cmd, minclock, reqclock, m_cmd_queue)
}

CommandWrapper *MCU::lookup_command(std::string msgformat, command_queue *cq)
{
    // CommandWrapper* command_wrapper = new CommandWrapper(m_serial, msgformat, cq);
    // return command_wrapper;
    //  return new CommandWrapper(m_serial, msgformat, cq);
}

CommandQueryWrapper *MCU::lookup_query_command(std::string msgformat, std::string respformat, int oid, command_queue *cq, bool is_async)
{
    // CommandQueryWrapper* command_query_warapper = new CommandQueryWrapper(m_serial, msgformat, respformat, oid, cq, is_async);
    // return command_query_warapper;
}

CommandWrapper *MCU::try_lookup_command(std::string msgformat)
{
}

void lookup_command_tag(std::string msgformat)
{
}

void MCU::get_enumerations()
{
}

void MCU::get_constants()
{
}

float MCU::get_constant_float(std::string name)
{
}

uint64_t MCU::print_time_to_clock(double print_time)
{
    uint64_t ret = m_clocksync->print_time_to_clock(print_time);
    return ret;
}

double MCU::clock_to_print_time(uint64_t clock)
{
    return m_clocksync->clock_to_print_time(clock);
}

double MCU::estimated_print_time(double eventtime)
{
    return m_clocksync->estimated_print_time(eventtime);
}

uint64_t MCU::clock32_to_clock64(uint32_t clock_32)
{
    return m_clocksync->clock32_to_clock64(clock_32);
}

void MCU::disconnect()
{
    m_serial->disconnect();
    steppersync_free(m_steppersync);
    m_steppersync = nullptr;
}

void MCU::break_connection()
{
    char pin[16] = {0};
    for (uint8_t i = 0; i < m_power_pin.size(); i++)
    {
        strncpy(pin, m_power_pin[i].c_str(), sizeof(pin));
        int port = pin[1] - 'A';
        int num = atoi(&pin[2]);
        gpio_set_value(port * 32 + num, GPIO_LOW);
    }
}

void MCU::shutdown(bool force)
{
    if (/*m_emergency_stop_cmd == nullptr ||*/ (m_is_shutdown && !force))
    {
        return;
    }
    m_clocksync->m_clock_sync_flag = 1; // 关闭时钟同步
    std::cout << "send emergency_stop cmd" << std::endl;
    m_serial->send("emergency_stop", 0, 0, m_serial->m_command_queue);
    // m_emergency_stop_cmd->send();  //---??---
}

void MCU::restart_arduino()
{
    LOG_E("Attempting MCU '%s' reset\n", m_name.c_str());
    // std::cout << "Attempting MCU '" << m_name << "' reset" << std::endl;
    this->disconnect();
    // arduino_reset(self._serialport, self._reactor); // serialhdl.cpp  //---??---
}

void MCU::restart_cheetah()
{
    LOG_E("Attempting MCU '%s' Cheetah-style reset\n", m_name.c_str());
    // std::cout << "Attempting MCU '" << m_name << "' Cheetah-style reset" << std::endl;
    this->disconnect();
    // cheetah_reset(m_serialport, m_reactor); // serialhdl.cpp //---??---
}

void MCU::restart_via_command()
{
    if (m_restart_method == "command")
    {
        printf("restart_via_command\n");
        m_serial->send("reset", 0, 0, m_serial->m_command_queue);
    }
    // sleep(2);
    // disconnect();

    // if((m_reset_cmd == nullptr && m_config_reset_cmd == nullptr) || !m_clocksync->is_active())
    // {
    //     std::cout << "Unable to issue reset command on MCU '" << m_name << "'" << std::endl;
    //     return;
    // }
    // if(m_reset_cmd == nullptr)
    // {
    //     // Attempt reset via config_reset command
    //     m_is_shutdown = true;
    //     shutdown(true);
    //     m_reactor.pause(get_monotonic() + 0.015);
    //     m_config_reset_cmd->send();
    // }  //---??---
    // else
    // {
    //     // Attempt reset via reset command

    // }
}

void MCU::restart_rpi_usb()
{
}

void MCU::firmware_restart(bool force)
{
}

void MCU::firmware_restart_bridge()
{
}

bool MCU::is_fileoutput()
{
    // return self._printer.get_start_args().get('debugoutput') is not None; //---??---
}

bool MCU::is_shutdown()
{
    return m_is_shutdown;
}

uint64_t MCU::get_shutdown_clock()
{
    return m_shutdown_clock;
}

void *MCU::setup_pin(std::string pin_type, pinParams *pin_params)
{
    if (pin_type == "endstop")
    {
        MCU_endstop *mcu_endstop = new MCU_endstop(this, pin_params);
        return mcu_endstop;
    }
    else if (pin_type == "digital_out")
    {
        MCU_digital_out *mcu_digital_out = new MCU_digital_out(this, pin_params);
        return mcu_digital_out;
    }
    else if (pin_type == "pwm")
    {
        MCU_pwm *mcu_pwm = new MCU_pwm(this, pin_params);
        return mcu_pwm;
    }
    else if (pin_type == "adc")
    {
        MCU_adc *mcu_adc = new MCU_adc(this, pin_params);
        return mcu_adc;
    }
    return nullptr;
}

void MCU::register_response(std::function<void(ParseResult &)> cb, std::string msg, int oid)
{
    m_serial->register_response(cb, msg, oid);
}

command_queue *MCU::alloc_command_queue()
{
    return m_serial->alloc_command_queue();
}

void MCU::flush_moves(double print_time) //--8-home-2task-G-G--UI_control_task--    //---25-2task-G-G--UI_control_task--
{
    if (m_steppersync == nullptr)
    {
        return;
    }
    uint64_t clock = m_clocksync->print_time_to_clock(print_time);
    if (clock < 0)
    {
        return;
    }
    int ret = steppersync_flush(m_steppersync, clock); //--9-home-2task-G-G--UI_control_task--  //---26-2task-G-G--UI_control_task--
    if (ret)
    {
        LOG_E("Internal error in MCU '%s' flush_moves print_time:%f clock:%lld ret:%d \n", m_name.c_str(), print_time, clock, ret);
        // std::cout << "Internal error in MCU '" + m_name + "' stepcompress" << std::endl;
    }
}

void MCU::check_active(double print_time, double eventtime)
{
    // LOG_D("MCU %s check_active start\n", m_name.c_str());
    if (m_steppersync == nullptr)
    {
        return;
    }
    std::vector<double> ret = m_clocksync->calibrate_clock(print_time, eventtime);
    steppersync_set_time(m_steppersync, ret[0], ret[1]);
    if (m_clocksync->is_active() || this->is_fileoutput() || m_is_timeout)
    {
        return;
    }
    else
    {
        if (m_clocksync->m_serial->m_name == "strain_gauge_mcu")
        {
            if (Printer::GetInstance()->m_virtual_sdcard->is_active() || !Printer::GetInstance()->m_virtual_sdcard->start_print_from_sd())
            {
                if (!Printer::GetInstance()->m_virtual_sdcard->get_cancel_flag())
                {
                    return;
                }
            }
        }
    }
    m_is_timeout = true;
    LOG_E("Timeout with MCU '%s' (eventtime=%f)\n", m_name.c_str(), eventtime);
    // std::cout << "Timeout with MCU '" << m_name << "' (eventtime=" << eventtime << ")" << std::endl;
    Printer::GetInstance()->invoke_shutdown("Lost communication with MCU '" + m_name + "'");
}

void MCU::get_status()
{
    // return dict(m_get_status_info)  //---??---
}
void MCU::stats(double eventtime)
{
}

void add_printer_mcu() //-1-MCU-G-G-2023-03-25--
{
    ClockSyncMain *mainsync = new ClockSyncMain(); // reactor                      //-2-MCU-G-G-2023-03-25--
    Printer::GetInstance()->m_mcu = new MCU("mcu", mainsync);
    Printer::GetInstance()->add_object("mcu", Printer::GetInstance()->m_mcu);

    // 找到除主mcu外的其他mcu
    std::vector<string> mcu_section = Printer::GetInstance()->m_pconfig->get_prefix_sections("mcu ");
    for (int i = 0; i < mcu_section.size(); i++)
    {
        Printer::GetInstance()->m_mcu_map[mcu_section[i]] = new MCU(mcu_section[i], new SecondarySync(mainsync));
        Printer::GetInstance()->add_object(mcu_section[i], Printer::GetInstance()->m_mcu_map[mcu_section[i]]);

        // Printer::GetInstance()->add_object(mcu_section[i],new MCU(mcu_section[i], mainsync) );
        // printer.add_object(s.section, MCU(mcu_section[i], SecondarySync(reactor, mainsync)));  //---??---
    }
}
MCU *get_printer_mcu(Printer *printer, std::string name)
{
    if (name == "mcu")
        return Printer::GetInstance()->m_mcu;
    return Printer::GetInstance()->m_mcu_map["mcu " + name];
}
