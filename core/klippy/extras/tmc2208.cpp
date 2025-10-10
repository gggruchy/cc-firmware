#include "klippy.h"
#include "tmc.h"
#include "tmc2208.h"
#include "my_string.h"
#include "tmc_uart.h"
std::map<std::string, std::map<std::string, int>> Fields;
std::map<std::string, uint8_t> Registers;
std::vector<std::string> ReadRegisters = {"GCONF", "GSTAT", "IFCNT", "OTP_READ", "IOIN", "FACTORY_CONF", "TSTEP",
                                                                                        "MSCNT", "MSCURACT", "CHOPCONF", "DRV_STATUS",
                                                                                        "PWMCONF", "PWM_SCALE", "PWM_AUTO","SG_RESULT"};
std::vector<std::string> SignedFields = {"cur_a", "cur_b", "pwm_scale_auto"};  

std::map<std::string, std::function<std::string(int)>> FieldFormatters;


void init_Registers_2208()
{
    Registers["GCONF"] = 0x00;
    Registers["GSTAT"] = 0x01;
    Registers["IFCNT"] = 0x02;
    Registers["SLAVECONF"] = 0x03;

    Registers["OTP_PROG"] = 0x04;
    Registers["OTP_READ"] = 0x05;
    Registers["IOIN"] = 0x06;
    Registers["FACTORY_CONF"] = 0x07;

    Registers["IHOLD_IRUN"] = 0x10;
    Registers["TPOWERDOWN"] = 0x11;
    Registers["TSTEP"] = 0x12;
    Registers["TPWMTHRS"] = 0x13;

    Registers["VACTUAL"] = 0x22;
    Registers["MSCNT"] = 0x6a;
    Registers["MSCURACT"] = 0x6b;
    Registers["CHOPCONF"] = 0x6c;

    Registers["DRV_STATUS"] = 0x6f;
    Registers["PWMCONF"] = 0x70;
    Registers["PWM_SCALE"] = 0x71;
    Registers["PWM_AUTO"] = 0x72;
}

void init_Fields_2208()
{
    std::map<std::string, int> GCONF_map;
    GCONF_map["i_scale_analog"] = 0x01;              //0?
    GCONF_map["internal_rsense"] = 0x01 << 1;    //0?
    GCONF_map["en_spreadcycle"] = 0x01 << 2;
    GCONF_map["shaft"] = 0x01 << 3;
    GCONF_map["index_otpw"] = 0x01 << 4;            //0?
    GCONF_map["index_step"] = 0x01 << 5;            //0
    GCONF_map["pdn_disable"] = 0x01 << 6;           //0
    GCONF_map["mstep_reg_select"] = 0x01 << 7;  //1
    GCONF_map["multistep_filt"] = 0x01 << 8;        //1
    GCONF_map["test_mode"] = 0x01 << 9;         //0
    Fields["GCONF"] = GCONF_map;

    std::map<std::string, int> GSTAT_map;
    GSTAT_map["reset"] = 0x01;
    GSTAT_map["drv_err"] = 0x01 << 1;
    GSTAT_map["uv_cp"] = 0x01 << 2;
    Fields["GSTAT"] = GSTAT_map;

    std::map<std::string, int> IFCNT_map;
    IFCNT_map["ifcnt"] = 0xff;
    Fields["IFCNT"] = IFCNT_map;

    std::map<std::string, int> SLAVECONF_map;
    SLAVECONF_map["senddelay"] = 0x0f << 8;
    Fields["SLAVECONF"] = SLAVECONF_map;

    std::map<std::string, int> OTP_PROG_map;
    OTP_PROG_map["OTPBIT"] = 0x07;
    OTP_PROG_map["OTPBYTE"] = 0x03 << 4;
    OTP_PROG_map["OTPMAGIC"] = 0xff << 8;
    Fields["OTP_PROG"] = OTP_PROG_map;

    std::map<std::string, int> OTP_READ_map;
    OTP_READ_map["OTP_FCLKTRIM"] = 0x1f;
    OTP_READ_map["otp_OTTRIM"] = 0x01 << 5;
    OTP_READ_map["otp_internalRsense"] = 0x01 << 6;
    OTP_READ_map["otp_TBL"] = 0x01 << 7;
    OTP_READ_map["OTP_PWM_GRAD"] = 0x0f << 8;
    OTP_READ_map["otp_pwm_autograd"] = 0x01 << 12;
    OTP_READ_map["OTP_TPWMTHRS"] = 0x07 << 13;
    OTP_READ_map["otp_PWM_OFS"] = 0x01 << 16;
    OTP_READ_map["otp_PWM_REG"] = 0x01 << 17;
    OTP_READ_map["otp_PWM_FREQ"] = 0x01 << 18;
    OTP_READ_map["OTP_IHOLDDELAY"] = 0x03 << 19;
    OTP_READ_map["OTP_IHOLD"] = 0x03 << 21;
    OTP_READ_map["otp_en_spreadCycle"] = 0x01 << 23;
    Fields["OTP_READ"] = OTP_READ_map;

    // IOIN mapping depends on the driver type (SEL_A field)
    // TMC222x (SEL_A == 0)

    std::map<std::string, int> FACTORY_CONF_map;
    FACTORY_CONF_map["fclktrim"] = 0x1f;
    FACTORY_CONF_map["ottrim"] = 0x03 << 8;
    Fields["FACTORY_CONF"] = FACTORY_CONF_map;

    std::map<std::string, int> IHOLD_IRUN_map;
    IHOLD_IRUN_map["ihold"] = 0x1f;
    IHOLD_IRUN_map["irun"] = 0x1f << 8;
    IHOLD_IRUN_map["iholddelay"] = 0x0f << 16;
    Fields["IHOLD_IRUN"] = IHOLD_IRUN_map;

    std::map<std::string, int> TPOWERDOWN_map;
    TPOWERDOWN_map["tpowerdown"] = 0xff;
    Fields["TPOWERDOWN"] = TPOWERDOWN_map;

    std::map<std::string, int> TSTEP_map;
    TSTEP_map["tstep"] = 0xfffff;
    Fields["TSTEP"] = TSTEP_map;

    std::map<std::string, int> TPWMTHRS_map;
    TPWMTHRS_map["tpwmthrs"] = 0xfffff;
    Fields["TPWMTHRS"] = TPWMTHRS_map;

    std::map<std::string, int> VACTUAL_map;
    VACTUAL_map["vactual"] = 0xffffff;
    Fields["VACTUAL"] = VACTUAL_map;

    std::map<std::string, int> MSCNT_map;
    MSCNT_map["mscnt"] = 0x3ff;
    Fields["MSCNT"] = MSCNT_map;

    std::map<std::string, int> MSCURACT_map;
    MSCURACT_map["cur_a"] = 0x1ff;
    MSCURACT_map["cur_b"] = 0x1ff << 16;
    Fields["MSCURACT"] = MSCURACT_map;

    std::map<std::string, int> CHOPCONF_map;            //斩波器配置
    CHOPCONF_map["toff"] = 0x0f;                        //3     3
    CHOPCONF_map["hstrt"] = 0x07 << 4;          //0     5
    CHOPCONF_map["hend"] = 0x0f << 7;           //5     0
    CHOPCONF_map["tbl"] = 0x03 << 15;           //2         2
    CHOPCONF_map["vsense"] = 0x01 << 17;      //1        0      ？
    CHOPCONF_map["mres"] = 0x0f << 24;      //16        4       16细分
    CHOPCONF_map["intpol"] = 0x01 << 28;        //1         1
    CHOPCONF_map["dedge"] = 0x01 << 29;     //1             0
    CHOPCONF_map["diss2g"] = 0x01 << 30;        //1         0
    CHOPCONF_map["diss2vs"] = 0x01 << 31;    //1            0
    Fields["CHOPCONF"] = CHOPCONF_map;

    std::map<std::string, int> DRV_STATUS_map;
    DRV_STATUS_map["otpw"] = 0x01;
    DRV_STATUS_map["ot"] = 0x01 << 1;
    DRV_STATUS_map["s2ga"] = 0x01 << 2;
    DRV_STATUS_map["s2gb"] = 0x01 << 3;
    DRV_STATUS_map["s2vsa"] = 0x01 << 4;
    DRV_STATUS_map["s2vsb"] = 0x01 << 5;
    DRV_STATUS_map["ola"] = 0x01 << 6;
    DRV_STATUS_map["olb"] = 0x01 << 7;
    DRV_STATUS_map["t120"] = 0x01 << 8;
    DRV_STATUS_map["t143"] = 0x01 << 9;
    DRV_STATUS_map["t150"] = 0x01 << 10;
    DRV_STATUS_map["t157"] = 0x01 << 11;
    DRV_STATUS_map["cs_actual"] = 0x1f << 16;
    DRV_STATUS_map["stealth"] = 0x01 << 30;
    DRV_STATUS_map["stst"] = 0x01 << 31;
    Fields["DRV_STATUS"] = DRV_STATUS_map;

    std::map<std::string, int> PWMCONF_map;
    PWMCONF_map["pwm_ofs"] = 0xff;
    PWMCONF_map["pwm_grad"] = 0xff << 8;
    PWMCONF_map["pwm_freq"] = 0x03 << 16;
    PWMCONF_map["pwm_autoscale"] = 0x01 << 18;
    PWMCONF_map["pwm_autograd"] = 0x01 << 19;
    PWMCONF_map["freewheel"] = 0x03 << 20;
    PWMCONF_map["pwm_reg"] = 0xf << 24;
    PWMCONF_map["pwm_lim"] = 0xf << 28;
    Fields["PWMCONF"] = PWMCONF_map;

    std::map<std::string, int> PWM_SCALE_map;
    PWM_SCALE_map["pwm_scale_sum"] = 0xff;
    PWM_SCALE_map["pwm_scale_auto"] = 0x1ff << 16;
    Fields["PWM_SCALE"] = PWM_SCALE_map;

    std::map<std::string, int> PWM_AUTO_map;
    PWM_AUTO_map["pwm_ofs_auto"] = 0xff;
    PWM_AUTO_map["pwm_grad_auto"] = 0xff << 16;
    Fields["PWM_AUTO"] = PWM_AUTO_map;


    // FieldFormatters["i_scale_analog"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["shaft"] = [](int v){if(v){ return "1(Reverse)"; }else {return "";}};
    // FieldFormatters["reset"] = [](int v){if(v){ return "1(Reset)"; }else {return "";}};
    // FieldFormatters["drv_err"] = [](int v){if(v){ return "1(ErrorShutdown!)"; }else {return "";}};
    // FieldFormatters["uv_cp"] = [](int v){if(v){ return "1(Undervoltage!)"; }else {return "";}};
    // FieldFormatters["version"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["mres"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["otpw"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["ot"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["s2ga"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["s2gb"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["ola"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["olb"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
    // FieldFormatters["cs_actual"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};
}

void init_FieldFormatters_2208()
{

//     FieldFormatters["i_scale_analog"] = [](int v){if(v){ return "1(ExtVREF)"; }else {return "";}};

//     "I_scale_analog":   (lambda v: "1(ExtVREF)" if v else ""),
//     "shaft":            (lambda v: "1(Reverse)" if v else ""),
//     "reset":            (lambda v: "1(Reset)" if v else ""),
//     "drv_err":          (lambda v: "1(ErrorShutdown!)" if v else ""),
//     "uv_cp":            (lambda v: "1(Undervoltage!)" if v else ""),
//     "version":          (lambda v: "%#x" % v),
//     "mres":             (lambda v: "%d(%dusteps)" % (v, 0x100 >> v)),
//     "otpw":             (lambda v: "1(OvertempWarning!)" if v else ""),
//     "ot":               (lambda v: "1(OvertempError!)" if v else ""),
//     "s2ga":             (lambda v: "1(ShortToGND_A!)" if v else ""),
//     "s2gb":             (lambda v: "1(ShortToGND_B!)" if v else ""),
//     "ola":              (lambda v: "1(OpenLoad_A!)" if v else ""),
//     "olb":              (lambda v: "1(OpenLoad_B!)" if v else ""),
//     "cs_actual":        (lambda v: ("%d" % v) if v else "0(Reset?)"),

//    "sel_a":            (lambda v: "%d(%s)" % (v, ["TMC222x", "TMC220x"][v])),
//     "s2vsa":            (lambda v: "1(LowSideShort_A!)" if v else ""),
//     "s2vsb":            (lambda v: "1(LowSideShort_B!)" if v else ""),
}















