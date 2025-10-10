
#include "klippy.h"
#include "tmc.h"
#include "tmc2208.h"
#include "tmc2209.h"
#include "tmc_uart.h"
#include "my_string.h"
#include "debug.h"

static const double TMC_FREQUENCY = 12000000.;

void init_Registers_2209()
{
    init_Registers_2208();

    Registers["TCOOLTHRS"] = 0x14;
    Registers["COOLCONF"] = 0x42;
    Registers["SGTHRS"] = 0x40;
    Registers["SG_RESULT"] = 0x41;
}
void init_ReadRegisters_2209()
{
    // ReadRegisters
}
void init_FieldFormatters_2209()
{
    init_FieldFormatters_2208();
}
void init_Fields_2209()
{
    init_Fields_2208();
    std::map<std::string, int> COOLCONF_map;
    COOLCONF_map["semin"] = 0x0F << 0;
    COOLCONF_map["seup"] = 0x03 << 5;
    COOLCONF_map["semax"] = 0x0F << 8;
    COOLCONF_map["sedn"] = 0x03 << 13;
    COOLCONF_map["seimin"] = 0x01 << 15;
    Fields["COOLCONF"] = COOLCONF_map;

    std::map<std::string, int> IOIN_map;
    IOIN_map["enn"] = 0x01 << 0;
    IOIN_map["ms1"] = 0x01 << 2;
    IOIN_map["ms2"] = 0x01 << 3;
    IOIN_map["diag"] = 0x01 << 4;
    IOIN_map["pdn_uart"] = 0x01 << 6;
    IOIN_map["step"] = 0x01 << 7;
    IOIN_map["SPREAD_EN"] = 0x01 << 8;
    IOIN_map["dir"] = 0x01 << 9;
    IOIN_map["version"] = 0xff << 24;
    Fields["IOIN"] = IOIN_map;

    std::map<std::string, int> SGTHRS_map;
    SGTHRS_map["sgthrs"] = 0xFF << 0;
    Fields["SGTHRS"] = SGTHRS_map;

    std::map<std::string, int> SG_RESULT_map;
    SG_RESULT_map["SG_RESULT"] = 0x3FF << 0;
    Fields["SG_RESULT"] = SG_RESULT_map;

    std::map<std::string, int> TCOOLTHRS_map;
    TCOOLTHRS_map["TCOOLTHRS"] = 0xfffff;
    Fields["TCOOLTHRS"] = TCOOLTHRS_map;
}

TMC2209::TMC2209(std::string section_name)
{
    // Setup mcu communication
    init_Registers_2209();
    init_Fields_2209();
    init_FieldFormatters_2209();

    m_fields = new FieldHelper(Fields, SignedFields, FieldFormatters);
    m_mcu_tmc = new MCU_TMC_uart(section_name, Registers, m_fields, 3);
    // Setup fields for UART

    m_fields->set_field("senddelay", 2); //  Avoid tx errors on shared uart  先设置 通信 再设置通用寄存器
                                         // Allow virtual pins to be created

    // Register commands
    current_helper = new TMCCurrentHelper(section_name, m_mcu_tmc);
    cmdhelper = new TMCCommandHelper(section_name, m_mcu_tmc, current_helper);
    cmdhelper->setup_register_dump(ReadRegisters);
    tmc_virPin_helper = new TMCVirtualPinHelper(section_name, m_mcu_tmc, current_helper);
    // Setup basic register values
    m_fields->set_field("pdn_disable", true);      // GCONF
    m_fields->set_field("mstep_reg_select", true); // GCONF
    // m_fields->set_field("multistep_filt", true);

    mh = new TMCMicrostepHelper(section_name, m_mcu_tmc);
    get_microsteps = std::bind(&TMCMicrostepHelper::get_microsteps, mh); //  &(mh->get_microsteps);
    get_phase = std::bind(&TMCMicrostepHelper::get_phase, mh);           //&(mh->get_phase);
    TMCStealthchopHelper(section_name, m_mcu_tmc, TMC_FREQUENCY);

    // set_config_field = m_fields->set_config_fieldl;
    // Allow other registers to be set from the config

    // 狂暴模式 无步进滤波 VREF下拉电阻  取消256细分 实际电流缩放状态  电感

    // SLAVECONF 0   200            串口接收到发送延时  确保单线通信成功
    m_fields->set_field("senddelay", 2);
    // GCONF 101     1c0     通用配置
    //  m_fields->set_config_field(section_name, "i_scale_analog", 0);              //默认1 不用VREF作为电流参考
    m_fields->set_field("i_scale_analog", 0);  // 默认1 不用VREF作为电流参考
    m_fields->set_field("internal_rsense", 0); // 外部0.1欧姆电阻
    // m_fields->set_config_field(section_name, "en_spreadcycle", 1);      //打开狂暴模式 ，   不配置  stealthchop_threshold 就直接是狂飙模式
    m_fields->set_field("shaft", 0);                               // 电机方向反向
    m_fields->set_field("pdn_disable", 1);                         // 设置串口模式
    m_fields->set_field("mstep_reg_select", 1);                    // 选择寄存器配置细分模式
    m_fields->set_config_field(section_name, "multistep_filt", 0); // 步进脉冲滤波打开
    // m_fields->set_field("multistep_filt", true);      //TSET寄存器设置滤波间隔
    m_fields->set_field("test_mode", 0);

    // CHOPCONF  14010053   14010283
    m_fields->set_config_field(section_name, "toff", 3);
    m_fields->set_config_field(section_name, "hstrt", 5);
    m_fields->set_config_field(section_name, "hend", 0);
    m_fields->set_config_field(section_name, "tbl", 2);
    // m_fields->set_field("mres", 4);             //microsteps
    // IHOLDIRUN  0  81310
    m_fields->set_config_field(section_name, "iholddelay", 8); // 避免断电电机抖动 1-15
    // PWMCONF  c80d0e24 c80d0e24
    m_fields->set_config_field(section_name, "pwm_ofs", 36);
    m_fields->set_config_field(section_name, "pwm_grad", 14);
    m_fields->set_config_field(section_name, "pwm_freq", 1);
    m_fields->set_config_field(section_name, "pwm_autoscale", true);
    m_fields->set_config_field(section_name, "pwm_autograd", true);
    m_fields->set_config_field(section_name, "pwm_reg", 8);
    m_fields->set_config_field(section_name, "pwm_lim", 12);

    // SGTHRS   0  64  失速检测阈值  100    越大越是灵敏
    m_fields->set_config_field(section_name, "sgthrs", 0); // 越大 检测负载越小  越容易 停下来

    // TPOWERDOWN  0   14  设置检测到电机停下到降低电机电流延时时间 默认20 只写
    m_fields->set_config_field(section_name, "tpowerdown", 20); // 20
    // TPWMTHRS  0 0      设置静音模式的最大速度   超速进入狂暴i模式  设置为0关闭模式切换
    //  m_fields->set_config_field(section_name, "tpwmthrs", 0);        //stealthchop_threshold
}

TMC2209::~TMC2209()
{
    if (m_fields != nullptr)
        delete m_fields;
    if (m_mcu_tmc != nullptr)
        delete m_mcu_tmc;
    if (tmc_virPin_helper != nullptr)
        delete tmc_virPin_helper;
    if (current_helper != nullptr)
        delete current_helper;
    if (cmdhelper != nullptr)
        delete cmdhelper;
    if (mh != nullptr)
        delete mh;
}
