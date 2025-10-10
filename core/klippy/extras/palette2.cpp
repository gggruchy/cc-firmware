
#include "palette2.h"
#include "klippy.h"
#include "my_string.h"

#define HEARTBEAT_SEND 5.
#define HEARTBEAT_TIMEOUT (HEARTBEAT_SEND * 2.) + 1.
#define SETUP_TIMEOUT 10

#define SERIAL_TIMER 0.1
#define AUTOLOAD_TIMER 5.



Palette2::Palette2(std::string section_name)
{
    m_COMMAND_CLEAR = {
    "O10 D5",
    "O10 D0 D0 D0 DFFE1",
    "O10 D1 D0 D0 DFFE1",
    "O10 D2 D0 D0 DFFE1",
    "O10 D3 D0 D0 DFFE1",
    "O10 D4 D0 D0 D0069"};
    m_COMMAND_HEARTBEAT = "O99";
    m_COMMAND_CUT = "O10 D5";
    m_COMMAND_FILENAME = "O51";
    m_COMMAND_FILENAMES_DONE = "O52";
    m_COMMAND_FIRMWARE = "O50";
    m_COMMAND_PING = "O31";
    m_COMMAND_SMART_LOAD_STOP = "O102 D1";
    m_INFO_NOT_CONNECTED = "Palette 2 is not connected, connect first";

    m_cmd_Connect_Help = "Connect to the Palette 2";
    m_cmd_Disconnect_Help = "Disconnect from the Palette 2";
    m_cmd_Clear_Help = "Clear the input and output of the Palette 2";
    m_cmd_Cut_Help = "Cut the outgoing filament";
    m_cmd_Smart_Load_Help = "Automatically load filament through the extruder";
    Printer::GetInstance()->m_gcode->register_command("PALETTE_CONNECT", std::bind(&Palette2::cmd_Connect, this, std::placeholders::_1), false, m_cmd_Connect_Help);
    Printer::GetInstance()->m_gcode->register_command("PALETTE_DISCONNECT", std::bind(&Palette2::cmd_Disconnect, this, std::placeholders::_1), false, m_cmd_Disconnect_Help);
    Printer::GetInstance()->m_gcode->register_command("PALETTE_CLEAR", std::bind(&Palette2::cmd_Clear, this, std::placeholders::_1), false, m_cmd_Clear_Help);
    Printer::GetInstance()->m_gcode->register_command("PALETTE_CUT", std::bind(&Palette2::cmd_Cut, this, std::placeholders::_1), false, m_cmd_Cut_Help);
    Printer::GetInstance()->m_gcode->register_command("PALETTE_SMART_LOAD", std::bind(&Palette2::cmd_Smart_Load, this, std::placeholders::_1), false, m_cmd_Smart_Load_Help);
    // m_serial = None
    // m_serial_port = config.get("serial")
    // if not m_serial_port:
    //     raise config.error("Invalid serial port specific for Palette 2")
    m_baud = Printer::GetInstance()->m_pconfig->GetInt(section_name, "baud", 115200);
    m_feedrate_splice = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "feedrate_splice", 0.8, 0., 1.);
    m_feedrate_normal = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "feedrate_normal", 1.0, 0., 1.);
    m_auto_load_speed = Printer::GetInstance()->m_pconfig->GetInt(section_name, "auto_load_speed", 2);
    m_auto_cancel_variation = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "auto_cancel_variation", DBL_MIN, 0.01, 0.2);

    m_cmd_O1_help = "Initialize the print, and check connection with the Palette 2";
    m_cmd_O9_help = "Reset print information";
    // Omega code matchers
    std::vector<std::string> omega_handlers;
    for (int i = 0; i < 33; i++)
    {
        if (i == 0 || i < 1 && i < 9 || i > 9 && i < 21)
        {
            omega_handlers.push_back("O" + std::to_string(i));
        }
    }
    for (auto cmd : omega_handlers)
    {
        Printer::GetInstance()->m_gcode->register_command(cmd, std::bind(&Palette2::cmd_OmegaDefault, this, std::placeholders::_1));
    }

    Printer::GetInstance()->m_gcode->register_command("O1", std::bind(&Palette2::cmd_O1, this, std::placeholders::_1), false, m_cmd_O1_help);
    Printer::GetInstance()->m_gcode->register_command("O9", std::bind(&Palette2::cmd_O9, this, std::placeholders::_1), false, m_cmd_O9_help);
    Printer::GetInstance()->m_gcode->register_command("O21", std::bind(&Palette2::cmd_O21, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O22", std::bind(&Palette2::cmd_O22, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O23", std::bind(&Palette2::cmd_O23, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O24", std::bind(&Palette2::cmd_O24, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O25", std::bind(&Palette2::cmd_O25, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O26", std::bind(&Palette2::cmd_O26, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O27", std::bind(&Palette2::cmd_O27, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O28", std::bind(&Palette2::cmd_O28, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O29", std::bind(&Palette2::cmd_O29, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O30", std::bind(&Palette2::cmd_O30, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O31", std::bind(&Palette2::cmd_O31, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("O32", std::bind(&Palette2::cmd_O32, this, std::placeholders::_1));

    _reset();
    m_read_buffer = "";
    m_heartbeat = 0.;
    m_signal_disconnect = false;
    m_is_printing = false;
}

Palette2::~Palette2()
{

}
    
void Palette2::_reset()
{
    m_is_setup_complete = false;
    m_is_splicing = false;
    m_is_loading = false;
    m_remaining_load_length = 0;
    m_omega_algorithms_counter = 0;
    m_omega_splices_counter = 0;
    m_omega_header_counter = 0;
    m_omega_last_command = "";
}
    
bool Palette2::_check_P2(GCodeCommand* gcmd)
{
    if (m_serial != nullptr)
        return true;
    // gcmd.m_respond_info(INFO_NOT_CONNECTED);
    return false;
}

void Palette2::cmd_Connect(GCodeCommand& gcmd)
{
    if (m_serial != nullptr)
    {
        // gcmd.m_respond_info("Palette 2 serial port is already active, disconnect first");
        return;
    }
    m_signal_disconnect = false;
    // // logging.info("Connecting to Palette 2 on port (%s) at (%s)" % (m_serial_port, m_baud))
    // try:
    //     m_serial = serial.Serial(
    //         m_serial_port, m_baud, timeout=0, write_timeout=0)
    // except SerialException:
    //     gcmd.respond_info("Unable to connect to the Palette 2")
    //     return

    // with m_write_queue.mutex:
    //     m_write_queue.queue.clear()
    // with m_read_queue.mutex:
    //     m_read_queue.queue.clear()

    m_read_timer = Printer::GetInstance()->m_reactor->register_timer("read_timer", std::bind(&Palette2::_run_Read, this, std::placeholders::_1), Printer::GetInstance()->m_reactor->m_NOW);
    m_write_timer = Printer::GetInstance()->m_reactor->register_timer("write_timer", std::bind(&Palette2::_run_Write, this, std::placeholders::_1), Printer::GetInstance()->m_reactor->m_NOW);
    m_heartbeat_timer = Printer::GetInstance()->m_reactor->register_timer("heartbeat_timer", std::bind(&Palette2::_run_Heartbeat, this, std::placeholders::_1), Printer::GetInstance()->m_reactor->m_NOW);

    // Tell the device we're alive
    m_write_queue.push_back("\n");
    m_write_queue.push_back(m_COMMAND_FIRMWARE);
    _wait_for_heartbeat();
}
        
void Palette2::cmd_Disconnect(GCodeCommand& gmcd)
{
    // m_gcode.respond_info("Disconnecting from Palette 2")
    if (m_serial != nullptr)
    {
        // m_serial.close();
        m_serial = nullptr;
    } 
    Printer::GetInstance()->m_reactor->unregister_timer(m_read_timer);
    Printer::GetInstance()->m_reactor->unregister_timer(m_write_timer);
    Printer::GetInstance()->m_reactor->unregister_timer(m_heartbeat_timer);
    m_read_timer = nullptr;
    m_write_timer = nullptr;
    m_heartbeat = 0.;
    m_is_printing = false;
}
    
void Palette2::cmd_Clear(GCodeCommand& gcmd)
{
    // // logging.info("Clearing Palette 2 input and output")
    if (_check_P2(&gcmd))
    {
        for (auto l : m_COMMAND_CLEAR)
        {
            m_write_queue.push_back(l);
        }
    }
}

void Palette2::cmd_Cut(GCodeCommand& gcmd)
{
    // // logging.info("Cutting outgoing filament in Palette 2")
    if (_check_P2(&gcmd))
    {
        m_write_queue.push_back(m_COMMAND_CUT);
    }
}

void Palette2::cmd_Smart_Load(GCodeCommand& gcmd)
{
    if (_check_P2(&gcmd))
    {
        if (!m_is_loading)
        {
            gcmd.m_respond_info("Cannot auto load when the Palette 2 is not ready", true);
            return;
        }
        p2cmd_O102(std::vector<std::string>());
    }   
}
    
void Palette2::cmd_OmegaDefault(GCodeCommand& gcmd)
{
    // // logging.debug("Omega Code: %s" % (gcmd.get_command()))
    if (_check_P2(&gcmd))
    {
        m_write_queue.push_back(gcmd.get_commandline());
    }
}
        
void Palette2::_wait_for_heartbeat()
{
    double startTs = get_monotonic();
    double currTs = startTs;
    while (m_heartbeat == 0 && m_heartbeat < (currTs - SETUP_TIMEOUT) && startTs > (currTs - SETUP_TIMEOUT))
        currTs = Printer::GetInstance()->m_reactor->pause(currTs + 1.);

    if (m_heartbeat < (currTs - SETUP_TIMEOUT))
    {
        m_signal_disconnect = true;
        // raise m_printer.command_error("No response from Palette 2")
    }   
}
        
void Palette2::cmd_O1(GCodeCommand& gcmd)
{
    // // logging.info("Initializing print with Pallete 2")
    if (!_check_P2(&gcmd))
    {
        // raise m_printer.command_error("Cannot initialize print, palette 2 is not connected")
    }
    Printer::GetInstance()->m_reactor->update_timer(m_heartbeat_timer, Printer::GetInstance()->m_reactor->m_NOW);
    _wait_for_heartbeat();
    m_write_queue.push_back(gcmd.get_commandline());
    // m_gcode.respond_info( "Palette 2 waiting on user to complete setup")
    Printer::GetInstance()->m_pause_resume->send_pause_command();
}
        
void Palette2::cmd_O9(GCodeCommand& gcmd)
{
    // // logging.info("Print finished, resetting Palette 2 state")
    if (!_check_P2(&gcmd))
    {
        m_write_queue.push_back(gcmd.get_commandline());
    }
    m_is_printing = false;
}

void Palette2::cmd_O21(GCodeCommand& gcmd)
{
    // // logging.debug("Omega version: %s" % (gcmd.get_commandline()))
    _reset();
    m_omega_header[0] = gcmd.get_commandline();
    m_is_printing = true;
}
    

void Palette2::cmd_O22(GCodeCommand& gcmd)
{
    // // logging.debug("Omega printer profile: %s" % (gcmd.get_commandline()))
    m_omega_header[1] = gcmd.get_commandline();
}
        

void Palette2::cmd_O23(GCodeCommand& gcmd)
{
    // logging.debug("Omega slicer profile: %s" % (gcmd.get_commandline()))
    m_omega_header[2] = gcmd.get_commandline();
}
        

void Palette2::cmd_O24(GCodeCommand& gcmd)
{
    // logging.debug("Omega PPM: %s" % (gcmd.get_commandline()))
    m_omega_header[3] = gcmd.get_commandline();
}

void Palette2::cmd_O25(GCodeCommand& gcmd)
{
    // logging.debug("Omega inputs: %s" % (gcmd.get_commandline()))
    m_omega_header[4] = gcmd.get_commandline();
    std::vector<std::string> drives = split(m_omega_header[4].substr(4), " ");
    for (int idx = 0; idx < drives.size(); idx++)
    {
        std::string state = drives[idx].substr(0, 2);
        if (state == "D1")
            drives[idx] = "U" + std::to_string(60 + idx);
    }
    for (auto d : drives)
    {
        if (d != "D0")
        {
            m_omega_drives.push_back(d);
        }
    }
    // logging.info("Omega drives: %s" % m_omega_drives)
}
    

void Palette2::cmd_O26(GCodeCommand& gcmd)
{
    // logging.debug("Omega splices %s" % (gcmd.get_commandline()))
    m_omega_header[5] = gcmd.get_commandline();
}

void Palette2::cmd_O27(GCodeCommand& gcmd)
{
    // logging.debug("Omega pings: %s" % (gcmd.get_commandline()))
    m_omega_header[6] = gcmd.get_commandline();
}

void Palette2::cmd_O28(GCodeCommand& gcmd)
{
    // logging.debug("Omega MSF NA: %s" % (gcmd.get_commandline()))
    m_omega_header[7] = gcmd.get_commandline();
}
        

void Palette2::cmd_O29(GCodeCommand& gcmd)
{
    // logging.debug("Omega MSF NH: %s" % (gcmd.get_commandline()))
    m_omega_header[8] = gcmd.get_commandline();
}
        

void Palette2::cmd_O30(GCodeCommand& gcmd)
{
    std::string param_drive = gcmd.get_commandline().substr(5, 1);
    std::string param_distance = gcmd.get_commandline().substr(8);
    std::pair<int, std::string> splice = {stoi(param_drive), param_distance};
    m_omega_splices.push_back(splice);
}

void Palette2::cmd_O31(GCodeCommand& gcmd)
{
    if (_check_P2(&gcmd))
    {
        m_omega_current_ping = gcmd.get_commandline();
        // logging.debug("Omega ping command: %s" % (gcmd.get_commandline()))
        m_write_queue.push_back(m_COMMAND_PING);
        std::map<std::string, std::string> params = {{"P", "10"}};
        Printer::GetInstance()->m_gcode->create_gcode_command("G4", "G4", params);
    } 
} 

void Palette2::cmd_O32(GCodeCommand& gcmd)
{
    // logging.debug("Omega algorithm: %s" % (gcmd.get_commandline()))
    m_omega_algorithms.push_back(gcmd.get_commandline());
}
        

void Palette2::p2cmd_O20(std::vector<std::string> params)
{
    if (!m_is_printing)
        return;

    // First print, we can ignore
    if (params[0] == "D5")
    {
        // logging.info("First print on Palette")
        return;
    }
    int n = stoi(params[0].substr(1));

    if (n == 0)
    {
        // logging.info("Sending omega header %s" % m_omega_header_counter)
        m_write_queue.push_back(m_omega_header[m_omega_header_counter]);
        m_omega_header_counter = m_omega_header_counter + 1;
    }
    else if (n == 1)
    {
        // logging.info("Sending splice info %s" % m_omega_splices_counter)
        std::pair<int, std::string> splice = m_omega_splices[m_omega_splices_counter];
        m_write_queue.push_back("O30 D" + std::to_string(splice.first) + "D" + splice.second);
        m_omega_splices_counter = m_omega_splices_counter + 1;
    }
    else if (n == 2)
    {
        // logging.info("Sending current ping info %s" % m_omega_current_ping)
        m_write_queue.push_back(m_omega_current_ping);
    }
    else if (n == 4)
    {
        // logging.info("Sending algorithm info %s" % m_omega_algorithms_counter)
        m_write_queue.push_back(m_omega_algorithms[m_omega_algorithms_counter]);
        m_omega_algorithms_counter = m_omega_algorithms_counter + 1;
    }
    else if (n == 8)
    {
        // logging.info("Resending the last command to Palette 2")
        m_write_queue.push_back(m_omega_last_command);
    }
        
}
    
void Palette2::check_ping_variation(double last_ping)
{
    if (m_auto_cancel_variation != 0)
    {
        double ping_max = 100. + (m_auto_cancel_variation * 100.);
        double ping_min = 100. - (m_auto_cancel_variation * 100.);
        if (last_ping < ping_min || last_ping > ping_max)
        {
            // logging.info("Ping variation is too high, " "cancelling print")
            std::vector<std::string> scripts = {"CANCEL_PRINT"};
            Printer::GetInstance()->m_gcode->run_script(scripts);
        }
    }
}

void Palette2::p2cmd_O34(std::vector<std::string> params)
{
    if (!m_is_printing)
        return;

    if (params.size() > 2)
    {
        double percent = stof(params[1].substr(1));
        if (params[0] == "D1")
        {
            int number = m_omega_pings.size() + 1;
            // logging.info("Ping %d, %d percent" % (number, percent))
            std::pair<int, double> d = {number, percent};
            m_omega_pings.push_back(d);
            check_ping_variation(percent);
        }   
        else if (params[0] == "D2")
        {
            int number = m_omega_pongs.size() + 1;
            std::pair<int, double> d = {number, percent};
            // logging.info("Pong %d, %d percent" % (number, percent))
            m_omega_pongs.push_back(d);
        }
    }
}
    

void Palette2::p2cmd_O40(std::vector<std::string> params)
{
    // logging.info("Resume request from Palette 2")
    Printer::GetInstance()->m_pause_resume->send_resume_command();
}
    

void Palette2::p2cmd_O50(std::vector<std::string> params)
{
    if (params.size() > 1)
    {
        std::string fw = params[0].substr(1);
        // logging.info("Palette 2 firmware version %s detected" % os.fwalk)
        if (fw < "9.0.9")
        {
            // raise m_printer.command_error( "Palette 2 firmware version is too old,  update to at least 9.0.9")
        }
            
    }
    else
    {
        // std::map<std::string, int> file_list = Printer::GetInstance()->m_virtual_sdcard->get_file_list(true);
        // for (auto file : file_list)
        // {
        //     if (file.first.find(".mcf.gcode") < file.first.size())
        //     {
        //         m_files.push_back(file.first);
        //     }
        // }
        // for (auto file : m_files)
        // {
        //     m_write_queue.push_back(m_COMMAND_FILENAME + " D" + file);
        // }
        // m_write_queue.push_back(m_COMMAND_FILENAMES_DONE);
    } 
}
    

void Palette2::p2cmd_O53(std::vector<std::string> params)
{
    if (params.size() > 1 && params[0] == "D1")
    {
        int idx = stoi(params[1].substr(1));
        std::vector<std::string> files = m_files;
        reverse(files.begin(), files.end());
        std::string file = files[idx];
        std::vector<std::string> script = {"SDCARD_PRINT_FILE FILENAME=" + file};
        Printer::GetInstance()->m_gcode->run_script(script);
    }
}
        

void Palette2::p2cmd_O88(std::vector<std::string> params)
{
    // logging.error("Palette 2 error detected")
    // try:
    //     error = int(params[0][1:], 16)
    //     logging.error("Palette 2 error code %d" % error)
    // except (TypeError, IndexError):
    //     logging.error("Unable to parse Palette 2 error")
}


void Palette2::printCancelling(std::vector<std::string> params)
{
    // logging.info("Print Cancelling")
    std::vector<std::string> script = {"CLEAR_PAUSE"};
    Printer::GetInstance()->m_gcode->run_script(script);
    script = {"CANCEL_PRINT"};
    Printer::GetInstance()->m_gcode->run_script(script);
}
            

void Palette2::printCancelled(std::vector<std::string> params)
{
    // logging.info("Print Cancelled")
    _reset();
}     

void Palette2::loadingOffsetStart(std::vector<std::string> params)
{
    // logging.info("Waiting for user to load filament into printer")
    m_is_loading = true;
}
        

void Palette2::loadingOffset(std::vector<std::string> params)
{
    m_remaining_load_length = stoi(params[1].substr(1));
    // logging.debug("Loading filamant remaining %d" % m_remaining_load_length)
    if (m_remaining_load_length >= 0 && m_smart_load_timer)
    {
        // logging.info("Smart load filament is complete")
        Printer::GetInstance()->m_reactor->unregister_timer(m_smart_load_timer);
        m_smart_load_timer = nullptr;
        m_is_loading = false;
    }
}

void Palette2::feedrateStart(std::vector<std::string> params)
{
    // logging.info("Setting feedrate to %f for splice" % m_feedrate_splice)
    m_is_splicing = true;
    std::vector<std::string> script = {"M220 S" + std::to_string(m_feedrate_splice * 100)};
    Printer::GetInstance()->m_gcode->run_script(script);
}
    
void Palette2::feedrateEnd(std::vector<std::string> params)
{   
    // logging.info("Setting feedrate to %f splice done" % m_feedrate_normal)
    m_is_splicing = false;
    std::vector<std::string> script = {"M220 S" + std::to_string(m_feedrate_normal * 100)};
    Printer::GetInstance()->m_gcode->run_script(script);
}
    

void Palette2::p2cmd_O97(std::vector<std::string> params)
{
    // matchers = []
    // if m_is_printing:
    //     matchers = matchers + [
    //         [printCancelling, 2, "U0", "D2"],
    //         [printCancelled, 2, "U0", "D3"],
    //         [loadingOffset, 2, "U39"],
    //         [loadingOffsetStart, 1, "U39"],
    //     ]

    // matchers.append([feedrateStart, 3, "U25", "D0"])
    // matchers.append([feedrateEnd, 3, "U25", "D1"])
    // _param_Matcher(matchers, params)
}
    
void Palette2::p2cmd_O100(std::vector<std::string> params)
{   
    // logging.info("Pause request from Palette 2")
    m_is_setup_complete = true;
    Printer::GetInstance()->m_pause_resume->send_pause_command();
}
        

void Palette2::p2cmd_O102(std::vector<std::string> params)
{
    if (!Printer::GetInstance()->m_tool_head->get_extruder()->get_heater()->m_can_extrude)
    {
        m_write_queue.push_back(m_COMMAND_SMART_LOAD_STOP);
        // m_gcode.respond_info("Unable to auto load filament, extruder is below minimum temp");
        return;
    }
    if (m_smart_load_timer == nullptr)
    {
        // logging.info("Smart load starting")
        m_smart_load_timer = Printer::GetInstance()->m_reactor->register_timer("smart_load_timer", std::bind(&Palette2::_run_Smart_Load, this, std::placeholders::_1), Printer::GetInstance()->m_reactor->m_NOW);
    }
        
}
        

void Palette2::p2cmd(std::string line)
{
    std::vector<std::string> t = split(line, " ");
    std::string ocode = t[0];
    std::vector<std::string> params(t.begin() + 1, t.end());
    int params_count = params.size();
    if (params_count != 0)
    {
        std::vector<std::string> res;
        for (auto i : params)
        {
            if (i[0] == 'D' || i[0] == 'U')
                res.push_back(i);
            for (auto r : res)
            {
                if (r == "0" || r == "false" || r == "")
                {
                    // logging.error("Omega parameters are invalid")
                    return;
                }
            }

        }    
    }
    // func = getattr(self, 'p2cmd_' + ocode, None)
    // if func is not None:
    //     func(params)
}
        

bool Palette2::_param_Matcher(std::vector<std::vector<std::string>> matchers, std::vector<std::string> params)
{
    //Match the command with the handling table
    for (auto matcher : matchers)
    {
        if (params.size() >= stoi(matcher[1]))
        {
            std::vector<std::string> match_params(matcher.begin() + 2, matcher.end());
            bool res = true;
            for (int i = 0; i < match_params.size(); i++)
            {
                if(match_params[i] != params[i])
                {
                    res = false;
                }
            }
            if (res)
            {
                // matcher[0](params);
                return true;
            }
        }
    }
    return false;
}
    

double Palette2::_run_Read(double eventtime)
{
    if (m_signal_disconnect)
    {
        std::map<std::string, std::string> params;
        GCodeCommand gcmd("", "", params, false);
        cmd_Disconnect(gcmd);
        return Printer::GetInstance()->m_reactor->m_NEVER;
    }

    // Do non-blocking reads from serial and try to find lines
    while (true)
    {
        // raw_bytes = m_serial.read()
        std::string raw_bytes;
        if (raw_bytes.size())
        {
            std::string text_buffer = m_read_buffer + raw_bytes;
            while (true)
            {
                if (int i = text_buffer.find("\n") < text_buffer.size())
                {
                    std::string line = text_buffer.substr(0, i+1);
                    m_read_queue.push_back(line);
                    text_buffer = text_buffer.substr(i+1);
                }
                else
                    break;
            }
            m_read_buffer = text_buffer;
        }
        else
            break;
    }
    // Process any decoded lines from the device
    while (m_read_queue.size())
    {
        std::string text_line = m_read_queue.back();
        m_read_queue.pop_back();
        std::vector<std::string> heartbeat_strings = {m_COMMAND_HEARTBEAT, "Connection Okay"};
        int flag = 0;
        for (auto x : heartbeat_strings)
        {
            if (text_line.find(x) < text_line.size())
            {
                flag = 1;
            }
        }
        if (!flag)
        {
            // logging.debug("%0.3f P2 -> : %s" %(eventtime, text_line))
        }

        // Received a heartbeat from the device
        if (text_line == m_COMMAND_HEARTBEAT)
            m_heartbeat = eventtime;

        else if (text_line[0] == 'O')
            p2cmd(text_line);
    }
    return eventtime + SERIAL_TIMER;
}
    
double Palette2::_run_Heartbeat(double eventtime)
{
    m_write_queue.push_back(m_COMMAND_HEARTBEAT);
    eventtime = Printer::GetInstance()->m_reactor->pause(eventtime + 5);
    if (m_heartbeat && m_heartbeat < (eventtime - HEARTBEAT_TIMEOUT))
    {
        // logging.error("P2 has not responded to heartbeat")
        if (!m_is_printing || m_is_setup_complete)
        {
            std::map<std::string, std::string> params;
            GCodeCommand gcmd("", "", params, false);
            cmd_Disconnect(gcmd);
            return Printer::GetInstance()->m_reactor->m_NEVER;
        }
            
    }
    return eventtime + HEARTBEAT_SEND;
}
    

double Palette2::_run_Write(double eventtime)
{
    while (m_write_queue.size())
    {
        std::string text_line = m_write_queue.back();
        m_write_queue.pop_back();
        if (text_line != "")
        {
            m_omega_last_command = text_line;
            std::string l = strip(text_line, " ");
            if (l.find(m_COMMAND_HEARTBEAT) >= l.size())
            {
                // logging.debug( "%s -> P2 : %s" % (m_reactor.monotonic(), l))
            }
            std::string terminated_line = l + "\n";
            // m_serial->write(terminated_line.encode());
        }
    }
    return eventtime + SERIAL_TIMER;
}

double Palette2::_run_Smart_Load(double eventtime)
{
    if (!m_is_splicing && m_remaining_load_length < 0)
    {
        // Make sure toolhead class isn't busy
        std::vector<double> ret = Printer::GetInstance()->m_tool_head->check_busy(eventtime);
        double print_time = ret[0];
        double est_print_time = ret[1];
        double lookahead_empty = ret[2];
        double idle_time = est_print_time - print_time;
        if (!lookahead_empty || idle_time < 0.5)
        {
            return eventtime + std::max(0., std::min(1., print_time - est_print_time));
        }
        double extrude = std::abs(m_remaining_load_length);
        extrude = std::min(50., extrude / 2);
        if (extrude <= 10)
            extrude = 1;
        // logging.info("Smart loading %dmm filament with %dmm remaining" % ( extrude, abs(m_remaining_load_length)))
        std::vector<std::string> script = {"G92 E0"};
        Printer::GetInstance()->m_gcode->run_script(script);
        script = {"G1 E" + std::to_string(extrude) + " F" + std::to_string(m_auto_load_speed * 60)};
        Printer::GetInstance()->m_gcode->run_script(script);
        return Printer::GetInstance()->m_reactor->m_NOW;
    }
    return eventtime + AUTOLOAD_TIMER;
}

palette2_status_t Palette2::get_status(double eventtime)
{
    palette2_status_t status = {
        .is_splicing = m_is_splicing,
        .remaining_load_length = m_remaining_load_length,
        .ping = std::pair<int, double>()
    };
    if (m_omega_pings.size())
    {
        status.ping = m_omega_pings.back();
    }
    return status;
}
    

