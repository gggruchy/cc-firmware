#include "klippy.h"
#include "tmc.h"

#include "my_string.h"
#include "tmc_uart.h"
#include "debug.h"
#define LOG_TAG "tmc"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

// Return the position of the first bit set in a mask
int ffs(int mask)
{
    return bit_length(mask & -mask) - 1;
}

FieldHelper::FieldHelper(std::map<std::string, std::map<std::string, int>> &all_fields, std::vector<std::string> &signed_fields, std::map<std::string, std::function<std::string(int)>> &field_formatters,std::map<std::string, int>registers)
{
    m_all_fields = all_fields;
    for (auto sf : signed_fields)
    {
        m_signed_fields[sf] = 1;
    }
    m_field_formatters = field_formatters;
    m_registers = registers;
    for (auto rf : m_all_fields)
    {
        for (auto f : rf.second)
        {
            auto iter = m_field_to_register.find(f.first);
            if (iter != m_field_to_register.end())
            {
                GAM_DEBUG_printf("-f.first:%s redo----------\n",f.first.c_str());
            }
            else
            {
                m_field_to_register[f.first] = rf.first;
            }
        }
    }
}

FieldHelper::~FieldHelper()
{
}

std::string FieldHelper::lookup_register(std::string field_name)
{
    auto iter = m_field_to_register.find(field_name);
    if (iter != m_field_to_register.end())
    {
        return iter->second;
    }
    else
    {
        return "";
    }
}

int FieldHelper::get_field(std::string field_name, int reg_value, std::string reg_name)
{
    // Returns value of the register field
    if (reg_name == "")
    {
        auto iter = m_field_to_register.find(field_name);
        if (iter != m_field_to_register.end())
        {
            reg_name = iter->second;
        }
        else
        {
            reg_name = "";
        }
    }
    if (reg_name == "")
    {
        LOG_E("---get_field-reg_name:field_name-%s:%s---------\r\n",reg_name.c_str(),field_name.c_str());
    }
    if (reg_value == 0)
    {
        auto iter = m_registers.find(reg_name);
        if (iter != m_registers.end())
        {
            reg_value = iter->second;
        }
        else
        {
            reg_value = 0;
        }
    }
    int mask = m_all_fields[reg_name][field_name];
    int field_value = (reg_value & mask) >> ffs(mask);
    if (m_signed_fields.find(field_name) != m_signed_fields.end() && ((reg_value & mask) << 1) > mask)
    {
        field_value -= (1 << bit_length(field_value));
    }
    return field_value;
}

int FieldHelper::set_field(std::string field_name, int field_value, int reg_value, std::string reg_name)
{
    // Returns register value with field bits filled with supplied value
    if (reg_name == "")
    {
        reg_name = m_field_to_register[field_name];
    }
    if (reg_value == 0)
    {
        auto iter = m_registers.find(reg_name);
        if (iter != m_registers.end())
        {
            reg_value = iter->second;
        }
        else
        {
            reg_value = 0;
        }
    }
    int mask = m_all_fields[reg_name][field_name];
    int new_value = (reg_value & ~mask) | ((field_value << ffs(mask)) & mask);
    m_registers[reg_name] = new_value;
    if (reg_name == "")
    {
        LOG_E("-set_field---reg_name:field_name-----%s:%s----%x-%x-----\r\n",reg_name.c_str(),field_name.c_str(),new_value,field_value);
    }
    return new_value;
}

int FieldHelper::set_config_field(std::string section_name, std::string field_name, int default_value)
{
    // Allow a field to be set from the config file
    // transform(field_name.begin(), field_name.end(), field_name.begin(), (int (*)(int))tolower);
    std::string config_name = "driver_" + field_name;
    std::string reg_name = m_field_to_register[field_name];
    int mask = m_all_fields[reg_name][field_name];
    int maxval = mask >> ffs(mask);
    int val = 0;
    if (maxval == 1)
    {
        val = Printer::GetInstance()->m_pconfig->GetBool(section_name, config_name, default_value);
    }
    else if (m_signed_fields.find(field_name) != m_signed_fields.end())
    {
        val = Printer::GetInstance()->m_pconfig->GetInt(section_name, config_name, default_value, -(maxval / 2 + 1), maxval / 2);
    }
    else
    {
        val = Printer::GetInstance()->m_pconfig->GetInt(section_name, config_name, default_value, 0, maxval);
    }
    return set_field(field_name, val);
}

std::string FieldHelper::pretty_format(std::string reg_name, int reg_value) //
{
    // 输出寄存器格式日志  Provide a string description of a register
    std::map<std::string, int> reg_fields;
    reg_fields = m_all_fields[reg_name];
    auto iter = reg_fields.begin();
    // reg_fields = sorted([(mask, name) for name, mask in reg_fields.items()]);    //对重新排序
    while (iter != reg_fields.end())
    {
        iter++;
        int field_value = get_field(iter->first, reg_value, reg_name);
        // std::string sval = m_field_formatters.find(field_name)->second(field_value);
        // if sval and sval != "0":
        //     fields.append(" %s=%s" % (field_name, sval));
    }
    return reg_name;
}

// new klipper
// std::map<std::string, int> FieldHelper::get_reg_fields(std::string reg_name, int reg_value)
// {
//     // Provide fields found in a register
//     auto iter = m_all_fields.find(reg_name);
//     std::map<std::string, int> reg_fields;
//     if (iter != m_all_fields.end())
//     {
//         reg_fields = m_all_fields[reg_name];
//     }
//     std::map<std::string, int> ret;
//     for (auto rf : reg_fields)
//     {
//         ret[rf.first] = get_field(rf.first, reg_value, reg_name);
//     }
//     return ret;
// }
static const double MAX_CURRENT = 2.000;
TMCCurrentHelper::TMCCurrentHelper(std::string section_name, MCU_TMC *mcu_tmc)
{
    m_name = split(section_name, " ").back();
    m_mcu_tmc = mcu_tmc;
    m_fields = m_mcu_tmc->get_fields();
   
    double run_current = Printer::GetInstance()->m_pconfig->GetDouble(section_name,"run_current", 1,0,MAX_CURRENT,0.);
    double hold_current = Printer::GetInstance()->m_pconfig->GetDouble(section_name,"hold_current", run_current,0,MAX_CURRENT,0.);
    double home_run_current = Printer::GetInstance()->m_pconfig->GetDouble(section_name,"home_run_current", run_current,MAX_CURRENT,0.);
    double home_hold_current = Printer::GetInstance()->m_pconfig->GetDouble(section_name,"home_hold_current", home_run_current,0,MAX_CURRENT,0.);
    m_sense_resistor = Printer::GetInstance()->m_pconfig->GetDouble(section_name,"sense_resistor", 0.110, 0,10,0);
    m_fields->driver_sgthrs_home = Printer::GetInstance()->m_pconfig->GetInt(section_name, "driver_sgthrs", 0);  
    
    int irun,ihold;
    int vsense = _calc_current(home_run_current, home_hold_current,irun,ihold);

    m_fields->irun_home = home_run_current;
    m_fields->ihold_home = home_hold_current;
    // m_fields->vsense_home = vsense;

    vsense = _calc_current(run_current, hold_current,irun,ihold);
    m_fields->irun = run_current;
    m_fields->ihold = hold_current;
    // m_fields->vsense = vsense;
    m_fields->set_field("vsense", vsense);
    m_fields->set_field("ihold", ihold);
    m_fields->set_field("irun", irun);
}

TMCCurrentHelper::~TMCCurrentHelper()
{
}

int TMCCurrentHelper::_calc_current_bits(double current, int vsense)
{
    double sense_resistor = m_sense_resistor + 0.020;
    double vref = 0.32;
    if (vsense)
        vref = 0.18;
    int cs = int(32. * current * sense_resistor * sqrt(2.) / vref - 1. + 0.5);
    return std::max(0, std::min(31, cs));
}
bool TMCCurrentHelper::_calc_current(double run_current, double hold_current,int &irun ,int & ihold )
{
        int vsense = 0;
        irun = _calc_current_bits(run_current, vsense);
        ihold = _calc_current_bits(std::min(hold_current, run_current),vsense);
        if(( irun < 16) && (ihold < 16))
        {
            vsense = 1;
            irun = _calc_current_bits(run_current, vsense);
            ihold = _calc_current_bits(std::min(hold_current, run_current), vsense);
        }
        return vsense;
}
double TMCCurrentHelper::_calc_current_from_field( std::string field_name)
{
        int bits = m_fields->get_field(field_name);
        double sense_resistor = m_sense_resistor + 0.020;
        double vref = 0.32;
        if (m_fields->get_field("vsense"))
            vref = 0.18;
        return (bits + 1) * vref / (32 * sense_resistor * sqrt(2.));
}
double TMCCurrentHelper::get_current(double &run_current ,double & hold_current )
{
         run_current = _calc_current_from_field("irun");
         hold_current = _calc_current_from_field("ihold");
        return MAX_CURRENT;
}
void TMCCurrentHelper::set_current( double run_current, double hold_current, double print_time)
{
        int irun, ihold ;
        int vsense = _calc_current(run_current, hold_current,irun,ihold);
        if (vsense != m_fields->get_field("vsense"))
        {
           int val = m_fields->set_field("vsense", vsense);
            m_mcu_tmc->set_register("CHOPCONF", val, print_time);
        }
        m_fields->set_field("ihold", ihold);
        int val = m_fields->set_field("irun", irun);
        m_mcu_tmc->set_register("IHOLD_IRUN", val, print_time);
}

TMCErrorCheck::TMCErrorCheck(std::string section_name, MCU_TMC *mcu_tmc)
{
}

TMCErrorCheck::~TMCErrorCheck()
{
}

TMCCommandHelper::TMCCommandHelper(std::string section_name, MCU_TMC *mcu_tmc, TMCCurrentHelper *current_helper)
{
    std::vector<std::string> name = split(section_name, " ");       //"tmc2209 stepper_z"
    m_stepper_name = "";
    for (int i = 1; i < name.size(); i++)
    {
        m_stepper_name += name[i];          //" stepper_z"
    }
    m_name = name.back();   //"stepper_z"
    m_mcu_tmc = mcu_tmc;
    m_current_helper = current_helper;
    m_echeck_helper = new TMCErrorCheck(section_name, mcu_tmc);
    m_fields = mcu_tmc->get_fields();
    m_read_registers = m_read_translate = std::vector<std::string>();
    m_toff = -1;

    std::string cmd_INIT_TMC_help = "Initialize TMC stepper driver registers";
    std::string cmd_SET_TMC_FIELD_help = "Set a register field of a TMC driver";
    std::string cmd_SET_TMC_CURRENT_help = "Set the current of a TMC driver";
    std::string cmd_DUMP_TMC_help = "Read and display TMC stepper driver registers";
    // m_stepper_enable = Printer::GetInstance()->m_stepper_enable;
    // Printer::GetInstance()->register_event_handler("stepper:sync_mcu_position", std::bind(&TMCCommandHelper::_handle_sync_mcu_pos, this));

    Printer::GetInstance()->register_event_handler("klippy:connect" + section_name, std::bind(&TMCCommandHelper::_handle_connect, this));

    // GCodeDispatch * gcode = (GCodeDispatch *)Printer::GetInstance()->lookup_object("gcode");
    Printer::GetInstance()->m_gcode->register_mux_command("SET_TMC_FIELD", "STEPPER", m_name, std::bind(&TMCCommandHelper::cmd_SET_TMC_FIELD, this, std::placeholders::_1), cmd_SET_TMC_FIELD_help);
    Printer::GetInstance()->m_gcode->register_mux_command("INIT_TMC", "STEPPER", m_name, std::bind(&TMCCommandHelper::cmd_INIT_TMC, this, std::placeholders::_1), cmd_INIT_TMC_help);
    // Printer::GetInstance()->m_gcode->register_mux_command("SET_TMC_CURRENT", "STEPPER", m_name, std::bind(&TMCCommandHelper::cmd_SET_TMC_CURRENT, this, std::placeholders::_1), cmd_SET_TMC_CURRENT_help);
    Printer::GetInstance()->m_gcode->register_command("SET_TMC_CURRENT_" + m_name, std::bind(&TMCCommandHelper::cmd_SET_TMC_CURRENT, this, std::placeholders::_1), false, cmd_INIT_TMC_help);
}

TMCCommandHelper::~TMCCommandHelper()
{
}
void TMCCommandHelper::_init_registers(double print_time)
{
    //  Send registers

    uint32_t stats1 ;
    stats1 = m_mcu_tmc->get_register("DRV_STATUS");
    // printf(" DRV_STATUS  register_val %x\n",stats1);
    auto iter = m_fields->m_registers.find("SLAVECONF");
    if (iter != m_fields->m_registers.end())
    {
        m_mcu_tmc->set_register(iter->first, iter->second, print_time);
    //         stats1 = m_mcu_tmc->get_register("DRV_STATUS");
    // printf(" DRV_STATUS  register_val %x\n",stats1);
    }
    iter = m_fields->m_registers.find("GCONF");
    if (iter != m_fields->m_registers.end())
    {
        m_mcu_tmc->set_register(iter->first, iter->second, print_time);
    //         stats1 = m_mcu_tmc->get_register("DRV_STATUS");
    // printf(" DRV_STATUS  register_val %x\n",stats1);
    }
    iter = m_fields->m_registers.begin();
    while (iter != m_fields->m_registers.end())
    {
        m_mcu_tmc->set_register(iter->first, iter->second, print_time);
            // stats1 = m_mcu_tmc->get_register("DRV_STATUS");
    // printf(" DRV_STATUS  register_val %x\n",stats1);
        iter++;
    }
    printf(" DRV_STATUS  register_val %x\n",stats1);
    stats1 = m_mcu_tmc->get_register("DRV_STATUS");
    printf(" DRV_STATUS  register_val %x\n",stats1);
}

void TMCCommandHelper::cmd_INIT_TMC(GCodeCommand &gcmd)
{
    // logging.info("INIT_TMC %s", m_name);
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    _init_registers(print_time);
}

void TMCCommandHelper::cmd_SET_TMC_FIELD(GCodeCommand &gcmd) // SET_TMC_FIELD FIELD=ASASA VALUE=0
{
    std::string field_name = gcmd.get_string("FIELD", "");
    std::string reg_name = m_fields->lookup_register(field_name);
    if (reg_name == "")
    {
        LOG_E("Unknown field name '%s'", field_name);
    }
    int value = gcmd.get_int("VALUE", 0);
    int reg_val = m_fields->set_field(field_name, value);
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    m_mcu_tmc->set_register(reg_name, reg_val, print_time);
}

void TMCCommandHelper::cmd_SET_TMC_CURRENT(GCodeCommand &gcmd)
{
    TMCCurrentHelper *ch = m_current_helper;
    double prev_run_current = 0;
    double prev_hold_current = 0;
    double max_current = 0;
    max_current = ch->get_current(prev_run_current,prev_hold_current); // 未实现
    double run_current = gcmd.get_double("CURRENT", DBL_MIN, 0, max_current);
    double hold_current = gcmd.get_double("HOLDCURRENT", DBL_MIN, 0, max_current, 0, DBL_MAX);
    if(run_current != DBL_MIN || hold_current != DBL_MIN)
    {
        if(run_current == DBL_MIN)
        {
            run_current = prev_run_current;
        }
        if(hold_current == DBL_MIN)
        {
            hold_current = prev_hold_current;
        }
        double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
        ch->set_current(run_current, hold_current, print_time);

        max_current = ch->get_current(prev_run_current,prev_hold_current); // 未实现
    }
    if (run_current == DBL_MIN && hold_current == DBL_MIN)
    {
        double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
        ch->set_current(m_fields->irun, m_fields->ihold, print_time);
    }
    // Report values
    if (fabs(prev_hold_current) < 1e-15)
    {
        gcmd.m_respond_info(m_name + " Run Current: " + to_string(prev_run_current) + "A", false);
    }
    else
    {
        gcmd.m_respond_info(m_name + " Run Current: " + to_string(prev_run_current) + "A Hold Current: " + to_string(prev_hold_current) + "A", false);
    }
}

// # Stepper enable/disable tracking
void TMCCommandHelper::_do_enable(double print_time)
{
    // try:
        if(m_toff != -1)
        {
            // Shared enable via comms handling
            m_fields->set_field("toff", m_toff);
        }
        _init_registers();
        m_echeck_helper->start_checks();
}

void TMCCommandHelper::_do_disable(double print_time)
{
    // try:
    if(m_toff != -1)
    {
        int val = m_fields->set_field("toff", 0);
        std::string reg_name = m_fields->lookup_register("toff");
        m_mcu_tmc->set_register(reg_name, val, print_time);
    }
    m_echeck_helper->start_checks();
    // except self.printer.command_error as e:
    // self.printer.invoke_shutdown(str(e))
}

void TMCCommandHelper::handle_stepper_enable(double print_time, bool is_enable)
{
    std::function<void(double)> cb;
    if(is_enable)
    {
        cb = std::bind(&TMCCommandHelper::_do_enable, this, std::placeholders::_1);
    }
    else
    {
        cb = std::bind(&TMCCommandHelper::_do_disable, this, std::placeholders::_1);
    }
    Printer::GetInstance()->get_reactor()->register_callback(cb, print_time); //未实现
}

void TMCCommandHelper::_handle_connect()
{
    //Check for soft stepper enable / disable
    PrinterStepperEnable *stepper_enable = (PrinterStepperEnable *)Printer::GetInstance()->lookup_object("stepper_enable");
    EnableTracking *enable_line = stepper_enable->lookup_enable(m_stepper_name);
    enable_line->register_state_callback(std::bind(&TMCCommandHelper::handle_stepper_enable, this, std::placeholders::_1, std::placeholders::_2));
    if(!enable_line->has_dedicated_enable())
    {
        m_toff = m_fields->get_field("toff");
        m_fields->set_field("toff", 0);
        // logging.info("Enabling TMC virtual enable for '%s'", m_stepper_name);
    }
    // Send init
    // try:
    // DisTraceMsg();
    _init_registers();
    // except self.printer.command_error as e:
    //     logging.info("TMC %s failed to init: %s", self.name, str(e))
}

// # DUMP_TMC support
void TMCCommandHelper::setup_register_dump( std::vector<std::string> read_registers, std::vector<std::string>read_translate)
{
    m_read_registers = read_registers;
    m_read_translate = read_translate;
    Printer::GetInstance()->m_gcode->register_mux_command("DUMP_TMC", "STEPPER", m_name,std::bind(&TMCCommandHelper::cmd_DUMP_TMC, this, std::placeholders::_1), cmd_DUMP_TMC_help);
}

void TMCCommandHelper::cmd_DUMP_TMC(GCodeCommand &gcmd)
{
    // logging.info("DUMP_TMC %s", m_name);
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    // gcmd.respond_info("========== Write-only registers ==========");
    auto iter = m_fields->m_registers.begin();
    while (iter != m_fields->m_registers.end())
    {
        //  if (iter->first not in m_read_registers)
        {
            //  gcmd.respond_info(m_fields->pretty_format(iter->first, iter->second));
        }
        iter++;
    }
    for(auto reg_name : m_read_registers)
    {
        uint32_t val = m_mcu_tmc->get_register(reg_name);
        if(m_read_translate.size() != 0)
        {
            // reg_name, val = m_read_translate(reg_name, val)
        }
        // gcmd.respond_info(m_fields.pretty_format(reg_name, val));
    }
}

// TMC virtual pins Helper class for "sensorless homing"  无传感器归0
TMCVirtualPinHelper::TMCVirtualPinHelper(std::string section_name, MCU_TMC *mcu_tmc, TMCCurrentHelper *current_helper)
{
    m_mcu_tmc = mcu_tmc;
    m_section_name = section_name;
    m_current_helper = current_helper;
    m_fields = mcu_tmc->get_fields();

    if (m_fields->lookup_register("diag0_stall") != "")
    {
        if (Printer::GetInstance()->m_pconfig->GetString(section_name,"diag0_pin", "")  != "")
        {
            m_diag_pin = Printer::GetInstance()->m_pconfig->GetString(section_name,"diag0_pin", "");
            m_diag_pin_field = "diag0_stall";
        }

        else
        {
            m_diag_pin = Printer::GetInstance()->m_pconfig->GetString(section_name,"diag1_pin", "");
            m_diag_pin_field = "diag1_stall";
        }
    }
    else
    {
        m_diag_pin = Printer::GetInstance()->m_pconfig->GetString(section_name,"diag_pin", "");
        m_diag_pin_field = "";
    }
    m_mcu_endstop = nullptr;
    m_en_pwm = 0;
    m_pwmthrs = 0;
    //  Register virtual_endstop pin
    std::vector<std::string> name_parts = split(section_name, " ");   //"tmc2209 stepper_z"
    // PrinterPins *ppins = Printer::GetInstance()->m_ppins;
    // std::cout << "name_parts size  " << name_parts.size() << " section_name:" << section_name << " name_parts0 " << name_parts[0] << " name_parts-1 " << name_parts.back() << std::endl;
    // GAM_DEBUG_printf("%d_section_name:%s name_parts:%s_%s\r\n",name_parts.size(),section_name.c_str(),name_parts[0].c_str(), name_parts.back().c_str() );
    Printer::GetInstance()->m_ppins->register_chip(name_parts[0] + "_" + name_parts.back()  , this);   //   //"tmc2209_stepper_z"
}
void * TMCVirtualPinHelper::setup_pin(std::string pin_type, pinParams* pin_params)
{
        // # Validate pin
    if ((pin_type != "endstop" ) || (pin_params->pin != "virtual_endstop") )
    {
        GAM_DEBUG_config("tmc virtual endstop only useful as endstop");
    }
    if ((pin_params->invert ) || (pin_params->pullup ) )
    {
        GAM_DEBUG_config("Can not pullup/invert tmc virtual pin");
    }
    if(m_diag_pin == "" )
    {
        GAM_DEBUG_config("tmc virtual endstop requires diag pin config");
    }
    //  Setup for sensorless homing
    std::string register_name = m_fields->lookup_register("en_pwm_mode");
    if (register_name == "")
    {
        m_en_pwm =  !m_fields->get_field("en_spreadcycle");
        m_pwmthrs = m_fields->get_field("tpwmthrs");
    }
    else
    {
        m_en_pwm = m_fields->get_field("en_pwm_mode");
        m_pwmthrs = 0;
    }
    Printer::GetInstance()->register_event_homing_move_handler("homing:homing_move_begin" + m_section_name, std::bind(&TMCVirtualPinHelper::handle_homing_move_begin, this, std::placeholders::_1));
    Printer::GetInstance()->register_event_homing_move_handler("homing:homing_move_end" + m_section_name, std::bind(&TMCVirtualPinHelper::handle_homing_move_end, this, std::placeholders::_1));
    Printer::GetInstance()->register_event_bool_handler("set_silent_mode" + m_section_name, std::bind(&TMCVirtualPinHelper::set_silent_mode, this, std::placeholders::_1));
    m_mcu_endstop = (MCU_endstop *)Printer::GetInstance()->m_ppins->setup_pin("endstop", m_diag_pin);
    return m_mcu_endstop;
}
void TMCVirtualPinHelper::handle_homing_move_begin( HomingMove*hmove)
{
    std::vector<MCU_endstop*> m_endstops = hmove->get_mcu_endstops();
	std::vector<MCU_endstop*>::iterator iter = std::find(m_endstops.begin(), m_endstops.end(), m_mcu_endstop);
	if (iter == m_endstops.end())
	{
		return;
	}
    int val;
    std::string register_name = m_fields->lookup_register("en_pwm_mode");
    if (register_name == "")
    {
        //  On "stallguard4" drivers, "stealthchop" must be enabled
        m_en_pwm = !m_fields->get_field("en_spreadcycle");
        int tp_val = m_fields->set_field("tpwmthrs", 0);                
        m_mcu_tmc->set_register("TPWMTHRS", tp_val);
        val = m_fields->set_field("en_spreadcycle", 0);         //静音模式
    }
    else
    {
        //  On earlier drivers, "stealthchop" must be disabled
        m_fields->set_field("en_pwm_mode", 0);
        val = m_fields->set_field(m_diag_pin_field, 1);
    }
    m_mcu_tmc->set_register("GCONF", val);
    int threshold = int(m_fields->threshold_velocity / 1.0 + .5);           //最小1mm/s的速度
    int tc_val = m_fields->set_field("TCOOLTHRS", max(0, min(0xfffff, threshold)) );     //0xfffff 打开失速输出  速度大于1mm/s才有效  TCOOLTHRS ≥ TSTEP > TPWMTHRS
    m_mcu_tmc->set_register("TCOOLTHRS", tc_val);
    // std::cout << "homing begin " << m_fields->irun_home << " " << m_fields->ihold_home << std::endl;
    // m_current_helper->set_current(m_fields->irun_home,m_fields->ihold_home ,DBL_MAX  );
    
    tc_val = m_fields->set_field("sgthrs", m_fields->driver_sgthrs_home);           //负载大小 
    m_mcu_tmc->set_register("SGTHRS", tc_val);
}
void TMCVirtualPinHelper::handle_homing_move_end(HomingMove* hmove)
{
     std::vector<MCU_endstop*> m_endstops = hmove->get_mcu_endstops();
	std::vector<MCU_endstop*>::iterator iter = std::find(m_endstops.begin(), m_endstops.end(), m_mcu_endstop);
	if (iter == m_endstops.end())
	{
		return;
	}
    int val;
    std::string register_name = m_fields->lookup_register("en_pwm_mode");
    if (register_name == "")
    {
        int tp_val = m_fields->set_field("tpwmthrs", m_pwmthrs);
        m_mcu_tmc->set_register("TPWMTHRS", tp_val);
        val = m_fields->set_field("en_spreadcycle", !m_en_pwm);         //恢复配置参数 静音模式 狂暴模式
    }
    else
    {
        m_fields->set_field("en_pwm_mode", m_en_pwm);
        val = m_fields->set_field(m_diag_pin_field, 0);
    }
    m_mcu_tmc->set_register("GCONF", val);
    int tc_val = m_fields->set_field("TCOOLTHRS", 0);    //关闭失速输出
    m_mcu_tmc->set_register("TCOOLTHRS", tc_val);
    // std::cout << "homing end " << m_fields->irun << " " << m_fields->ihold << std::endl;
    // m_current_helper->set_current(m_fields->irun,m_fields->ihold ,DBL_MAX  );
    tc_val = m_fields->set_field("sgthrs", 0);
    m_mcu_tmc->set_register("SGTHRS", tc_val);          //关闭负载大小检测  负载最大        静音模式才有用

}

void TMCVirtualPinHelper::set_silent_mode(bool silent_mode)
{
    if (silent_mode)
    {
        FieldHelper *fields = m_mcu_tmc->get_fields();
        int threshold = int(fields->threshold_velocity / 99999 + .5); 
        int tp_val = m_fields->set_field("tpwmthrs", threshold);
        m_mcu_tmc->set_register("TPWMTHRS", tp_val);
        int val = m_fields->set_field("en_spreadcycle", 0); // 静音模式
        m_mcu_tmc->set_register("GCONF", val);
        // Printer::GetInstance()->m_pconfig->SetInt(m_section_name, "stealthchop_threshold", 99999);
        // Printer::GetInstance()->m_pconfig->WriteIni(CONFIG_PATH);
    }
    else
    {
        FieldHelper *fields = m_mcu_tmc->get_fields();
        int threshold = int(fields->threshold_velocity / 0 + .5); 
        int tp_val = m_fields->set_field("tpwmthrs", threshold);
        m_mcu_tmc->set_register("TPWMTHRS", tp_val);
        int val = m_fields->set_field("en_spreadcycle", 1);         //恢复配置参数 静音模式 狂暴模式
        m_mcu_tmc->set_register("GCONF", val);
        // Printer::GetInstance()->m_pconfig->SetInt(m_section_name, "stealthchop_threshold", 0);
        // Printer::GetInstance()->m_pconfig->WriteIni(CONFIG_PATH);
    }
}

// # Helper to configure and query the microstep settings
TMCMicrostepHelper::TMCMicrostepHelper(std::string section_name, MCU_TMC *mcu_tmc)
{
    m_mcu_tmc = mcu_tmc;
    m_fields = mcu_tmc->get_fields();
    std::vector<std::string> name = split(section_name, " ");       //"tmc2209 stepper_z"
    std::string stepper_name = "";
    for (int i = 1; i < name.size(); i++)
    {
        stepper_name += name[i];          //" stepper_z"
    }
    if(!Printer::GetInstance()->m_pconfig->IsExistSection(stepper_name))
    {
        LOG_E( "Could not find config section '[%s]' required by tmc driver", stepper_name.c_str() );
    }
    // std::string stepper_name
    if( (Printer::GetInstance()->m_pconfig->GetInt(stepper_name, "microsteps", -1) == -1 ) 
        &&(Printer::GetInstance()->m_pconfig->GetInt(section_name, "microsteps", -1) != -1 ) )
    {   //Older config format with microsteps in tmc config section
        stepper_name = section_name;
    }
    // std::vetctor<int> steps = {256: 0, 128: 1, 64: 2, 32: 3, 16: 4, 8: 5, 4: 6, 2: 7, 1: 8}
    int mres = Printer::GetInstance()->m_pconfig->GetInt(stepper_name, "microsteps", 16);   //  .getchoice('microsteps', steps)
    mres = 9-bit_length(mres);
    if(mres < 0) mres= 0;
    if(mres > 8) mres= 8;
    m_fields->set_field("mres", mres);
    bool intpol = Printer::GetInstance()->m_pconfig->GetBool(stepper_name, "interpolate",false); 
    if(!intpol)
    {
        intpol = Printer::GetInstance()->m_pconfig->GetBool(stepper_name, "interpolate",true); 
        if(intpol) //stepper_name 节 没有设置
        {
            intpol = Printer::GetInstance()->m_pconfig->GetBool(section_name, "interpolate", true); 
        }
    }
    m_fields->set_field("intpol", intpol );
}
 
int TMCMicrostepHelper:: get_microsteps( )
{
    return 256 >> m_fields->get_field("mres");
}
        
int TMCMicrostepHelper:: get_phase()
{
        std::string field_name  = "mscnt";
        if (m_fields->lookup_register(field_name) == "")
        {
            // # TMC2660 uses MSTEP
            field_name = "MSTEP";
        }
        int reg = m_mcu_tmc->get_register(m_fields->lookup_register(field_name));
        int mscnt = m_fields->get_field(field_name, reg);
        return 1023 - mscnt;//1023 - mscnt, 1024;
}



void TMCStealthchopHelper(std::string section_name, MCU_TMC *mcu_tmc,double tmc_freq)
{
    FieldHelper *fields = mcu_tmc->get_fields();
    int en_pwm_mode = 0;
    double velocity = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "stealthchop_threshold", DBL_MAX, 0);


    std::vector<std::string> name = split(section_name, " ");       //"tmc2209 stepper_z"
    std::string stepper_name = "";
    for (int i = 1; i < name.size(); i++)
    {
        stepper_name += name[i];          //" stepper_z"
    }
    double step_dist = parse_step_distance(stepper_name,false,false);
    double step_dist_256 = step_dist / (1 << fields->get_field("mres"));
    double threshold_velocity = tmc_freq * step_dist_256 ;     
    fields->threshold_velocity = threshold_velocity;
    if (velocity != DBL_MAX)
    {
        int threshold = int(threshold_velocity / velocity + .5);              //12000000
        fields->set_field("tpwmthrs", max(0, min(0xfffff, threshold)));             //速度越大 tpwmthrs 越小
        en_pwm_mode = 1;
    }
    std::string register_name = fields->lookup_register("en_pwm_mode");
    if (register_name != "")
    {
        fields->set_field("en_pwm_mode", en_pwm_mode);
    }
    else
    {
    //   # TMC2208 uses en_spreadCycle
        fields->set_field("en_spreadcycle", !en_pwm_mode);
    }
  
}
