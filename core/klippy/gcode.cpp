#include "gcode.h"
#include "klippy.h"
#include "my_string.h"
#include <iostream>
#include <sstream>
#include <cstdarg>
#define LOG_TAG "gcode"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

GCodeCommand::GCodeCommand(std::string command, std::string commandline, std::map<std::string, std::string> &params, bool need_ack)
{
    m_command = command;
    m_commandline = commandline;
    m_params = params;
    m_need_ack = need_ack;
    // 需要绑定GCodeDispatch单例对象  //----??----
    m_respond_info = std::bind(&GCodeDispatch::respond_info, Printer::GetInstance()->m_gcode, std::placeholders::_1, std::placeholders::_2);
    m_respond_raw = std::bind(&GCodeDispatch::respond_raw, Printer::GetInstance()->m_gcode, std::placeholders::_1);
}

GCodeCommand::~GCodeCommand()
{
}

std::string GCodeCommand::get_command()
{
    return m_command;
}

std::string GCodeCommand::get_commandline()
{
    return m_commandline;
}

void GCodeCommand::get_command_parameters(std::map<std::string, std::string> &params)
{
    params = m_params;
    return;
}

std::string GCodeCommand::get_raw_command_parameters()
{
    std::string command = m_command;
    if (startswith(command, "M117 ") || startswith(command, "M118 "))
    {
        command = command.substr(0, 4);
    }
    std::string rawparams = m_commandline;
    std::string urawparams = rawparams;
    transform(urawparams.begin(), urawparams.end(), urawparams.begin(), ::toupper);
    if (!startswith(urawparams, command))
    {
        rawparams = rawparams.substr(urawparams.find(command));
        int end = rawparams.rfind('*');
        if (end >= 0)
        {
            rawparams = rawparams.substr(0, end);
        }
    }
    rawparams = rawparams.substr(command.length());
    if (startswith(rawparams, " "))
    {
        rawparams = rawparams.substr(1);
    }
    return rawparams;
}

bool GCodeCommand::ack(std::string msg)
{
    if (!m_need_ack)
        return false;
    std::string ok_msg = "ok";
    if (msg != "")
    {
        ok_msg = "ok " + msg;
    }
    // m_respond_raw(ok_msg);
    m_need_ack = false;
    return true;
}

std::string GCodeCommand::get_string(std::string name, std::string val_default)
{
    auto iter = m_params.find(name);
    if (iter == m_params.end())
    {
        return val_default;
    }
    return iter->second;
}

int GCodeCommand::get_int(std::string name, int val_default, int minval, int maxval)
{
    auto iter = m_params.find(name);
    if (iter == m_params.end())
    {
        return val_default;
    }
    int value = stoi(iter->second);
    if (errno)
    {
        // std::cout << "error on " << m_commandline << ": missing " << name << std::endl;
    }
    if (minval != INT32_MIN && value < minval)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must have minimum of " << minval << std::endl;
        return val_default;
    }
    if (maxval != INT32_MAX && value > maxval)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must have maximum of " << maxval << std::endl;
        return val_default;
    }
    return value;
}

double GCodeCommand::get_double(std::string name, double val_default, double minval,
                                double maxval, double above, double below)
{
    auto iter = m_params.find(name);
    if (iter == m_params.end())
    {
        return val_default;
    }
    double value = stod(iter->second);
    if (errno)
    {
        // std::cout << "error on " << m_commandline << ": missing " << name << std::endl;
    }
    if (minval != DBL_MIN && value < minval)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must have minimum of " << minval << std::endl;
        return val_default;
    }
    if (maxval != DBL_MAX && value > maxval)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must have maximum of " << maxval << std::endl;
        return val_default;
    }
    if (above != DBL_MIN && value <= above)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must be above " << above << std::endl;
        return val_default;
    }
    if (below != DBL_MAX && value >= below)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must be below " << below << std::endl;
        return val_default;
    }
    return value;
}

float GCodeCommand::get_float(std::string name, float val_default, float minval,
                              double maxval, double above, double below)
{
    auto iter = m_params.find(name);
    if (iter == m_params.end())
    {
        return val_default;
    }
    float value = stof(iter->second);
    if (errno)
    {
        // std::cout << "error on " << m_commandline << ": missing " << name << std::endl;
    }
    if (minval != FLT_MIN && value < minval)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must have minimum of " << minval << std::endl;
        return val_default;
    }
    if (maxval != FLT_MAX && value > maxval)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must have maximum of " << maxval << std::endl;
        return val_default;
    }
    if (above != FLT_MIN && value <= above)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must be above " << above << std::endl;
        return val_default;
    }
    if (below != FLT_MAX && value >= below)
    {
        std::cout << "error on " << m_commandline << ": " << name << "must be below " << below << std::endl;
        return val_default;
    }
    return value;
}

GCodeDispatch::GCodeDispatch()
{
    // DisTraceMsg();
    // self.printer = printer  //----??----
    // self.is_fileinput = not not printer.get_start_args().get("debuginput")
    Printer::GetInstance()->register_event_handler("klippy:ready:GCodeDispatch", std::bind(&GCodeDispatch::handle_ready, this));
    Printer::GetInstance()->register_event_handler("klippy:shutdown:GCodeDispatch", std::bind(&GCodeDispatch::handle_shutdown, this));
    Printer::GetInstance()->register_event_handler("klippy:disconnect:GCodeDispatch", std::bind(&GCodeDispatch::handle_disconnect, this));
    m_is_printer_ready = false;
    // // m_mutex = Printer::GetInstance()->m_reactor->mutex();
    m_cmd_RESTART_help = "Reload config file and restart host software";
    m_cmd_FIRMWARE_RESTART_help = "Restart firmware, host, and reload config";
    m_cmd_STATUS_help = "Report the printer status";
    m_cmd_HELP_help = "Report the list of available extended G-Code commands";
    register_command("G", std::bind(&GCodeDispatch::cmd_G, this, std::placeholders::_1));
    register_command("M", std::bind(&GCodeDispatch::cmd_M, this, std::placeholders::_1));
    register_command("M110", std::bind(&GCodeDispatch::cmd_M110, this, std::placeholders::_1), true, "");
    register_command("M112", std::bind(&GCodeDispatch::cmd_M112, this, std::placeholders::_1), true, "");
    register_command("M115", std::bind(&GCodeDispatch::cmd_M115, this, std::placeholders::_1), true, "");
    register_command("RESTART", std::bind(&GCodeDispatch::cmd_RESTART, this, std::placeholders::_1), true, m_cmd_RESTART_help);
    register_command("FIRMWARE_RESTART", std::bind(&GCodeDispatch::cmd_FIRMWARE_RESTART, this, std::placeholders::_1), true, m_cmd_FIRMWARE_RESTART_help);
    register_command("ECHO", std::bind(&GCodeDispatch::cmd_ECHO, this, std::placeholders::_1), true, "");
    register_command("STATUS", std::bind(&GCodeDispatch::cmd_STATUS, this, std::placeholders::_1), true, m_cmd_STATUS_help);
    register_command("HELP", std::bind(&GCodeDispatch::cmd_HELP, this, std::placeholders::_1), true, m_cmd_HELP_help);
    m_gcode_is_lock = false;
}

GCodeDispatch::~GCodeDispatch()
{
}

bool GCodeDispatch::is_traditional_gcode(std::string cmd)
{
    transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    if (isupper(cmd[0]) && isdigit(cmd[1]))
        return true;
    else
        return false;
}

std::function<void(GCodeCommand &)> GCodeDispatch::register_command(std::string cmd, std::function<void(GCodeCommand &)> func, bool when_not_ready, std::string desc)
{
    if (func == nullptr)
    {
        std::function<void(GCodeCommand &)> old_cmd_fun = ready_gcode_handlers[cmd];
        std::map<std::string, std::function<void(GCodeCommand &)>>::iterator it;
        if (ready_gcode_handlers.find(cmd) != ready_gcode_handlers.end())
        {
            for (it = ready_gcode_handlers.begin(); it != ready_gcode_handlers.end();)
            {
                if (it->first == cmd)
                {
                    it = ready_gcode_handlers.erase(it);
                    std::cout << "ready_gcode_handlers.erase(it) " << std::endl;
                }
                else
                {
                    it++;
                }
            }
        }
        if (base_gcode_handlers.find(cmd) != base_gcode_handlers.end())
        {
            std::map<std::string, std::function<void(GCodeCommand &)>>::iterator it;
            for (it = base_gcode_handlers.begin(); it != base_gcode_handlers.end();)
            {
                if (it->first == cmd)
                {
                    it = base_gcode_handlers.erase(it);
                }
                else
                {
                    it++;
                }
            }
        }
        return old_cmd_fun;
    }
    auto ret = ready_gcode_handlers.find(cmd);
    if (ret != ready_gcode_handlers.end())
    {
        // error gcode command cmd already registered;
    }
    if (!this->is_traditional_gcode(cmd)) //----??----
    {
        // std::function<void(GCodeCommand&)> origfunc = func;
        // func = [this](GCodeCommand& params) -> GCodeCommand {this->get_extended_params(params);};  //----??----
    }
    ready_gcode_handlers[cmd] = func;
    if (when_not_ready)
        base_gcode_handlers[cmd] = func;
    if (desc != "")
        gcode_help[cmd] = desc;
    return nullptr;
}

void GCodeDispatch::register_mux_command(std::string cmd, std::string key, std::string value, std::function<void(GCodeCommand &)> func, std::string desc)
{
    register_command(cmd, func, false, desc); //--G-G-?--这里还需要完善
}

std::map<std::string, std::string> GCodeDispatch::get_command_help()
{
    return gcode_help;
}

void GCodeDispatch::register_output_handler(std::function<void(std::string)> cb)
{
    output_callbacks.push_back(cb);
}

void GCodeDispatch::handle_shutdown()
{
    if (!m_is_printer_ready)
        return;
    m_is_printer_ready = false;
    gcode_handlers = base_gcode_handlers; //----??----
    respond_state("Shutdown");
}

void GCodeDispatch::handle_disconnect()
{
    respond_state("Disconnect");
}

void GCodeDispatch::handle_ready()
{
    m_is_printer_ready = true;
    gcode_handlers = ready_gcode_handlers; //----??----
    respond_state("Ready");
}

void GCodeDispatch::process_command(std::vector<std::string> &commands, bool need_ack)
{
    // uint64_t start_time = hl_get_tick_us();
    for (int i = 0; i < commands.size(); i++)
    {
        // Ignore comments and leading/trailing spaces
        std::map<std::string, std::string> params;
        std::string cmd = "";
        std::string line = strip(commands[i]);
        if (line.size() == 0)
        {
            continue;
        }
        if (!is_traditional_gcode(line))
        {
            std::vector<std::string> parts;
            bool in_quotes = false; // 判断是否在双引号内
            std::string current_str;
            for (int i = 0; i < line.size(); i++)
            {
                if (line[i] == '\"')
                {
                    in_quotes = !in_quotes; // 判断是否在双引号内
                    current_str += line[i];
                }
                else if (line[i] == ' ' && !in_quotes) // 如果不在双引号内，按照空格分割字符串
                {
                    if (!current_str.empty())
                        parts.push_back(current_str);
                    current_str.clear();
                }
                else
                {
                    current_str += line[i];
                }
            }
            if (!current_str.empty())
                parts.push_back(current_str);

            cmd = parts[0];
            for (int i = 1; i < parts.size(); i++)
            {
                if (startswith(parts[i], ";") || startswith(parts[i], "#") || startswith(parts[i], "*")) // 忽略注释
                    break;
                std::vector<std::string> param_map;
                size_t pos = parts[i].find("=");
                if (pos > 0)
                {
                    std::string key = parts[i].substr(0, pos);
                    std::string value = parts[i].substr(pos + 1);
                    param_map.insert(param_map.begin(), value);
                    param_map.insert(param_map.begin(), key);
                }
                else
                {
                    std::vector<std::string> param_map = split(parts[i], "=");
                }
                // 如果以双引号开头，那么直到找到下一个双引号之前的所有内容都是参数的值。--hao--
                if (param_map.size() >= 2 && startswith(param_map[1], "\""))
                {
                    int j = i + 1;
                    while (j < parts.size())
                    {
                        param_map[1] += " " + parts[j];
                        if (endswith(parts[j], "\""))
                            break;
                        j++;
                    }
                    i = j;
                    // 去掉双引号
                    param_map[1] = param_map[1].substr(1, param_map[1].length() - 2);
                }
                if (param_map.size() >= 2)
                {
                    params[param_map[0]] = param_map[1];
                }
                else
                {
                    params[param_map[0]] = "";
                }
            }
        }
        else
        {
            std::vector<std::string> parts;
            std::string temp;
            transform(line.begin(), line.end(), line.begin(), ::toupper);
            for (char c : line)
            {
                if (std::isupper(c) || c == '_' || c == '*' || c == '/')
                {
                    if (!temp.empty())
                    {
                        parts.push_back(temp);
                        temp.clear();
                    }
                    parts.push_back(std::string(1, c));
                }
                else
                {
                    temp += c;
                }
            }
            if (!temp.empty())
            {
                parts.push_back(temp);
            }

            int numparts = parts.size();
            if (numparts >= 2 && parts[0] != "N")
            {
                cmd = parts[0] + strip(parts[1]);
            }
            else if (numparts >= 4 && parts[0] == "N")
            {
                cmd = parts[2] + strip(parts[3]);
            }
            for (int i = 0; i < parts.size(); i += 2)
            {
                if (i + 1 >= parts.size())
                    params[parts[i]] = " ";
                else
                    params[parts[i]] = parts[i + 1];
            }
        }
        // printf("cmd = %s\n", cmd.c_str());
        // printf("line = %s\n", line.c_str());
        // for (auto it : params)
        // {
        //     printf("params[%s] = %s\n", it.first.c_str(), it.second.c_str());
        // }
        GCodeCommand gcmd(cmd, line, params, need_ack);
        cmd.erase(std::remove_if(cmd.begin(), cmd.end(), [](unsigned char c)
                                 { return std::isspace(c) || c == '\0'; }),
                  cmd.end());
        // LOG_E("process_command time:%f\n", (hl_get_tick_us() - start_time) / 1000.0f);
        auto iter = gcode_handlers.find(cmd);
        if (iter != gcode_handlers.end())
        {
            try
            {
                // std::cout << "cmd : " << line << std::endl;
                // LOG_I("cmd : %s\n", line.c_str());
                
                iter->second(gcmd);
            }
            catch (const std::exception& e)
            {
                if (strstr(e.what(), "No response from MCU") != nullptr)
                    throw;
                LOG_E("error\n");
            }
        }
        else
        {
            try
            {
                cmd_default(gcmd);
            }
            catch (const std::exception& e)
            {
                if (strstr(e.what(), "No response from MCU") != nullptr)
                    throw;
                LOG_E("error\n");
            }
        }
        gcmd.ack();
    }
    // LOG_E("process_command time:%f\n", (hl_get_tick_us() - start_time) / 1000.0f);
}

void GCodeDispatch::run_script_from_command(std::vector<std::string> &script)
{
    this->process_command(script, false);
}

void GCodeDispatch::run_script(std::vector<std::string> &script)
{
    this->process_command(script, false);
}

void GCodeDispatch::run_script(std::string &script)
{
    std::vector<std::string> cmds;
    cmds.push_back(script);
    this->process_command(cmds, false);
}

void GCodeDispatch::cmd_default(GCodeCommand &gcmd)
{
    std::string cmd = gcmd.get_command();
    if (cmd == "M105")
    {
        // Don't warn about temperature requests when not ready
        gcmd.ack("T:0");
        return;
    }
    if (cmd == "M21")
    {
        // Don't warn about sd card init when not ready
        return;
    }
    if (!m_is_printer_ready)
    {
        // raise gcmd.error(self.printer.get_state_message()[0]) //----??----
        return;
    }
    if (cmd == "")
    {
        std::string cmdline = gcmd.get_commandline();
        if (cmdline != "")
        {
            std::cout << "note: cmd_default " << cmdline << std::endl;
        }
        return;
    }
    if (startswith(cmd, "M117 ") || startswith(cmd, "M118 "))
    {
        // Handle M117/M118 gcode with numeric and special characters
        auto iter = gcode_handlers.find(cmd.substr(0, 4));
        if (iter != gcode_handlers.end())
        {
            iter->second(gcmd);
            return;
        }
    }
    else if ((cmd == "M140" || cmd == "M104") && !gcmd.get_double("S", 0.))
    {
        // Don't warn about requests to turn off heaters when not present
        return;
    }
    else if (cmd == "M107" || (cmd == "M106" && (!gcmd.get_double("S", 1.) || m_is_fileinput)))
    {
        // Don't warn about requests to turn off fan when fan not present
        return;
    }
    // gcmd.m_respond_info("unknown command: " + cmd, true);
    // serial_info("unknown command: " + cmd);
    std::cout << "unknown command: " << cmd << std::endl;
    // std::cout << "\r\n" << std::endl;
}

std::mutex GCodeDispatch::get_mutex()
{
    // return m_mutex;
}

GCodeCommand GCodeDispatch::create_gcode_command(std::string command, std::string commandline, std::map<std::string, std::string> &params)
{
    // GCodeCommand gcmd = new GCodeCommand(command, commandline, params, false);       //内存泄漏		//----------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:create_gcode_command",0);
    return GCodeCommand(command, commandline, params, false);
}

// Response handling
void GCodeDispatch::respond_raw(std::string msg)
{
    // for(int i = 0; i < output_callbacks.size(); i++)
    // {
    //     output_callbacks[i](msg);
    // }
    LOG_I("%s\n", msg.c_str());
}

void GCodeDispatch::respond_info(std::string msg, bool log)
{
    LOG_I("%s\n", msg.c_str());
}

void GCodeDispatch::respond_error(std::string msg)
{
}

void GCodeDispatch::respond_state(std::string state)
{
}

GCodeCommand &GCodeDispatch::get_extended_params(GCodeCommand &gcmd) //??
{
    // std::string extended = R"(^\s*(?:N[0-9]+\s*)?(?P<cmd>[a-zA-Z_][a-zA-Z0-9_]+)(?:\s+|$)(?P<args>[^#*;]*?)\s*(?:[#*;].*)?$)";
    // std::regex extended_r = (std::regex)extended;

    std::string cmd = gcmd.get_commandline();
    std::vector<std::string> params = split(cmd, " ");
    if (startswith(params[0], "N"))
    {
        for (int i = 2; i < params.size(); i++)
        {
            if (startswith(params[i], ";") || startswith(params[i], "#") || startswith(params[i], "*"))
                break;
            std::vector<std::string> map = split(params[i], "=");
            gcmd.m_params[map[0]] = map[1];
        }
    }
}

void GCodeDispatch::cmd_mux(GCodeCommand &gcmd)
{
    // auto a = mux_commands.find(command);   // ----??----
    // if(values == none)
    // {
    //     key_param = gcmd.get(key, "");
    // }
}

void GCodeDispatch::cmd_G(GCodeCommand &gcmd)
{
    std::cout << "list of G command" << std::endl;
    for (auto handler : gcode_handlers)
    {
        if (startswith(handler.first, "G"))
        {
            // std::cout << handler.first << std::endl;
            gcmd.m_respond_raw(handler.first);
        }
    }
}

void GCodeDispatch::cmd_M(GCodeCommand &gcmd)
{
    std::cout << "list of M command" << std::endl;
    for (auto handler : gcode_handlers)
    {
        if (startswith(handler.first, "M"))
        {
            gcmd.m_respond_raw(handler.first);
            // std::cout << handler.first << std::endl;
        }
    }
}
// Low-level G-Code commands that are needed before the config file is loaded
void GCodeDispatch::cmd_M110(GCodeCommand &gcmd)
{
    // set current line number
    return;
}

void GCodeDispatch::cmd_M112(GCodeCommand &gcmd)
{
    // Emergency Stop
    Printer::GetInstance()->invoke_shutdown("shutdown due to M112 command");
}

void GCodeDispatch::cmd_M115(GCodeCommand &gcmd)
{
    // Get Firmware Version and Capabilities

    // software_version = self.printer.get_start_args().get('software_version')
    // kw = {"FIRMWARE_NAME": "Klipper", "FIRMWARE_VERSION": software_version}
    // msg = " ".join(["%s:%s" % (k, v) for k, v in kw.items()])
    // did_ack = gcmd.ack(msg)
    // if not did_ack:
    //     gcmd.m_respond_info(msg)
}

void GCodeDispatch::request_restart(std::string result)
{
    if (m_is_printer_ready)
    {
        double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
        if (result == "exit")
            std::cout << "exiting (print time " << print_time << "s)" << std::endl;
        Printer::GetInstance()->send_event("gcode:request_restart", print_time);
        Printer::GetInstance()->m_tool_head->dwell(0.500);
        Printer::GetInstance()->m_tool_head->wait_moves();
    }
}

void GCodeDispatch::cmd_RESTART(GCodeCommand &gcmd)
{
    this->request_restart("restart");
}

void GCodeDispatch::cmd_FIRMWARE_RESTART(GCodeCommand &gcmd)
{
    this->request_restart("firmware_restart");
}

void GCodeDispatch::cmd_ECHO(GCodeCommand &gcmd)
{
    gcmd.m_respond_info(gcmd.get_commandline(), false);
}

void GCodeDispatch::cmd_STATUS(GCodeCommand &gcmd)
{
    if (m_is_printer_ready)
    {
        this->respond_state("Ready");
        return;
    }
    // std::string msg = self.printer.get_state_message()[0]; //----??----
    // msg = msg.rstrip() + "\nKlipper state: Not ready"
    // raise gcmd.error(msg)
}

void GCodeDispatch::cmd_HELP(GCodeCommand &gcmd)
{
    std::vector<std::string> cmdhelp;
    if (!m_is_printer_ready)
    {
        cmdhelp.push_back("Printer is not ready - not all commands available.");
    }
    cmdhelp.push_back("Available extended commands:");

    for (auto it : gcode_handlers)
    {
        auto it_help = gcode_help.find(it.first);
        if (it_help != gcode_help.end())
        {
            cmdhelp.push_back(it.first + ": " + it_help->second);
        }
    }
    // gcmd.m_respond_info(cmdhelp, false);
}

GCodeIO::GCodeIO()
{
    Printer::GetInstance()->register_event_handler("klippy:ready:GCodeIO", std::bind(&GCodeIO::_handle_ready, this));
    Printer::GetInstance()->register_event_handler("klippy:shutdown:GCodeIO", std::bind(&GCodeIO::_handle_shutdown, this));
    m_print_file_size = 1;
    m_print_file_current_pos = 0;
    m_is_fileinput = true;
    m_fd = -1;
}

GCodeIO::~GCodeIO()
{
}

void GCodeIO::_handle_ready()
{
    // m_is_printer_ready = true;
    // if (m_is_fileinput && m_fd_handle == nullptr)
    // {
    //     m_fd_handle = Printer::GetInstance()->register_fd(m_fd, self._process_data);
    // }
}
void GCodeIO::_dump_debug()
{
    // out = []
    // out.append("Dumping gcode input %d blocks" % (len(self.input_log),))
    // for eventtime, data in self.input_log:
    //     out.append("Read %f: %s" % (eventtime, repr(data)))
    // logging.info("\n".join(out))
}
void GCodeIO::_handle_shutdown()
{
    // if not self.is_printer_ready:
    //     return
    // self.is_printer_ready = False
    // self._dump_debug()
    // if self.is_fileinput:
    //     self.printer.request_exit('error_exit')
}

void GCodeIO::_process_data(double eventtime)
{
    // # Read input, separate by newline, and add to pending_commands
    // try:
    //     data = os.read(self.fd, 4096)
    // except os.error:
    //     logging.exception("Read g-code")
    //     return
    // self.input_log.append((eventtime, data))
    // self.bytes_read += len(data)
    // lines = data.split('\n')
    // lines[0] = self.partial_input + lines[0]
    // self.partial_input = lines.pop()
    // pending_commands = self.pending_commands
    // pending_commands.extend(lines)
    // self.pipe_is_active = True
    // # Special handling for debug file input EOF
    // if not data and self.is_fileinput:
    //     if not self.is_processing_data:
    //         self.reactor.unregister_fd(self.fd_handle)
    //         self.fd_handle = None
    //         self.gcode.request_restart('exit')
    //     pending_commands.append("")
    // # Handle case where multiple commands pending
    // if self.is_processing_data or len(pending_commands) > 1:
    //     if len(pending_commands) < 20:
    //         # Check for M112 out-of-order
    //         for line in lines:
    //             if self.m112_r.match(line) is not None:
    //                 self.gcode.cmd_M112(None)
    //     if self.is_processing_data:
    //         if len(pending_commands) >= 20:
    //             # Stop reading input
    //             self.reactor.unregister_fd(self.fd_handle)
    //             self.fd_handle = None
    //         return
    // # Process commands
    // self.is_processing_data = True
    // while pending_commands:
    //     self.pending_commands = []
    //     with self.gcode_mutex:
    //         self.gcode._process_commands(pending_commands)
    //     pending_commands = self.pending_commands
    // self.is_processing_data = False
    // if self.fd_handle is None:
    //     self.fd_handle = self.reactor.register_fd(self.fd,
    //                                                 self._process_data)
}
void GCodeIO::_respond_raw(std::string msg)
{
    // if self.pipe_is_active:
    //     try:
    //         os.write(self.fd, msg+"\n")
    //     except os.error:
    //         logging.exception("Write g-code response")
    //         self.pipe_is_active = False
}
void GCodeIO::stats(double eventtime)
{
    // return False, "gcodein=%d" % (self.bytes_read,);
}

void GCodeIO::open_file(std::string filename)
{
    if (!m_read_gcode_file.is_open())
    {
        std::cout << "start read file " << std::endl;
        m_read_gcode_file.open(filename.c_str());
        m_read_gcode_file.seekg(0, std::ios::end);
        m_print_file_size = m_read_gcode_file.tellg();
        m_read_gcode_file.seekg(0, std::ios::beg);
        if (!m_read_gcode_file.is_open())
        {
            return;
        }
    }
}

void GCodeIO::read_file()
{
    if (!m_read_gcode_file.eof() && print_gcode == true) //
    {
        m_print_file_current_pos = m_read_gcode_file.tellg();
        std::string command_line;
        std::vector<std::string> cmds;
        int read_len = 0;
        while (read_len < 4096 && !m_read_gcode_file.eof())
        {
            std::istream &ifstream = getline(m_read_gcode_file, command_line);
            read_len = m_read_gcode_file.tellg() - m_print_file_current_pos;
            cmds.push_back(command_line);
        }
        Printer::GetInstance()->m_gcode->process_command(cmds);
    }
    else if (m_read_gcode_file.eof())
    {
        m_read_gcode_file.close();
    }
}

void GCodeIO::single_command(std::string cmd)
{
    std::vector<std::string> cmds;
    cmds.push_back(cmd);
    Printer::GetInstance()->m_gcode->process_command(cmds);
    return;
}

/// @brief 不定长参数，格式化传入参数，但是注意该函数不是线程安全函数，会存在多线程竞争。目前调用该接口保证在打印线程，不要在别地方调用。
/// @param fmt
/// @param
void GCodeIO::single_command(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    LOG_I("single_command<%s>\n", buf);
    std::vector<std::string> cmds;
    cmds.push_back(buf);
    Printer::GetInstance()->m_gcode->process_command(cmds);
    return;
}
