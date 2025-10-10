#ifndef PRINTER_PARA_H
#define PRINTER_PARA_H
#include <string>
#include <vector>
#include <map>
#include "gcode.h"

class Printer_para
{
private:
public:
    bool is_root;

    Printer_para();
    ~Printer_para();
    void load_default_config();
    void load_user_config();
    void set_cfg(std::string section, std::string key, std::string value);
    void set_cfg(std::string section, std::string key, double value);
    void set_cfg(std::string section, std::string key, int value);

    void cmd_ROOT(GCodeCommand &gcode);
    void cmd_UNROOT(GCodeCommand &gcode);
    // stepper
    void cmd_M8000(GCodeCommand &gcode);
    void cmd_M8001(GCodeCommand &gcode);
    void cmd_M8002(GCodeCommand &gcode);
    void cmd_M8003(GCodeCommand &gcode);
    void cmd_M8004(GCodeCommand &gcode);
    void cmd_M8005(GCodeCommand &gcode);
    void cmd_M8006(GCodeCommand &gcode);
    void cmd_M8007(GCodeCommand &gcode);
    void cmd_M8008(GCodeCommand &gcode);
    void cmd_M8009(GCodeCommand &gcode);
    void cmd_M8010(GCodeCommand &gcode);
    void cmd_M8011(GCodeCommand &gcode);
    void cmd_M8012(GCodeCommand &gcode);
    void cmd_M8013(GCodeCommand &gcode);
    void cmd_M8014(GCodeCommand &gcode);
    void cmd_M8015(GCodeCommand &gcode);
    void cmd_M92(GCodeCommand &gcmd);
    void cmd_M93(GCodeCommand &gcmd);
    void cmd_M8016(GCodeCommand &gcode);
    void cmd_M8017(GCodeCommand &gcode);
    void cmd_M8018(GCodeCommand &gcode);
    void cmd_M8019(GCodeCommand &gcode);
    void cmd_M8020(GCodeCommand &gcode);
    void cmd_M8021(GCodeCommand &gcode);
    void cmd_M8022(GCodeCommand &gcode);
    void cmd_M8023(GCodeCommand &gcode);
    void cmd_M8024(GCodeCommand &gcode);
    void cmd_M8025(GCodeCommand &gcode);
    void cmd_M8026(GCodeCommand &gcode);
    void cmd_M8027(GCodeCommand &gcode);
    void cmd_M8028(GCodeCommand &gcode);
    void cmd_M8029(GCodeCommand &gcode);
    void cmd_M8030(GCodeCommand &gcode);
    void cmd_M8031(GCodeCommand &gcode);
    void cmd_M8032(GCodeCommand &gcode);
    void cmd_M8033(GCodeCommand &gcode);
    void cmd_M8034(GCodeCommand &gcode);
    void cmd_M8035(GCodeCommand &gcode);
    void cmd_M8036(GCodeCommand &gcode);
    void cmd_M8037(GCodeCommand &gcode);
    void cmd_M8038(GCodeCommand &gcode);
    void cmd_M8039(GCodeCommand &gcode);
    void cmd_M8040(GCodeCommand &gcode);
    void cmd_M8041(GCodeCommand &gcode);
    void cmd_M8042(GCodeCommand &gcode);
    void cmd_M8043(GCodeCommand &gcode);
    void cmd_M8044(GCodeCommand &gcode);
    void cmd_M8045(GCodeCommand &gcode);
    void cmd_M8046(GCodeCommand &gcode);
    void cmd_M8047(GCodeCommand &gcode);

    // extruder
    void cmd_M8050(GCodeCommand &gcode);
    void cmd_M8051(GCodeCommand &gcode);
    void cmd_M8052(GCodeCommand &gcode);
    void cmd_M8053(GCodeCommand &gcode);
    void cmd_M8054(GCodeCommand &gcode);
    void cmd_M8055(GCodeCommand &gcode);
    // extruder heat protect
    void cmd_M8056(GCodeCommand &gcode);
    void cmd_M8057(GCodeCommand &gcode);
    void cmd_M8058(GCodeCommand &gcode);
    void cmd_M8059(GCodeCommand &gcode);

    // heater bed
    void cmd_M8060(GCodeCommand &gcode);
    void cmd_M8061(GCodeCommand &gcode);
    void cmd_M8062(GCodeCommand &gcode);
    // heater bed heat protect
    void cmd_M8066(GCodeCommand &gcode);
    void cmd_M8067(GCodeCommand &gcode);
    void cmd_M8068(GCodeCommand &gcode);
    void cmd_M8069(GCodeCommand &gcode);

    // resonance_tester
    void cmd_M8070(GCodeCommand &gcode);
    void cmd_M8071(GCodeCommand &gcode);
    void cmd_M8072(GCodeCommand &gcode);
    void cmd_M8073(GCodeCommand &gcode);
    void cmd_M8074(GCodeCommand &gcode);

    // printer
    void cmd_M8080(GCodeCommand &gcode);
    void cmd_M8081(GCodeCommand &gcode);
    void cmd_M8082(GCodeCommand &gcode);
    void cmd_M8083(GCodeCommand &gcode);
    void cmd_M8084(GCodeCommand &gcode);

    // heater_fan
    void cmd_M8090(GCodeCommand &gcode);
    void cmd_M8091(GCodeCommand &gcode);

    // controller_fan
    void cmd_M8095(GCodeCommand &gcode);
    void cmd_M8096(GCodeCommand &gcode);
    void cmd_M8097(GCodeCommand &gcode);
    void cmd_M8098(GCodeCommand &gcode);
    void cmd_M8099(GCodeCommand &gcode);

    // tmc2209
    void cmd_M8100(GCodeCommand &gcode);
    void cmd_M8101(GCodeCommand &gcode);
    void cmd_M8102(GCodeCommand &gcode);
    void cmd_M8103(GCodeCommand &gcode);
    void cmd_M8104(GCodeCommand &gcode);
    void cmd_M8105(GCodeCommand &gcode);
    void cmd_M8106(GCodeCommand &gcode);
    void cmd_M8107(GCodeCommand &gcode);
    void cmd_M8108(GCodeCommand &gcode);
    void cmd_M8109(GCodeCommand &gcode);
    void cmd_M8110(GCodeCommand &gcode);
    void cmd_M8111(GCodeCommand &gcode);
    void cmd_M8112(GCodeCommand &gcode);
    void cmd_M8113(GCodeCommand &gcode);
    void cmd_M8114(GCodeCommand &gcode);
    void cmd_M8115(GCodeCommand &gcode);
    void cmd_M8116(GCodeCommand &gcode);
    void cmd_M8117(GCodeCommand &gcode);
    void cmd_M8118(GCodeCommand &gcode);
    void cmd_M8119(GCodeCommand &gcode);
    void cmd_M8120(GCodeCommand &gcode);
    void cmd_M8121(GCodeCommand &gcode);
    void cmd_M8122(GCodeCommand &gcode);

    // bed_mesh
    void cmd_M8200(GCodeCommand &gcode);
    void cmd_M8201(GCodeCommand &gcode);
    void cmd_M8202(GCodeCommand &gcode);
    void cmd_M8203(GCodeCommand &gcode);
    void cmd_M8204(GCodeCommand &gcode);

    // auto leveling
    void cmd_M8210(GCodeCommand &gcode);
    void cmd_M8211(GCodeCommand &gcode);
    void cmd_M8212(GCodeCommand &gcode);
    void cmd_M8213(GCodeCommand &gcode);
    void cmd_M8214(GCodeCommand &gcode);
    void cmd_M8215(GCodeCommand &gcode);
    void cmd_M8216(GCodeCommand &gcode);
    void cmd_M8217(GCodeCommand &gcode);
    void cmd_M8218(GCodeCommand &gcode);
    void cmd_M8219(GCodeCommand &gcode);
    void cmd_M8220(GCodeCommand &gcode);

    // probe
    void cmd_M8230(GCodeCommand &gcode);
    void cmd_M8231(GCodeCommand &gcode);
    void cmd_M8232(GCodeCommand &gcode);
    void cmd_M8233(GCodeCommand &gcode);
    void cmd_M8234(GCodeCommand &gcode);
    void cmd_M8235(GCodeCommand &gcode);
    void cmd_M8236(GCodeCommand &gcode);
    void cmd_M8237(GCodeCommand &gcode);
    void cmd_M8238(GCodeCommand &gcode);
    void cmd_M8239(GCodeCommand &gcode);
    void cmd_M8240(GCodeCommand &gcode);

    // bed mesh probe
    void cmd_M8250(GCodeCommand &gcode);
    void cmd_M8251(GCodeCommand &gcode);
    void cmd_M8252(GCodeCommand &gcode);
    void cmd_M8253(GCodeCommand &gcode);
    void cmd_M8254(GCodeCommand &gcode);
    void cmd_M8255(GCodeCommand &gcode);
    void cmd_M8256(GCodeCommand &gcode);
    void cmd_M8257(GCodeCommand &gcode);
    void cmd_M8258(GCodeCommand &gcode);
    void cmd_M8259(GCodeCommand &gcode);

    // save
    void cmd_M8800(GCodeCommand &gcode);
    void cmd_M8801(GCodeCommand &gcode);
    void cmd_M8802(GCodeCommand &gcode);
    void cmd_M8803(GCodeCommand &gcode);
    void cmd_M503(GCodeCommand &gcmd);
    // system
    void cmd_M8804(GCodeCommand &gcode);
    void cmd_M8805(GCodeCommand &gcode);
    void cmd_M8806(GCodeCommand &gcode);
    void cmd_M8807(GCodeCommand &gcode);
    void cmd_M8808(GCodeCommand &gcode);
    void cmd_M8809(GCodeCommand &gcode);
    void cmd_M8810(GCodeCommand &gcode);
    void cmd_M8811(GCodeCommand &gcode);
    void cmd_M8815(GCodeCommand &gcode);
    void cmd_M8812(GCodeCommand &gcode);
    void cmd_M8813(GCodeCommand &gcode);
    void cmd_M8814(GCodeCommand &gcode);
    void cmd_M8816(GCodeCommand &gcode);
    void cmd_M8819(GCodeCommand &gcode);
    // void cmd_M8820(GCodeCommand &gcode);
    // void cmd_M8821(GCodeCommand &gcode);
    void cmd_M8822(GCodeCommand &gcode);
    void cmd_M8823(GCodeCommand &gcode);
    void cmd_M8824(GCodeCommand &gcode);
    void func_M8824(bool isUpdate=false);
    void cmd_M8825(GCodeCommand &gcode);
    void cmd_M8826(GCodeCommand &gcode);
};

enum
{
    UI_LOAD_ENGINEERING = 0,
};
typedef void (*ui_load_callback_t)(int state);
int ui_load_register_state_callback(ui_load_callback_t state_callback);
int ui_load_callback_call(int state);

#endif