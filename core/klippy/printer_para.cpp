#include "printer_para.h"
#include "klippy.h"
#include "my_string.h"
#include "Define_config_path.h"
#include "Define_reference.h"
#include "hl_disk.h"
#include "utils.h"
#include "log.h"
#include "hl_common.h"
#include "hl_ringbuffer.h"
#include "hl_wlan.h"
#include "hl_boot.h"
extern void ui_wifi_try_connect(const char *ssid, const char *psk, hl_wlan_key_mgmt_t key_mgmt);
extern ConfigParser *get_sysconf();
Printer *gp_printer = nullptr;
ConfigParser *gp_pconfig = nullptr;
ConfigParser *user_pconfig = nullptr;
Printer_para::Printer_para()
{
    is_root = false;
    Printer::GetInstance()->m_gcode->register_command("ROOT", std::bind(&Printer_para::cmd_ROOT, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("UNROOT", std::bind(&Printer_para::cmd_UNROOT, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8000", std::bind(&Printer_para::cmd_M8000, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8001", std::bind(&Printer_para::cmd_M8001, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8002", std::bind(&Printer_para::cmd_M8002, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8003", std::bind(&Printer_para::cmd_M8003, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8004", std::bind(&Printer_para::cmd_M8004, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8005", std::bind(&Printer_para::cmd_M8005, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8006", std::bind(&Printer_para::cmd_M8006, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8007", std::bind(&Printer_para::cmd_M8007, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8008", std::bind(&Printer_para::cmd_M8008, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8009", std::bind(&Printer_para::cmd_M8009, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8010", std::bind(&Printer_para::cmd_M8010, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8011", std::bind(&Printer_para::cmd_M8011, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8012", std::bind(&Printer_para::cmd_M8012, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8013", std::bind(&Printer_para::cmd_M8013, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8014", std::bind(&Printer_para::cmd_M8014, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8015", std::bind(&Printer_para::cmd_M8015, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8016", std::bind(&Printer_para::cmd_M8016, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8017", std::bind(&Printer_para::cmd_M8017, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8018", std::bind(&Printer_para::cmd_M8018, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8019", std::bind(&Printer_para::cmd_M8019, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8020", std::bind(&Printer_para::cmd_M8020, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8021", std::bind(&Printer_para::cmd_M8021, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8022", std::bind(&Printer_para::cmd_M8022, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8023", std::bind(&Printer_para::cmd_M8023, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8024", std::bind(&Printer_para::cmd_M8024, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8025", std::bind(&Printer_para::cmd_M8025, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8026", std::bind(&Printer_para::cmd_M8026, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8027", std::bind(&Printer_para::cmd_M8027, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8028", std::bind(&Printer_para::cmd_M8028, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8029", std::bind(&Printer_para::cmd_M8029, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8030", std::bind(&Printer_para::cmd_M8030, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8031", std::bind(&Printer_para::cmd_M8031, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8032", std::bind(&Printer_para::cmd_M8032, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8033", std::bind(&Printer_para::cmd_M8033, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8034", std::bind(&Printer_para::cmd_M8034, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8035", std::bind(&Printer_para::cmd_M8035, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8036", std::bind(&Printer_para::cmd_M8036, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8037", std::bind(&Printer_para::cmd_M8037, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8038", std::bind(&Printer_para::cmd_M8038, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8039", std::bind(&Printer_para::cmd_M8039, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8040", std::bind(&Printer_para::cmd_M8040, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8041", std::bind(&Printer_para::cmd_M8041, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8042", std::bind(&Printer_para::cmd_M8042, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8043", std::bind(&Printer_para::cmd_M8043, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8044", std::bind(&Printer_para::cmd_M8044, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8045", std::bind(&Printer_para::cmd_M8045, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8046", std::bind(&Printer_para::cmd_M8046, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8047", std::bind(&Printer_para::cmd_M8047, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8050", std::bind(&Printer_para::cmd_M8050, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8051", std::bind(&Printer_para::cmd_M8051, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8052", std::bind(&Printer_para::cmd_M8052, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8053", std::bind(&Printer_para::cmd_M8053, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8054", std::bind(&Printer_para::cmd_M8054, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8055", std::bind(&Printer_para::cmd_M8055, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8056", std::bind(&Printer_para::cmd_M8056, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8057", std::bind(&Printer_para::cmd_M8057, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8058", std::bind(&Printer_para::cmd_M8058, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8059", std::bind(&Printer_para::cmd_M8059, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8060", std::bind(&Printer_para::cmd_M8060, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8061", std::bind(&Printer_para::cmd_M8061, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8062", std::bind(&Printer_para::cmd_M8062, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8066", std::bind(&Printer_para::cmd_M8066, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8067", std::bind(&Printer_para::cmd_M8067, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8068", std::bind(&Printer_para::cmd_M8068, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8069", std::bind(&Printer_para::cmd_M8069, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8070", std::bind(&Printer_para::cmd_M8070, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8071", std::bind(&Printer_para::cmd_M8071, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8072", std::bind(&Printer_para::cmd_M8072, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8073", std::bind(&Printer_para::cmd_M8073, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8074", std::bind(&Printer_para::cmd_M8074, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8080", std::bind(&Printer_para::cmd_M8080, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8081", std::bind(&Printer_para::cmd_M8081, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8082", std::bind(&Printer_para::cmd_M8082, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8083", std::bind(&Printer_para::cmd_M8083, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8084", std::bind(&Printer_para::cmd_M8084, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8090", std::bind(&Printer_para::cmd_M8090, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8091", std::bind(&Printer_para::cmd_M8091, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8095", std::bind(&Printer_para::cmd_M8095, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8096", std::bind(&Printer_para::cmd_M8096, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8097", std::bind(&Printer_para::cmd_M8097, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8098", std::bind(&Printer_para::cmd_M8098, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8099", std::bind(&Printer_para::cmd_M8099, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8100", std::bind(&Printer_para::cmd_M8100, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8101", std::bind(&Printer_para::cmd_M8101, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8102", std::bind(&Printer_para::cmd_M8102, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8103", std::bind(&Printer_para::cmd_M8103, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8104", std::bind(&Printer_para::cmd_M8104, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8105", std::bind(&Printer_para::cmd_M8105, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8106", std::bind(&Printer_para::cmd_M8106, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8107", std::bind(&Printer_para::cmd_M8107, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8108", std::bind(&Printer_para::cmd_M8108, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8109", std::bind(&Printer_para::cmd_M8109, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8110", std::bind(&Printer_para::cmd_M8110, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8111", std::bind(&Printer_para::cmd_M8111, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8112", std::bind(&Printer_para::cmd_M8112, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8113", std::bind(&Printer_para::cmd_M8113, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8114", std::bind(&Printer_para::cmd_M8114, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8115", std::bind(&Printer_para::cmd_M8115, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8116", std::bind(&Printer_para::cmd_M8116, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8117", std::bind(&Printer_para::cmd_M8117, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8118", std::bind(&Printer_para::cmd_M8118, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8119", std::bind(&Printer_para::cmd_M8119, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8120", std::bind(&Printer_para::cmd_M8120, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8121", std::bind(&Printer_para::cmd_M8121, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8122", std::bind(&Printer_para::cmd_M8122, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8200", std::bind(&Printer_para::cmd_M8200, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8201", std::bind(&Printer_para::cmd_M8201, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8202", std::bind(&Printer_para::cmd_M8202, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8203", std::bind(&Printer_para::cmd_M8203, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8204", std::bind(&Printer_para::cmd_M8204, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8210", std::bind(&Printer_para::cmd_M8210, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8211", std::bind(&Printer_para::cmd_M8211, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8212", std::bind(&Printer_para::cmd_M8212, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8213", std::bind(&Printer_para::cmd_M8213, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8214", std::bind(&Printer_para::cmd_M8214, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8215", std::bind(&Printer_para::cmd_M8215, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8216", std::bind(&Printer_para::cmd_M8216, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8217", std::bind(&Printer_para::cmd_M8217, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8218", std::bind(&Printer_para::cmd_M8218, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8219", std::bind(&Printer_para::cmd_M8219, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8220", std::bind(&Printer_para::cmd_M8220, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8230", std::bind(&Printer_para::cmd_M8230, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8231", std::bind(&Printer_para::cmd_M8231, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8232", std::bind(&Printer_para::cmd_M8232, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8233", std::bind(&Printer_para::cmd_M8233, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8234", std::bind(&Printer_para::cmd_M8234, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8235", std::bind(&Printer_para::cmd_M8235, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8236", std::bind(&Printer_para::cmd_M8236, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8237", std::bind(&Printer_para::cmd_M8237, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8238", std::bind(&Printer_para::cmd_M8238, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8239", std::bind(&Printer_para::cmd_M8239, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8240", std::bind(&Printer_para::cmd_M8240, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("M8250", std::bind(&Printer_para::cmd_M8250, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8251", std::bind(&Printer_para::cmd_M8251, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8252", std::bind(&Printer_para::cmd_M8252, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8253", std::bind(&Printer_para::cmd_M8253, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8254", std::bind(&Printer_para::cmd_M8254, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8255", std::bind(&Printer_para::cmd_M8255, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8256", std::bind(&Printer_para::cmd_M8256, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8257", std::bind(&Printer_para::cmd_M8257, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8258", std::bind(&Printer_para::cmd_M8258, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8259", std::bind(&Printer_para::cmd_M8259, this, std::placeholders::_1));

    // Printer::GetInstance()->m_gcode->register_command("M500", std::bind(&Printer_para::cmd_M8800, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8800", std::bind(&Printer_para::cmd_M8800, this, std::placeholders::_1));
    // Printer::GetInstance()->m_gcode->register_command("M503", std::bind(&Printer_para::cmd_M503, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8801", std::bind(&Printer_para::cmd_M8801, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8802", std::bind(&Printer_para::cmd_M8802, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8803", std::bind(&Printer_para::cmd_M8803, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8804", std::bind(&Printer_para::cmd_M8804, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8805", std::bind(&Printer_para::cmd_M8805, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8806", std::bind(&Printer_para::cmd_M8806, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8807", std::bind(&Printer_para::cmd_M8807, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8808", std::bind(&Printer_para::cmd_M8808, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8809", std::bind(&Printer_para::cmd_M8809, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8810", std::bind(&Printer_para::cmd_M8810, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8811", std::bind(&Printer_para::cmd_M8811, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8815", std::bind(&Printer_para::cmd_M8815, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8812", std::bind(&Printer_para::cmd_M8812, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8813", std::bind(&Printer_para::cmd_M8813, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8814", std::bind(&Printer_para::cmd_M8814, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8816", std::bind(&Printer_para::cmd_M8816, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8819", std::bind(&Printer_para::cmd_M8819, this, std::placeholders::_1));
    // Printer::GetInstance()->m_gcode->register_command("M8820", std::bind(&Printer_para::cmd_M8820, this, std::placeholders::_1));
    // Printer::GetInstance()->m_gcode->register_command("M8821", std::bind(&Printer_para::cmd_M8821, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8822", std::bind(&Printer_para::cmd_M8822, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8823", std::bind(&Printer_para::cmd_M8823, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8824", std::bind(&Printer_para::cmd_M8824, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8825", std::bind(&Printer_para::cmd_M8825, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M8826", std::bind(&Printer_para::cmd_M8826, this, std::placeholders::_1));
    gp_pconfig = Printer::GetInstance()->m_pconfig = new ConfigParser(CONFIG_PATH);
    if (access(USER_CONFIG_PATH, F_OK) != -1)
        user_pconfig = Printer::GetInstance()->user_pconfig = new ConfigParser(USER_CONFIG_PATH);
    Printer::GetInstance()->m_unmodifiable_cfg = new ConfigParser(UNMODIFIABLE_CFG_PATH);
    gp_printer = Printer::GetInstance();

}

Printer_para::~Printer_para()
{
}

void Printer_para::load_default_config()
{
    set_cfg("stepper_x", "step_pin", DEFAULT_STEPPER_X_STEP_PIN);
    set_cfg("stepper_x", "dir_pin", DEFAULT_STEPPER_X_DIR_PIN);
    set_cfg("stepper_x", "enable_pin", DEFAULT_STEPPER_X_ENABLE_PIN);
    set_cfg("stepper_x", "microsteps", DEFAULT_STEPPER_X_MICROSTEPS);
    set_cfg("stepper_x", "full_steps_per_rotation", DEFAULT_STEPPER_X_FULL_STEPS_PER_ROTATION);
    set_cfg("stepper_x", "step_distance", DEFAULT_STEPPER_X_STEP_DISTANCE);
    set_cfg("stepper_x", "endstop_pin", DEFAULT_STEPPER_X_ENDSTOP_PIN);
    set_cfg("stepper_x", "position_endstop", DEFAULT_STEPPER_X_POSITION_ENDSTOP);
    set_cfg("stepper_x", "position_max", DEFAULT_STEPPER_X_POSITION_MAX);
    set_cfg("stepper_x", "homing_speed", DEFAULT_STEPPER_X_HOMING_SPEED);
    set_cfg("stepper_x", "virtrual_endstop", DEFAULT_STEPPER_X_VIRTRUAL_ENDSTOP);

    set_cfg("stepper_y", "step_pin", DEFAULT_STEPPER_Y_STEP_PIN);
    set_cfg("stepper_y", "dir_pin", DEFAULT_STEPPER_Y_DIR_PIN);
    set_cfg("stepper_y", "enable_pin", DEFAULT_STEPPER_Y_ENABLE_PIN);
    set_cfg("stepper_y", "microsteps", DEFAULT_STEPPER_Y_MICROSTEPS);
    set_cfg("stepper_y", "full_steps_per_rotation", DEFAULT_STEPPER_Y_FULL_STEPS_PER_ROTATION);
    set_cfg("stepper_y", "step_distance", DEFAULT_STEPPER_Y_STEP_DISTANCE);
    set_cfg("stepper_y", "endstop_pin", DEFAULT_STEPPER_Y_ENDSTOP_PIN);
    set_cfg("stepper_y", "position_endstop", DEFAULT_STEPPER_Y_POSITION_ENDSTOP);
    set_cfg("stepper_y", "position_max", DEFAULT_STEPPER_Y_POSITION_MAX);
    set_cfg("stepper_y", "homing_speed", DEFAULT_STEPPER_Y_HOMING_SPEED);
    set_cfg("stepper_y", "virtrual_endstop", DEFAULT_STEPPER_Y_VIRTRUAL_ENDSTOP);

    set_cfg("stepper_z", "step_pin", DEFAULT_STEPPER_Z_STEP_PIN);
    set_cfg("stepper_z", "dir_pin", DEFAULT_STEPPER_Z_DIR_PIN);
    set_cfg("stepper_z", "enable_pin", DEFAULT_STEPPER_Z_ENABLE_PIN);
    set_cfg("stepper_z", "microsteps", DEFAULT_STEPPER_Z_MICROSTEPS);
    set_cfg("stepper_z", "full_steps_per_rotation", DEFAULT_STEPPER_Z_FULL_STEPS_PER_ROTATION);
    set_cfg("stepper_z", "step_distance", DEFAULT_STEPPER_Z_STEP_DISTANCE);
    set_cfg("stepper_z", "endstop_pin", DEFAULT_STEPPER_Z_ENDSTOP_PIN);
    set_cfg("stepper_z", "position_endstop", DEFAULT_STEPPER_Z_POSITION_ENDSTOP);
    set_cfg("stepper_z", "position_max", DEFAULT_STEPPER_Z_POSITION_MAX);
    set_cfg("stepper_z", "homing_speed", DEFAULT_STEPPER_Z_HOMING_SPEED);
    set_cfg("stepper_z", "virtrual_endstop", DEFAULT_STEPPER_Z_VIRTRUAL_ENDSTOP);

    set_cfg("extruer", "step_pin", DEFAULT_EXTRUDER_STEP_PIN);
    set_cfg("extruder", "dir_pin", DEFAULT_EXTRUDER_DIR_PIN);
    set_cfg("extruder", "enable_pin", DEFAULT_EXTRUDER_ENABLE_PIN);
    set_cfg("extruder", "microsteps", DEFAULT_EXTRUDER_MICROSTEPS);
    set_cfg("extruder", "full_steps_per_rotation", DEFAULT_EXTRUDER_FULL_STEPS_PER_ROTATION);
    set_cfg("extruder", "step_distance", DEFAULT_EXTRUDER_STEP_DISTANCE);
    set_cfg("extruder", "nozzle_diameter", DEFAULT_NOZZLE_DIAMTER);
    set_cfg("extruder", "filament_diameter", DEFAULT_FILAMENT_DIAMETER);
    set_cfg("extruder", "heater_pin", DEFAULT_EXTRUDER_HEATER_PIN);
    set_cfg("extruder", "sensor_pin", DEFAULT_EXTRUDER_SENSOR_PIN);
    set_cfg("extruder", "sensor_type", DEFAULT_EXTRUDER_SENSOR_TYPE);
    set_cfg("extruder", "control", DEFAULT_EXTRUDER_CONTROL);
    set_cfg("extruder", "pid_Kp", DEFAULT_EXTRUDER_PID_KP);
    set_cfg("extruder", "pid_Ki", DEFAULT_EXTRUDER_PID_KI);
    set_cfg("extruder", "pid_Kd", DEFAULT_EXTRUDER_PID_KD);
    set_cfg("extruder", "min_temp", DEFAULT_EXTRUDER_MIN_TEMP);
    set_cfg("extruder", "max_temp", DEFAULT_EXTRUDER_MAX_TEMP);
    set_cfg("extruder", "pressure_advance", DEFAULT_EXTRUDER_PRESSURE_ADVANCE);

    set_cfg("heater_bed", "heater_pin", DEFAULT_HEATER_BED_HEATER_PIN);
    set_cfg("heater_bed", "sensor_pin", DEFAULT_HEATER_BED_SENSOR_PIN);
    set_cfg("heater_bed", "sensor_type", DEFAULT_HEATER_BED_SENSOR_TYPE);
    set_cfg("heater_bed", "control", DEFAULT_HEATER_BED_CONTROL);
    set_cfg("heater_bed", "pid_Kp", DEFAULT_HEATER_BED_PID_KP);
    set_cfg("heater_bed", "pid_Ki", DEFAULT_HEATER_BED_PID_KI);
    set_cfg("heater_bed", "pid_Kd", DEFAULT_HEATER_BED_PID_KD);
    set_cfg("heater_bed", "min_temp", DEFAULT_HEATER_BED_MIN_TEMP);
    set_cfg("heater_bed", "max_temp", DEFAULT_HEATER_BED_MAX_TEMP);

    set_cfg("fan", "pin", DEFAULT_FAN_PIN);

    set_cfg("heater_fan", "pin", DEFAULT_HEATER_FAN_PIN);

    set_cfg("fan_generic", "pin", DEFAULT_FAN_GENERIC_PIN_1);

    set_cfg("fan_generic", "pin", DEFAULT_FAN_GENERIC_PIN_2);

    set_cfg("input_shaper", "shaper_type_x", DEFAULT_SHAPER_TYPE_X);
    set_cfg("input_shaper", "shaper_freq_x", DEFAULT_SHAPER_TYPE_X);
    set_cfg("input_shaper", "shaper_type_y", DEFAULT_SHAPER_TYPE_Y);
    set_cfg("input_shaper", "shaper_freq_y", DEFAULT_SHAPER_TYPE_Y);

    set_cfg("resonance_tester", "probe_points", DEFAULT_RESONANCE_TESTER_PROBE_POINTS);
    set_cfg("resonance_tester", "accel_chip", DEFAULT_RESONANCE_TESTER_ACCEL_CHIP);
    set_cfg("resonance_tester", "min_freq", DEFAULT_RESONANCE_TESTER_MIN_FREQ);
    set_cfg("resonance_tester", "max_freq", DEFAULT_RESONANCE_TESTER_MAX_FREQ);
    set_cfg("resonance_tester", "accel_per_hz", DEFAULT_RESONANCE_TESTER_ACCEL_PER_HZ);
    set_cfg("resonance_tester", "hz_per_sec", DEFAULT_RESONANCE_TESTER_HZ_PER_SEC);
    set_cfg("resonance_tester", "max_smoothing", DEFAULT_RESONANCE_TESTER_MAX_SMOOTHING);

    set_cfg("printer", "kinematics", DEFAULT_PRINTER_KINEMATICS);
    set_cfg("printer", "max_velocity", DEFAULT_PRINTER_MAX_VELOCITY);
    set_cfg("printer", "max_accel", DEFAULT_PRINTER_MAX_ACCEL);
    set_cfg("printer", "max_z_velocity", DEFAULT_PRINTER_MAX_Z_VELOCITY);
    set_cfg("printer", "max_z_accel", DEFAULT_PRINTER_MAX_Z_ACCEL);
    set_cfg("printer", "minimum_z_position", DEFAULT_PRINTER_MINIMUM_Z_POSITION);
    set_cfg("printer", "square_corner_velocity", DEFAULT_PRINTER_SQUARE_CORNER_VELOCITY);
}

void Printer_para::set_cfg(std::string section, std::string key, std::string value)
{
    Printer::GetInstance()->m_pconfig->SetValue(section, key, value);
}

void Printer_para::set_cfg(std::string section, std::string key, double value)
{
    Printer::GetInstance()->m_pconfig->SetDouble(section, key, value);
}

void Printer_para::set_cfg(std::string section, std::string key, int value)
{
    Printer::GetInstance()->m_pconfig->SetInt(section, key, value);
}

void Printer_para::load_user_config()
{
    Printer::GetInstance()->m_pconfig->ReadIni();
}

void Printer_para::cmd_ROOT(GCodeCommand &gcode)
{
    is_root = true;
}

void Printer_para::cmd_UNROOT(GCodeCommand &gcode)
{
    is_root = false;
}

// 步进电机方向控制
void Printer_para::cmd_M8000(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    DisErrorMsg();
    std::string ori_data = DEFAULT_STEPPER_X_STEP_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_x", "step_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_x", "step_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8001(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    DisErrorMsg();
    std::string ori_data = DEFAULT_STEPPER_Y_STEP_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_y", "step_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_y", "step_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8002(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    DisErrorMsg();
    std::string ori_data = DEFAULT_STEPPER_Z_STEP_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_z", "step_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_z", "step_pin", "!" + ori_data);
        break;
    default:
        break;
    };
    // DisErrorMsg();
}

void Printer_para::cmd_M8003(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    DisErrorMsg();
    std::string ori_data = DEFAULT_EXTRUDER_STEP_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("extruder", "step_pin", ori_data);
        break;
    case -1:
        set_cfg("extruder", "step_pin", "!" + ori_data);
        break;
    default:
        break;
    };
    // DisErrorMsg();
}

void Printer_para::cmd_M8004(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    DisErrorMsg();
    std::string ori_data = DEFAULT_STEPPER_X_DIR_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_x", "dir_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_x", "dir_pin", "!" + ori_data);
        break;
    default:
        break;
    };
    // DisErrorMsg();
}

void Printer_para::cmd_M8005(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_STEPPER_Y_DIR_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_y", "dir_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_y", "dir_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8006(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_STEPPER_Z_DIR_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_z", "dir_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_z", "dir_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8007(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_EXTRUDER_DIR_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("extruder", "dir_pin", ori_data);
        break;
    case -1:
        set_cfg("extruder", "dir_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8008(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_STEPPER_X_ENABLE_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_x", "enable_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_x", "enable_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8009(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_STEPPER_X_ENABLE_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_y", "enable_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_y", "enable_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8010(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_STEPPER_Z_ENABLE_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_z", "enable_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_z", "enable_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8011(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_EXTRUDER_ENABLE_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("extruder", "enable_pin", ori_data);
        break;
    case -1:
        set_cfg("extruder", "enable_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8012(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_x", "microsteps", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "microsteps", DEFAULT_STEPPER_X_MICROSTEPS)));
}

void Printer_para::cmd_M8013(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_y", "microsteps", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "microsteps", DEFAULT_STEPPER_Y_MICROSTEPS)));
}

void Printer_para::cmd_M8014(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_z", "microsteps", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "microsteps", DEFAULT_STEPPER_Z_MICROSTEPS)));
}

void Printer_para::cmd_M8015(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("extruder", "microsteps", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "microsteps", DEFAULT_EXTRUDER_MICROSTEPS)));
}

void Printer_para::cmd_M92(GCodeCommand &gcmd)
{
    // if(gcmd.get_string("X","").find(":") != std::string::npos)
    // {
    //     std::string gear_ratio = gcmd.get_string("X","");
    //     double result = 1.;
    //     std::istringstream iss(gear_ratio);	// 输入流
    //     std::string token1;			// 接收缓冲区
    //     while (getline(iss, token1, ','))	// 以split为分隔符
    //     {
    //         std::string token2;
    //         std::istringstream iss(token2);	// 输入流
    //         std::vector<double> out;
    //         while (getline(iss, token2, ':'))
    //         {
    //             out.push_back(atof(token2.c_str()));
    //         }
    //         double g1 = out[0];
    //         double g2 = out[1];
    //         double result = result * (g1 / g2);
    //     }

    // }
    // else
    // if(double stepper_per_mm = gcmd.get_double("X", -1, 0) != -1)
    // {
    //     double full_steps_per_rotation = Printer::GetInstance()->m_pconfig->GetInt("stepper_x", "full_steps_per_rotation", 200, 1);
    //     double microsteps = Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "microsteps", 16);
    //     double rotation_distance = full_steps_per_rotation * microsteps / stepper_per_mm;
    //     set_cfg("stepper_x", "rotation_distance", rotation_distance);
    // }
    // if(double stepper_per_mm = gcmd.get_double("Y", -1, 0) != -1)
    // {
    //     double full_steps_per_rotation = Printer::GetInstance()->m_pconfig->GetInt("stepper_y", "full_steps_per_rotation", 200, 1);
    //     double microsteps = Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "microsteps", 16);
    //     double rotation_distance = full_steps_per_rotation * microsteps / stepper_per_mm;
    //     set_cfg("stepper_y", "rotation_distance", rotation_distance);
    // }
    // if(double stepper_per_mm = gcmd.get_double("Z", -1, 0) != -1)
    // {
    //     double full_steps_per_rotation = Printer::GetInstance()->m_pconfig->GetInt("stepper_z", "full_steps_per_rotation", 200, 1);
    //     double microsteps = Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "microsteps", 16);
    //     double rotation_distance = full_steps_per_rotation * microsteps / stepper_per_mm;
    //     set_cfg("stepper_z", "rotation_distance", rotation_distance);
    // }
    // if(double stepper_per_mm = gcmd.get_double("E", -1, 0) != -1)
    // {
    //     double full_steps_per_rotation = Printer::GetInstance()->m_pconfig->GetInt("extruder", "full_steps_per_rotation", 200, 1);
    //     double microsteps = Printer::GetInstance()->m_pconfig->GetDouble("extruder", "microsteps", 16);
    //     double rotation_distance = full_steps_per_rotation * microsteps / stepper_per_mm;
    //     set_cfg("extruder", "rotation_distance", rotation_distance);
    // }
}

void Printer_para::cmd_M93(GCodeCommand &gcmd)
{
    // double rotation_distance = Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "rotation_distance");
    // double full_steps_per_rotation = Printer::GetInstance()->m_pconfig->GetInt("stepper_x", "full_steps_per_rotation", 200, 1);
    // double microsteps = Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "microsteps", 16);
    // std::cout << "x_steps_per_unit = " << full_steps_per_rotation * microsteps / rotation_distance << std::endl;

    // rotation_distance = Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "rotation_distance");
    // full_steps_per_rotation = Printer::GetInstance()->m_pconfig->GetInt("stepper_y", "full_steps_per_rotation", 200, 1);
    // microsteps = Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "microsteps", 16);
    // std::cout << "y_steps_per_unit = " << full_steps_per_rotation * microsteps / rotation_distance << std::endl;

    // rotation_distance = Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "rotation_distance");
    // full_steps_per_rotation = Printer::GetInstance()->m_pconfig->GetInt("stepper_z", "full_steps_per_rotation", 200, 1);
    // microsteps = Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "microsteps", 16);
    // std::cout << "z_steps_per_unit = " << full_steps_per_rotation * microsteps / rotation_distance << std::endl;

    // rotation_distance = Printer::GetInstance()->m_pconfig->GetDouble("extruder", "rotation_distance");
    // full_steps_per_rotation = Printer::GetInstance()->m_pconfig->GetInt("extruder", "full_steps_per_rotation", 200, 1);
    // microsteps = Printer::GetInstance()->m_pconfig->GetDouble("extruder", "microsteps", 16);
    // std::cout << "e_steps_per_unit = " << full_steps_per_rotation * microsteps / rotation_distance << std::endl;
}

void Printer_para::cmd_M8016(GCodeCommand &gcode)
{
    if (!is_root && !Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_x", "full_steps_per_rotation", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "full_steps_per_rotation", DEFAULT_STEPPER_X_FULL_STEPS_PER_ROTATION)));
}

void Printer_para::cmd_M8017(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_y", "full_steps_per_rotation", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "full_steps_per_rotation", DEFAULT_STEPPER_Y_FULL_STEPS_PER_ROTATION)));
}

void Printer_para::cmd_M8018(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_z", "full_steps_per_rotation", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "full_steps_per_rotation", DEFAULT_STEPPER_Z_FULL_STEPS_PER_ROTATION)));
}

void Printer_para::cmd_M8019(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("extruder", "full_steps_per_rotation", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "full_steps_per_rotation", DEFAULT_EXTRUDER_FULL_STEPS_PER_ROTATION)));
}

void Printer_para::cmd_M8020(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_x", "step_distance", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "step_distance", DEFAULT_STEPPER_X_STEP_DISTANCE)));
}

void Printer_para::cmd_M8021(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_y", "step_distance", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "step_distance", DEFAULT_STEPPER_Y_STEP_DISTANCE)));
}

void Printer_para::cmd_M8022(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("stepper_z", "step_distance", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "step_distance", DEFAULT_STEPPER_Z_STEP_DISTANCE)));
}

void Printer_para::cmd_M8023(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    set_cfg("extruder", "step_distance", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "step_distance", DEFAULT_EXTRUDER_STEP_DISTANCE)));
}

void Printer_para::cmd_M8024(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_STEPPER_X_ENDSTOP_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_x", "endstop_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_x", "endstop_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8025(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_STEPPER_Y_ENDSTOP_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_y", "endstop_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_y", "endstop_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8026(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string ori_data = DEFAULT_STEPPER_Z_ENDSTOP_PIN;
    switch (gcode.get_int("S", INT_MIN))
    {
    case 1:
        set_cfg("stepper_z", "endstop_pin", ori_data);
        break;
    case -1:
        set_cfg("stepper_z", "endstop_pin", "!" + ori_data);
        break;
    default:
        break;
    };
}

void Printer_para::cmd_M8027(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double position_endstop = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "position_endstop", DEFAULT_STEPPER_X_POSITION_ENDSTOP));
    set_cfg("stepper_x", "position_endstop", position_endstop);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->m_position_endstop = position_endstop;
}

void Printer_para::cmd_M8028(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double position_endstop = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "position_endstop", DEFAULT_STEPPER_Y_POSITION_ENDSTOP));
    set_cfg("stepper_y", "position_endstop", position_endstop);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->m_position_endstop = position_endstop;
}

void Printer_para::cmd_M8029(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double position_endstop = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "position_endstop", DEFAULT_STEPPER_Z_POSITION_ENDSTOP));
    set_cfg("stepper_z", "position_endstop", position_endstop);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop = position_endstop;
}

void Printer_para::cmd_M8030(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double position_max = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "position_max", DEFAULT_STEPPER_X_POSITION_MAX));
    set_cfg("stepper_x", "position_max", position_max);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->m_position_max = position_max;
}

void Printer_para::cmd_M8031(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double position_max = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "position_max", DEFAULT_STEPPER_Y_POSITION_MAX));
    set_cfg("stepper_y", "position_max", position_max);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->m_position_max = position_max;
}

void Printer_para::cmd_M8032(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double position_max = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "position_max", DEFAULT_STEPPER_Z_POSITION_MAX));
    set_cfg("stepper_z", "position_max", position_max);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_max = position_max;
}

void Printer_para::cmd_M8033(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "homing_speed", DEFAULT_STEPPER_X_HOMING_SPEED));
    set_cfg("stepper_x", "homing_speed", homing_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->m_homing_speed = homing_speed;
}

void Printer_para::cmd_M8034(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "homing_speed", DEFAULT_STEPPER_Y_HOMING_SPEED));
    set_cfg("stepper_y", "homing_speed", homing_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->m_homing_speed = homing_speed;
}

void Printer_para::cmd_M8035(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "homing_speed", DEFAULT_STEPPER_Z_HOMING_SPEED));
    set_cfg("stepper_z", "homing_speed", homing_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_homing_speed = homing_speed;
}

void Printer_para::cmd_M8036(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double second_homing_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "second_homing_speed", DEFAULT_STEPPER_X_HOMING_SPEED / 2));
    set_cfg("stepper_x", "second_homing_speed", second_homing_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->m_second_homing_speed = second_homing_speed;
}

void Printer_para::cmd_M8037(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double second_homing_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "second_homing_speed", DEFAULT_STEPPER_X_HOMING_SPEED / 2));
    set_cfg("stepper_y", "second_homing_speed", second_homing_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->m_second_homing_speed = second_homing_speed;
}

void Printer_para::cmd_M8038(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double second_homing_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "second_homing_speed", DEFAULT_STEPPER_X_HOMING_SPEED / 2));
    set_cfg("stepper_z", "second_homing_speed", second_homing_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_second_homing_speed = second_homing_speed;
}

void Printer_para::cmd_M8039(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_retract_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "homing_retract_speed", DEFAULT_STEPPER_X_HOMING_SPEED));
    set_cfg("stepper_x", "homing_retract_speed", homing_retract_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->m_homing_retract_speed = homing_retract_speed;
}

void Printer_para::cmd_M8040(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_retract_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "homing_retract_speed", DEFAULT_STEPPER_Y_HOMING_SPEED));
    set_cfg("stepper_y", "homing_retract_speed", homing_retract_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->m_homing_retract_speed = homing_retract_speed;
}

void Printer_para::cmd_M8041(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_retract_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "homing_retract_speed", DEFAULT_STEPPER_Z_HOMING_SPEED));
    set_cfg("stepper_z", "homing_retract_speed", homing_retract_speed);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_homing_retract_speed = homing_retract_speed;
}

void Printer_para::cmd_M8042(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_retract_dist = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_x", "homing_retract_dist", DEFAULT_STEPPER_X_HOMING_RETRACT_DIST));
    set_cfg("stepper_x", "homing_retract_dist", homing_retract_dist);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[0]->m_homing_retract_dist = homing_retract_dist;
}

void Printer_para::cmd_M8043(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_retract_dist = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_y", "homing_retract_dist", DEFAULT_STEPPER_Y_HOMING_RETRACT_DIST));
    set_cfg("stepper_y", "homing_retract_dist", homing_retract_dist);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[1]->m_homing_retract_dist = homing_retract_dist;
}

void Printer_para::cmd_M8044(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double homing_retract_dist = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("stepper_z", "homing_retract_dist", DEFAULT_STEPPER_Z_HOMING_RETRACT_DIST));
    set_cfg("stepper_z", "homing_retract_dist", homing_retract_dist);
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_homing_retract_dist = homing_retract_dist;
}
// virtrual endstop
void Printer_para::cmd_M8045(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int virtrual_endstop = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("stepper_x", "virtrual_endstop", DEFAULT_STEPPER_X_VIRTRUAL_ENDSTOP));
    set_cfg("stepper_x", "virtrual_endstop", virtrual_endstop);
}

void Printer_para::cmd_M8046(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int virtrual_endstop = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("stepper_y", "virtrual_endstop", DEFAULT_STEPPER_Y_VIRTRUAL_ENDSTOP));
    set_cfg("stepper_y", "virtrual_endstop", virtrual_endstop);
}

void Printer_para::cmd_M8047(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int virtrual_endstop = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("stepper_z", "virtrual_endstop", DEFAULT_STEPPER_Z_VIRTRUAL_ENDSTOP));
    set_cfg("stepper_z", "virtrual_endstop", virtrual_endstop);
}

// extruder
void Printer_para::cmd_M8050(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double nozzle_diameter = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "nozzle_diameter", DEFAULT_EXTRUDER_NOZZLE_DIAMETER));
    set_cfg("extruder", "nozzle_diameter", nozzle_diameter);
}

void Printer_para::cmd_M8051(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double filament_diameter = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "filament_diameter", DEFAULT_EXTRUDER_FILAMENT_DIAMETER));
    set_cfg("extruder", "filament_diameter", filament_diameter);
}

void Printer_para::cmd_M8052(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double pid_Kp = gcode.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "pid_Kp", DEFAULT_EXTRUDER_PID_KP));
    double pid_Ki = gcode.get_double("I", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "pid_Ki", DEFAULT_EXTRUDER_PID_KI));
    double pid_Kd = gcode.get_double("D", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "pid_Kd", DEFAULT_EXTRUDER_PID_KD));
    set_cfg("extruder", "pid_Kp", pid_Kp);
    set_cfg("extruder", "pid_Ki", pid_Ki);
    set_cfg("extruder", "pid_Kd", pid_Kd);
    Printer::GetInstance()->m_printer_extruder->m_heater->m_control->set_pid(pid_Kp, pid_Ki, pid_Kd);
}

void Printer_para::cmd_M8053(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double min_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "min_temp", DEFAULT_EXTRUDER_MIN_TEMP));
    set_cfg("extruder", "min_temp", min_temp);
    Printer::GetInstance()->m_printer_extruder->m_heater->m_min_temp = min_temp;
}

void Printer_para::cmd_M8054(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "max_temp", DEFAULT_EXTRUDER_MAX_TEMP));
    set_cfg("extruder", "max_temp", max_temp);
    Printer::GetInstance()->m_printer_extruder->m_heater->m_max_temp = max_temp;
}

void Printer_para::cmd_M8055(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double pressure_advance = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("extruder", "pressure_advance", DEFAULT_EXTRUDER_PRESSURE_ADVANCE));
    set_cfg("extruder", "pressure_advance", pressure_advance);
    Printer::GetInstance()->m_gcode_io->single_command("SET_PRESSURE_ADVANCE ADVANCE=" + to_string(pressure_advance));
}

void Printer_para::cmd_M8056(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_error = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("verify_heater extruder", "max_error", DEFAULT_VERIFY_HEATER_EXTRUDER_MAX_ERROR));
    set_cfg("verify_heater extruder", "max_error", max_error);
}

void Printer_para::cmd_M8057(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double check_gain_time = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("verify_heater extruder", "check_gain_time", DEFAULT_VERIFY_HEATER_EXTRUDER_CHECK_GAIN_TIME));
    set_cfg("verify_heater extruder", "check_gain_time", check_gain_time);
}

void Printer_para::cmd_M8058(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double hysteresis = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("verify_heater extruder", "hysteresis", DEFAULT_VERIFY_HEATER_EXTRUDER_HYSTERESIS));
    set_cfg("verify_heater extruder", "hysteresis", hysteresis);
}

void Printer_para::cmd_M8059(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double heating_gain = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("verify_heater extruder", "heating_gain", DEFAULT_VERIFY_HEATER_EXTRUDER_HEATING_GAIM));
    set_cfg("verify_heater extruder", "heating_gain", heating_gain);
}

// heater_bed
void Printer_para::cmd_M8060(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double min_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("heater_bed", "min_temp", DEFAULT_HEATER_BED_MIN_TEMP));
    set_cfg("heater_bed", "min_temp", min_temp);
    Printer::GetInstance()->m_bed_heater->m_heater->m_min_temp = min_temp;
}

void Printer_para::cmd_M8061(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("heater_bed", "max_temp", DEFAULT_HEATER_BED_MAX_TEMP));
    set_cfg("heater_bed", "max_temp", max_temp);
    Printer::GetInstance()->m_bed_heater->m_heater->m_max_temp = max_temp;
}

void Printer_para::cmd_M8062(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double pid_Kp = gcode.get_double("P", Printer::GetInstance()->m_pconfig->GetDouble("heater_bed", "pid_Kp", DEFAULT_EXTRUDER_PID_KP));
    double pid_Ki = gcode.get_double("I", Printer::GetInstance()->m_pconfig->GetDouble("heater_bed", "pid_Ki", DEFAULT_EXTRUDER_PID_KI));
    double pid_Kd = gcode.get_double("D", Printer::GetInstance()->m_pconfig->GetDouble("heater_bed", "pid_Kd", DEFAULT_EXTRUDER_PID_KD));
    set_cfg("heater_bed", "pid_Kp", pid_Kp);
    set_cfg("heater_bed", "pid_Ki", pid_Ki);
    set_cfg("heater_bed", "pid_Kd", pid_Kd);
    Printer::GetInstance()->m_bed_heater->m_heater->m_control->set_pid(pid_Kp, pid_Ki, pid_Kd);
}

void Printer_para::cmd_M8066(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_error = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("verify_heater heater_bed", "max_error", DEFAULT_VERIFY_HEATER_HEATER_BED_MAX_ERROR));
    set_cfg("verify_heater heater_bed", "max_error", max_error);
}

void Printer_para::cmd_M8067(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double check_gain_time = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("verify_heater heater_bed", "check_gain_time", DEFAULT_VERIFY_HEATER_HEATER_BED_CHECK_GAIN_TIME));
    set_cfg("verify_heater heater_bed", "check_gain_time", check_gain_time);
}

void Printer_para::cmd_M8068(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double hysteresis = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("verify_heater heater_bed", "hysteresis", DEFAULT_VERIFY_HEATER_HEATER_BED_HYSTERESIS));
    set_cfg("verify_heater heater_bed", "hysteresis", hysteresis);
}

void Printer_para::cmd_M8069(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double heating_gain = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("verify_heater heater_bed", "heating_gain", DEFAULT_VERIFY_HEATER_HEATER_BED_HEATING_GAIM));
    set_cfg("verify_heater heater_bed", "heating_gain", heating_gain);
}

// resonance_tester
void Printer_para::cmd_M8070(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double min_freq = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("resonance_tester", "min_freq", DEFAULT_RESONANCE_TESTER_MIN_FREQ));
    set_cfg("resonance_tester", "min_freq", min_freq);
    Printer::GetInstance()->m_resonance_tester->m_test->m_min_freq = min_freq;
}

void Printer_para::cmd_M8071(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_freq = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("resonance_tester", "max_freq", DEFAULT_RESONANCE_TESTER_MAX_FREQ));
    set_cfg("resonance_tester", "max_freq", max_freq);
    Printer::GetInstance()->m_resonance_tester->m_test->m_max_freq = max_freq;
}

void Printer_para::cmd_M8072(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double accel_per_hz = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("resonance_tester", "accel_per_hz", DEFAULT_RESONANCE_TESTER_ACCEL_PER_HZ));
    set_cfg("resonance_tester", "accel_per_hz", accel_per_hz);
    Printer::GetInstance()->m_resonance_tester->m_test->m_accel_per_hz = accel_per_hz;
}
void Printer_para::cmd_M8073(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double hz_per_sec = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("resonance_tester", "hz_per_sec", DEFAULT_RESONANCE_TESTER_HZ_PER_SEC));
    set_cfg("resonance_tester", "hz_per_sec", hz_per_sec);
    Printer::GetInstance()->m_resonance_tester->m_test->m_hz_per_sec = hz_per_sec;
}

void Printer_para::cmd_M8074(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_smoothing = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("resonance_tester", "max_smoothing", DEFAULT_RESONANCE_TESTER_MAX_SMOOTHING));
    set_cfg("resonance_tester", "max_smoothing", gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("resonance_tester", "max_smoothing", DEFAULT_RESONANCE_TESTER_MAX_SMOOTHING)));
}

// printer
void Printer_para::cmd_M8080(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_velocity = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_velocity", DEFAULT_PRINTER_MAX_VELOCITY));
    set_cfg("printer", "max_velocity", max_velocity);
    Printer::GetInstance()->m_gcode_io->single_command("SET_VELOCITY_LIMIT VELOCITY=" + to_string(max_velocity));
}

void Printer_para::cmd_M8081(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_accel = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_accel", DEFAULT_PRINTER_MAX_ACCEL));
    set_cfg("printer", "max_accel", max_accel);
    Printer::GetInstance()->m_gcode_io->single_command("SET_VELOCITY_LIMIT ACCEL=" + to_string(max_accel));
}

void Printer_para::cmd_M8082(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_z_velocity = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_z_velocity", DEFAULT_PRINTER_MAX_Z_VELOCITY));
    set_cfg("printer", "max_z_velocity", max_z_velocity);
    Printer::GetInstance()->m_tool_head->m_kin->m_max_z_velocity = max_z_velocity;
}

void Printer_para::cmd_M8083(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double max_z_accel = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("printer", "max_z_accel", DEFAULT_PRINTER_MAX_Z_ACCEL));
    set_cfg("printer", "max_z_accel", max_z_accel);
    Printer::GetInstance()->m_tool_head->m_kin->m_max_z_accel = max_z_accel;
}

void Printer_para::cmd_M8084(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double square_corner_velocity = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("printer", "square_corner_velocity", DEFAULT_PRINTER_SQUARE_CORNER_VELOCITY));
    set_cfg("printer", "square_corner_velocity", square_corner_velocity);
    Printer::GetInstance()->m_gcode_io->single_command("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=" + to_string(square_corner_velocity));
}

void Printer_para::cmd_M8090(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double heater_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("heater_fan hotend_cooling_fan", "heater_temp", DEFAULT_HEATER_FAN_HEATER_TEMP));
    set_cfg("heater_fan hotend_cooling_fan", "heater_temp", heater_temp);
}

void Printer_para::cmd_M8091(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double fan_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("heater_fan hotend_cooling_fan", "fan_speed", DEFAULT_HEATER_FAN_FAN_SPEED));
    set_cfg("heater_fan hotend_cooling_fan", "fan_speed", fan_speed);
}

void Printer_para::cmd_M8095(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::vector<int> heaters;
    heaters.push_back(gcode.get_int("E", 1));
    heaters.push_back(gcode.get_int("B", 1));
    std::vector<std::string> h = {"E", "B"};
    std::map<std::string, std::string> heaters_name;
    heaters_name["E"] = "extruder";
    heaters_name["B"] = "heater_bed";
    std::string ss;
    for (int i = 0; i < heaters.size(); i++)
    {
        if (heaters[i])
        {
            ss += heaters_name[h[i]];
            ss += ",";
        }
    }
    set_cfg("controller_fan board_cooling_fan", "heater", ss);
}

void Printer_para::cmd_M8096(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::vector<int> steppers;
    steppers.push_back(gcode.get_int("X", 1));
    steppers.push_back(gcode.get_int("Y", 1));
    steppers.push_back(gcode.get_int("Z", 1));
    steppers.push_back(gcode.get_int("E", 1));
    std::vector<std::string> axis = {"X", "Y", "Z", "E"};
    std::map<std::string, std::string> steppers_name;
    steppers_name["X"] = "stepper_x";
    steppers_name["Y"] = "stepper_y";
    steppers_name["Z"] = "stepper_z";
    steppers_name["E"] = "extruder";
    std::string ss;
    for (int i = 0; i < steppers.size(); i++)
    {
        if (steppers[i])
        {
            ss += steppers_name[axis[i]];
            ss += ",";
        }
    }
    set_cfg("controller_fan board_cooling_fan", "stepper", ss);
}

void Printer_para::cmd_M8097(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double fan_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("controller_fan board_cooling_fan", "fan_speed", DEFAULT_CONTROLLER_FAN_FAN_SPEED));
    set_cfg("controller_fan board_cooling_fan", "fan_speed", fan_speed);
}

void Printer_para::cmd_M8098(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double heater_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("controller_fan board_cooling_fan", "heater_temp", DEFAULT_CONTROLLER_FAN_IDLE_SPEED));
    set_cfg("controller_fan board_cooling_fan", "idle_speed", heater_temp);
}

void Printer_para::cmd_M8099(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double idle_timeout = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("controller_fan board_cooling_fan", "idle_timeout", DEFAULT_CONTROLLER_FAN_IDLE_TIMEOUT));
    set_cfg("controller_fan board_cooling_fan", "idle_timeout", idle_timeout);
}

// tmc2209
void Printer_para::cmd_M8100(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int interpolate = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_x", "interpolate", DEFAULT_TMC2209_STEPPER_X_INTERPOLATE));
    set_cfg("tmc2209 stepper_x", "interpolate", interpolate);
}

void Printer_para::cmd_M8101(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int interpolate = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_y", "interpolate", DEFAULT_TMC2209_STEPPER_Y_INTERPOLATE));
    set_cfg("tmc2209 stepper_y", "interpolate", interpolate);
}

void Printer_para::cmd_M8102(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int interpolate = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_z", "interpolate", DEFAULT_TMC2209_STEPPER_Z_INTERPOLATE));
    set_cfg("tmc2209 stepper_z", "interpolate", interpolate);
}

void Printer_para::cmd_M8103(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int interpolate = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 extruder", "interpolate", DEFAULT_TMC2209_EXTRUDER_INTERPOLATE));
    set_cfg("tmc2209 extruder", "interpolate", interpolate);
}

void Printer_para::cmd_M8104(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int stealthchop_threshold = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_x", "stealthchop_threshold", DEFAULT_TMC2209_STEPPER_X_STEALTHCHOP_THRESHOLD));
    set_cfg("tmc2209 stepper_x", "stealthchop_threshold", stealthchop_threshold);
}

void Printer_para::cmd_M8105(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int stealthchop_threshold = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_y", "stealthchop_threshold", DEFAULT_TMC2209_STEPPER_Y_STEALTHCHOP_THRESHOLD));
    set_cfg("tmc2209 stepper_y", "stealthchop_threshold", stealthchop_threshold);
}

void Printer_para::cmd_M8106(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int stealthchop_threshold = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_z", "stealthchop_threshold", DEFAULT_TMC2209_STEPPER_Z_STEALTHCHOP_THRESHOLD));
    set_cfg("tmc2209 stepper_z", "stealthchop_threshold", stealthchop_threshold);
}

void Printer_para::cmd_M8107(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int stealthchop_threshold = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 extruder", "stealthchop_threshold", DEFAULT_TMC2209_EXTRUDER_STEALTHCHOP_THRESHOLD));
    set_cfg("tmc2209 extruder", "stealthchop_threshold", stealthchop_threshold);
}

void Printer_para::cmd_M8108(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double run_current = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("tmc2209 stepper_x", "run_current", DEFAULT_TMC2209_STEPPER_X_RUN_CURRENT));
    set_cfg("tmc2209 stepper_x", "run_current", run_current);
}

void Printer_para::cmd_M8109(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double run_current = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("tmc2209 stepper_y", "run_current", DEFAULT_TMC2209_STEPPER_Y_RUN_CURRENT));
    set_cfg("tmc2209 stepper_y", "run_current", run_current);
}

void Printer_para::cmd_M8110(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double run_current = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("tmc2209 stepper_z", "run_current", DEFAULT_TMC2209_STEPPER_Z_RUN_CURRENT));
    set_cfg("tmc2209 stepper_z", "run_current", run_current);
}

void Printer_para::cmd_M8111(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double run_current = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("tmc2209 extruder", "run_current", DEFAULT_TMC2209_EXTRUDER_RUN_CURRENT));
    set_cfg("tmc2209 extruder", "run_current", run_current);
}

void Printer_para::cmd_M8112(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double hold_current = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("tmc2209 stepper_x", "hold_current", DEFAULT_TMC2209_STEPPER_X_HOLD_CURRENT));
    set_cfg("tmc2209 stepper_x", "hold_current", hold_current);
}

void Printer_para::cmd_M8113(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double hold_current = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("tmc2209 stepper_y", "hold_current", DEFAULT_TMC2209_STEPPER_Y_HOLD_CURRENT));
    set_cfg("tmc2209 stepper_y", "hold_current", hold_current);
}

void Printer_para::cmd_M8114(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double hold_current = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("tmc2209 stepper_z", "hold_current", DEFAULT_TMC2209_STEPPER_Z_HOLD_CURRENT));
    set_cfg("tmc2209 stepper_z", "hold_current", hold_current);
}

void Printer_para::cmd_M8115(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double hold_current = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("tmc2209 extruder", "hold_current", DEFAULT_TMC2209_EXTRUDER_HOLD_CURRENT));
    set_cfg("tmc2209 extruder", "hold_current", hold_current);
}

void Printer_para::cmd_M8116(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int microsteps = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_x", "microsteps", DEFAULT_TMC2209_STEPPER_X_MICROSTPES));
    set_cfg("tmc2209 stepper_x", "microsteps", microsteps);
}

void Printer_para::cmd_M8117(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int microsteps = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_y", "microsteps", DEFAULT_TMC2209_STEPPER_Y_MICROSTPES));
    set_cfg("tmc2209 stepper_y", "microsteps", microsteps);
}

void Printer_para::cmd_M8118(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int microsteps = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_z", "microsteps", DEFAULT_TMC2209_STEPPER_Z_MICROSTPES));
    set_cfg("tmc2209 stepper_z", "microsteps", microsteps);
}

void Printer_para::cmd_M8119(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int microsteps = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 extruder", "microsteps", DEFAULT_TMC2209_EXTRUDER_MICROSTPES));
    set_cfg("tmc2209 extruder", "microsteps", microsteps);
}

void Printer_para::cmd_M8120(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int driver_sgthrs = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_x", "driver_sgthrs", DEFAULT_TMC2209_STEPPER_X_DRIVER_SGTHRS));
    set_cfg("tmc2209 stepper_x", "driver_sgthrs", driver_sgthrs);
}

void Printer_para::cmd_M8121(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int driver_sgthrs = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_y", "driver_sgthrs", DEFAULT_TMC2209_STEPPER_Y_DRIVER_SGTHRS));
    set_cfg("tmc2209 stepper_y", "driver_sgthrs", driver_sgthrs);
}

void Printer_para::cmd_M8122(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int driver_sgthrs = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("tmc2209 stepper_z", "driver_sgthrs", DEFAULT_TMC2209_STEPPER_Z_DRIVER_SGTHRS));
    set_cfg("tmc2209 stepper_z", "driver_sgthrs", driver_sgthrs);
}

// bed mesh
void Printer_para::cmd_M8200(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh", "speed", DEFAULT_BED_MESH_SPEED));
    set_cfg("bed_mesh", "speed", speed);
}

void Printer_para::cmd_M8201(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double horizontal_move_z = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh", "horizontal_move_z", DEFAULT_BED_MESH_HORIZONTAL_MOVE_Z));
    set_cfg("bed_mesh", "horizontal_move_z", horizontal_move_z);
}

void Printer_para::cmd_M8202(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string mesh_min = gcode.get_string("S", Printer::GetInstance()->m_pconfig->GetString("bed_mesh", "mesh_min", DEFAULT_BED_MESH_MESH_MIN));
    set_cfg("bed_mesh", "mesh_min", mesh_min);
}

void Printer_para::cmd_M8203(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string mesh_max = gcode.get_string("S", Printer::GetInstance()->m_pconfig->GetString("bed_mesh", "mesh_max", DEFAULT_BED_MESH_MESH_MAX));
    set_cfg("bed_mesh", "mesh_max", mesh_max);
}

void Printer_para::cmd_M8204(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string probe_count = gcode.get_string("S", Printer::GetInstance()->m_pconfig->GetString("bed_mesh", "probe_count", DEFAULT_BED_MESH_PROBE_COUNT));
    set_cfg("bed_mesh", "probe_count", probe_count);
}

// auto leveling
void Printer_para::cmd_M8210(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double bed_mesh_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "bed_mesh_temp", DEFAULT_AUTO_LEVELING_BED_MESH_TEMP));
    set_cfg("auto_leveling", "bed_mesh_temp", bed_mesh_temp);
}

void Printer_para::cmd_M8211(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double extruder_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "extruder_temp", DEFAULT_AUTO_LEVELING_EXTRUDER_TEMP));
    set_cfg("auto_leveling", "extruder_temp", extruder_temp);
}

void Printer_para::cmd_M8212(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
//     double target_spot_x = gcode.get_double("S", Printer::GetInstance()->m_unmodifiable_cfg->GetDouble("auto_leveling", "target_spot_x", DEFAULT_AUTO_LEVELING_TARGET_SPOT_X));
//     Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("auto_leveling", "target_spot_x", target_spot_x);
}

void Printer_para::cmd_M8213(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    // double target_spot_y = gcode.get_double("S", Printer::GetInstance()->m_unmodifiable_cfg->GetDouble("auto_leveling", "target_spot_y", DEFAULT_AUTO_LEVELING_TARGET_SPOT_Y));
    // Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("auto_leveling", "target_spot_y", target_spot_y);
}

void Printer_para::cmd_M8214(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double move_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "move_speed", DEFAULT_AUTO_LEVELING_MOVE_SPEED));
    set_cfg("auto_leveling", "move_speed", move_speed);
}

void Printer_para::cmd_M8215(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double extrude_length = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "extrude_length", DEFAULT_AUTO_LEVELING_EXTRUDE_LENGTH));
    set_cfg("auto_leveling", "extrude_length", extrude_length);
}

void Printer_para::cmd_M8216(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double extrude_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "extrude_speed", DEFAULT_AUTO_LEVELING_EXTRUDE_SPEED));
    set_cfg("auto_leveling", "extrude_speed", extrude_speed);
}

void Printer_para::cmd_M8217(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double extruder_pullback_length = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "extruder_pullback_length", DEFAULT_AUTO_LEVELING_EXTRUDER_PULLBACK_LENGTH));
    set_cfg("auto_leveling", "extruder_pullback_length", extruder_pullback_length);
}

void Printer_para::cmd_M8218(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double extruder_pullback_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "extruder_pullback_speed", DEFAULT_AUTO_LEVELING_EXTRUDER_PULLBACK_SPEED));
    set_cfg("auto_leveling", "extruder_pullback_speed", extruder_pullback_speed);
}

void Printer_para::cmd_M8219(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double extruder_cool_down_temp = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "extruder_cool_down_temp", DEFAULT_AUTO_LEVELING_EXTRUDER_COOL_DOWN_TEMP));
    set_cfg("auto_leveling", "extruder_cool_down_temp", extruder_cool_down_temp);
}

void Printer_para::cmd_M8220(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double lifting_after_completion = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("auto_leveling", "lifting_after_completion", DEFAULT_AUTO_LEVELING_LIFTING_ATER_COMPLETION));
    set_cfg("auto_leveling", "lifting_after_completion", lifting_after_completion);
}

// probe
void Printer_para::cmd_M8230(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int deactivate_on_each_sample = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("probe", "deactivate_on_each_sample", DEFAULT_PROBE_DEACTIVATE_ON_EACH_SAMPLE));
    set_cfg("probe", "deactivate_on_each_sample", deactivate_on_each_sample);
}

void Printer_para::cmd_M8231(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double x_offset = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "x_offset", DEFAULT_PROBE_X_OFFSET));
    set_cfg("probe", "x_offset", x_offset);
}

void Printer_para::cmd_M8232(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double y_offset = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "y_offset", DEFAULT_PROBE_Y_OFFSET));
    set_cfg("probe", "y_offset", y_offset);
}

/**
 * @description: UI再打印中修改z轴便宜：即修改[probe]中的z_offset，同时修改position_endstop，并判断当前选择的AB面，并把z_offset记录到对应[bed_mesh]中的的z_offset中standard_z_offset|enhancement_z_offset中
 *               若参数 p==1 ，则立即生效
 * @author:  
 * @param {GCodeCommand} &gcode
 * @return {*}
 */
void Printer_para::cmd_M8233(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double cur_z_offset = Printer::GetInstance()->m_pconfig->GetDouble("probe", "z_offset", DEFAULT_PROBE_Z_OFFSET);
    // double z_offset_adj = Printer::GetInstance()->m_unmodifiable_cfg->GetDouble("probe", "z_offset_adjust", DEFAULT_PROBE_Z_OFFSET_ADJUST);
    double z_offset = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "z_offset", DEFAULT_PROBE_Z_OFFSET));
    int is_apply = gcode.get_int("P", 1); // 默认不带这个参数，就会应用上，不想马上应用就带个P0
    set_cfg("probe", "z_offset", z_offset);
    set_cfg("stepper_z", "position_endstop", (0.0f - z_offset + Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop_extra));
    if (Printer::GetInstance()->m_bed_mesh->m_platform_material == "standard")
    {
        set_cfg("bed_mesh", "standard_z_offset", z_offset);
        Printer::GetInstance()->m_strain_gauge->m_cfg->m_fix_z_offset = -Printer::GetInstance()->m_unmodifiable_cfg->GetDouble("strain_gauge", "standard_fix_z_offset", Printer::GetInstance()->m_strain_gauge->m_cfg->m_fix_z_offset);
    }
    else
    {
        set_cfg("bed_mesh", "enhancement_z_offset", z_offset);
        Printer::GetInstance()->m_strain_gauge->m_cfg->m_fix_z_offset = -Printer::GetInstance()->m_unmodifiable_cfg->GetDouble("strain_gauge", "enhancement_fix_z_offset", Printer::GetInstance()->m_strain_gauge->m_cfg->m_fix_z_offset);
    }
    std::vector<std::string> v;
    v.push_back("platform_material");
    v.push_back("standard_z_offset");
    v.push_back("enhancement_z_offset");
    Printer::GetInstance()->m_pconfig->WriteI_specified_Ini(USER_CONFIG_PATH, "bed_mesh", v);
    std::cout << "cmd_M8233 probe z_offset : " << z_offset << std::endl;
    std::cout << "cmd_M8233 stepper_z position_endstop : " << (0.0f - z_offset + Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop_extra) << std::endl;
    double diff_z_offset = cur_z_offset - z_offset;
    // z_offset_adj = z_offset_adj - diff_z_offset;
    // Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("probe", "z_offset_adjust", z_offset_adj);
    // Printer::GetInstance()->m_probe->m_z_offset_adjust = z_offset_adj;
    // printf("z_offset_adj = %f\n", z_offset_adj);
    if (is_apply == 1)
    {
        // Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop = (0.0f - z_offset + Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop_extra);
        Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop = (0.0f - z_offset + Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop_extra + Printer::GetInstance()->m_strain_gauge->m_cfg->m_fix_z_offset);
        Printer::GetInstance()->m_probe->m_z_offset = (0.0f - z_offset + Printer::GetInstance()->m_strain_gauge->m_cfg->m_fix_z_offset);
        Printer::GetInstance()->m_bed_mesh_probe->m_z_offset = (0.0f - z_offset + Printer::GetInstance()->m_strain_gauge->m_cfg->m_fix_z_offset);
    }
    Printer::GetInstance()->m_pconfig->WriteIni(CONFIG_PATH);
    std::vector<string> keys;
    keys.push_back("z_offset");
    Printer::GetInstance()->m_pconfig->WriteI_specified_Ini(USER_CONFIG_PATH, "probe", keys);
    keys.clear();
    keys.push_back("position_endstop");
    keys.push_back("position_endstop_extra");
    Printer::GetInstance()->m_pconfig->WriteI_specified_Ini(USER_CONFIG_PATH, "stepper_z", keys);
}

void Printer_para::cmd_M8234(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "speed", DEFAULT_PROBE_SPEED));
    set_cfg("probe", "speed", speed);
}

void Printer_para::cmd_M8235(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int samples = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("probe", "samples", DEFAULT_PROBE_SAMPLES));
    set_cfg("probe", "samples", samples);
}

void Printer_para::cmd_M8236(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double sample_retract_dist = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "sample_retract_dist", DEFAULT_PROBE_SAMPLE_RETRACT_DIST));
    set_cfg("probe", "sample_retract_dist", sample_retract_dist);
}

void Printer_para::cmd_M8237(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double lift_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "lift_speed", DEFAULT_PROBE_LIFT_SPEED));
    set_cfg("probe", "lift_speed", lift_speed);
}

void Printer_para::cmd_M8238(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double samples_tolerance = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "samples_tolerance", DEFAULT_PROBE_SAMPLES_TOLERANCE));
    set_cfg("probe", "samples_tolerance", samples_tolerance);
}

void Printer_para::cmd_M8239(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double samples_tolerance_retries = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "samples_tolerance_retries", DEFAULT_PROBE_SAMPLES_RETRIES));
    set_cfg("probe", "samples_tolerance_retries", samples_tolerance_retries);
}

void Printer_para::cmd_M8240(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double move_after_each_sample = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("probe", "move_after_each_sample", DEFAULT_PROBE_MOVE_AFTER_EACH_SAMPLE));
    set_cfg("probe", "move_after_each_sample", move_after_each_sample);
}

// bed_mesh_probe
void Printer_para::cmd_M8250(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int deactivate_on_each_sample = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("bed_mesh_probe", "deactivate_on_each_sample", DEFAULT_BED_MESH_PROBE_DEACTIVATE_ON_EACH_SAMPLE));
    set_cfg("bed_mesh_probe", "deactivate_on_each_sample", deactivate_on_each_sample);
}

void Printer_para::cmd_M8251(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double x_offset = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh_probe", "x_offset", DEFAULT_BED_MESH_PROBE_X_OFFSET));
    set_cfg("bed_mesh_probe", "x_offset", x_offset);
}

void Printer_para::cmd_M8252(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double y_offset = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh_probe", "y_offset", DEFAULT_BED_MESH_PROBE_Y_OFFSET));
    set_cfg("bed_mesh_probe", "y_offset", y_offset);
}

void Printer_para::cmd_M8253(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    // double z_offset_adjust = gcode.get_double("S", Printer::GetInstance()->m_unmodifiable_cfg->GetDouble("probe", "z_offset_adjust", DEFAULT_PROBE_Z_OFFSET_ADJUST));
    if (gcode.get_int("R", 0) == 1)
    {
        // Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("probe", "z_offset_adjust", DEFAULT_PROBE_Z_OFFSET_ADJUST);
    }
    // Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("probe", "z_offset_adjust", z_offset_adjust);
}

void Printer_para::cmd_M8254(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh_probe", "speed", DEFAULT_BED_MESH_PROBE_SPEED));
    set_cfg("bed_mesh_probe", "speed", speed);
}

void Printer_para::cmd_M8255(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int samples = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("bed_mesh_probe", "samples", DEFAULT_BED_MESH_PROBE_SAMPLES));
    set_cfg("bed_mesh_probe", "samples", samples);
}

void Printer_para::cmd_M8256(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double sample_retract_dist = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh_probe", "sample_retract_dist", DEFAULT_BED_MESH_PROBE_SAMPLE_RETRACT_DIST));
    set_cfg("bed_mesh_probe", "sample_retract_dist", sample_retract_dist);
}

void Printer_para::cmd_M8257(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double lift_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh_probe", "lift_speed", DEFAULT_BED_MESH_PROBE_LIFT_SPEED));
    set_cfg("bed_mesh_probe", "lift_speed", lift_speed);
}

void Printer_para::cmd_M8258(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double samples_tolerance = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh_probe", "samples_tolerance", DEFAULT_BED_MESH_PROBE_SAMPLES_TOLERANCE));
    set_cfg("bed_mesh_probe", "samples_tolerance", samples_tolerance);
}

void Printer_para::cmd_M8259(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    double samples_tolerance_retries = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("bed_mesh_probe", "samples_tolerance_retries", DEFAULT_BED_MESH_PROBE_SAMPLES_RETRIES));
    set_cfg("bed_mesh_probe", "samples_tolerance_retries", samples_tolerance_retries);
}

// save
void Printer_para::cmd_M8800(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    Printer::GetInstance()->m_pconfig->WriteIni(CONFIG_PATH);
    // Printer::GetInstance()->m_unmodifiable_cfg->WriteIni(UNMODIFIABLE_CFG_PATH);
}

void Printer_para::cmd_M8801(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    load_default_config();
    Printer::GetInstance()->m_pconfig->WriteIni(CONFIG_PATH);
    // Printer::GetInstance()->m_unmodifiable_cfg->WriteIni(UNMODIFIABLE_CFG_PATH);
}

// 重启
void Printer_para::cmd_M8802(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    system("reboot");
}

// 拷贝配置文件
void Printer_para::cmd_M8803(GCodeCommand &gcode)
{
    system("cp /board-resource/printer.cfg /mnt/exUDISK/printer.cfg");
    if (access("/board-resource/user_printer.cfg", F_OK) == 0)
        system("cp /board-resource/user_printer.cfg /mnt/exUDISK/user_printer.cfg");
    if (access("/board-resource/unmodifiable.cfg", F_OK) == 0)
        system("cp /board-resource/unmodifiable.cfg /mnt/exUDISK/unmodifiable.cfg");
    system("sync");
}
// 修改语言
void Printer_para::cmd_M8804(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int language = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("system", "language", DEFAULT_SYSTEM_LANGUAGE));
    set_cfg("system", "language", language);
}
// 修改蜂鸣器开关状态
void Printer_para::cmd_M8805(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int sound_switch = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("system", "sound", DEFAULT_SYSTEM_SOUND_SWITCH));
    set_cfg("system", "sound", sound_switch);
}
// 修改断料检测开关状态
void Printer_para::cmd_M8806(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int material_breakage_detection_switch = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("system", "material_breakage_detection", DEFAULT_SYSTEM_MATERIAL_BREAK_DETECTION));
    set_cfg("system", "material_breakage_detection", material_breakage_detection_switch);
}
// 拷贝u盘配置到系统
void Printer_para::cmd_M8807(GCodeCommand &gcode)
{
    system("cp /mnt/exUDISK/printer.cfg /board-resource/printer.cfg");
    if (access("/mnt/exUDISK/user_printer.cfg", F_OK) == 0)
        system("cp /mnt/exUDISK/user_printer.cfg /board-resource/user_printer.cfg");
    system("sync");
}
// 首次开机
void Printer_para::cmd_M8808(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int boot = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("system", "boot", 1));
    set_cfg("system", "boot", boot);
}
void Printer_para::cmd_M503(GCodeCommand &gcmd)
{
    // std::string str_line = "";
    // fstream fp(CONFIG_PATH, ios::trunc | ios::out );
    // if (!fp.is_open())
    // {
    //     serial_log("open file failed.");
    // }
    // while(getline(fp, str_line))
    // {
    //     serial_log(str_line);
    // }
    // fp.close();
    return;
}
// WIFI 开关
void Printer_para::cmd_M8809(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int wifi_switch = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("system", "wifi", DEFAULT_WIFI_SEITCH));
    set_cfg("system", "wifi", wifi_switch);
}
void Printer_para::cmd_M8810(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    char disk_path[1024];
    if (hl_disk_get_default_mountpoint(HL_DISK_TYPE_USB, NULL, disk_path, sizeof(disk_path)) == 0)
    {
        LOG_I("export log path %s\n", disk_path);
        // utils_vfork_system("cp %s/coredump.gz %s", disk_path);
        log_export_to_path(disk_path);
        utils_vfork_system("cp /user-resource/core %s", disk_path);
        utils_vfork_system("sync");
    }
}
// 地区
void Printer_para::cmd_M8811(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int area = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("system", "area", 0));
    set_cfg("system", "area", area);
}

// 运行模式
void Printer_para::cmd_M8815(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int run_mode = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("system", "run_mode", DEFAULT_SYSTEM_RUN_MODE));
    set_cfg("system", "run_mode", run_mode);
}
void Printer_para::cmd_M8812(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int extrude_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("system", "extrude_speed", 0));
    set_cfg("system", "extrude_speed", extrude_speed);
}

void Printer_para::cmd_M8813(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int retract_speed = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("system", "retract_speed", 0));
    set_cfg("system", "retract_speed", retract_speed);
}

void Printer_para::cmd_M8814(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int retract_extrude_length = gcode.get_double("S", Printer::GetInstance()->m_pconfig->GetDouble("system", "retract_extrude_length", 0));
    set_cfg("system", "retract_extrude_length", retract_extrude_length);
}

void Printer_para::cmd_M8816(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    int screen_off_time = gcode.get_int("S", Printer::GetInstance()->m_pconfig->GetInt("system", "screen_off_time", 0));
    set_cfg("system", "screen_off_time", screen_off_time);
}

void Printer_para::cmd_M8819(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    // 将共振测试 自动调平 靶点位置写到文件
}

// void Printer_para::cmd_M8820(GCodeCommand &gcode)
// {
//     if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
//     {
//         return;
//     }
//     hl_system("cp '%s' '%s'", RESOURCES_CONFIG_PATH, CONFIG_PATH);
//     system("fw_setenv parts_clean UDISK");
//     system("rm /user/history.txt");
//     utils_system("rm %s", WLAN_ENTRY_FILE_PATH);
//     system("sync");
//     system("reboot");
// }
// void Printer_para::cmd_M8821(GCodeCommand &gcode)
// {
//     if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
//     {
//         return;
//     }
//     std::string cmd, ssid, psk;
//     std::stringstream ss(gcode.m_commandline);
//     ss >> cmd >> ssid >> psk;
//     ui_wifi_try_connect(ssid.c_str(), psk.c_str(), HL_WLAN_KEY_MGMT_WPA2_PSK);
// }

#define UI_LOAD_CALLBACK_SIZE 16
static ui_load_callback_t ui_load_callback[UI_LOAD_CALLBACK_SIZE];

int ui_load_register_state_callback(homing_state_callback_t state_callback)
{
    for (int i = 0; i < UI_LOAD_CALLBACK_SIZE; i++)
    {
        if (ui_load_callback[i] == NULL)
        {
            ui_load_callback[i] = state_callback;
            return 0;
        }
    }
    return -1;
}
int ui_load_callback_call(int state)
{
    for (int i = 0; i < UI_LOAD_CALLBACK_SIZE; i++)
    {
        if (ui_load_callback[i] != NULL)
        {
            ui_load_callback[i](state);
        }
    }
    return 0;
}
void Printer_para::cmd_M8822(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    ui_load_callback_call(UI_LOAD_ENGINEERING);
}
/**
 * @description: 清空[probe]下的z_offset和设置对应面[bed_mesh]中的standard_fix_z_offset|enhancement_fix_z_offset，并把这个值加到对应面的 [m_unmodifiable_cfg][strain_gauge]中的 standard_fix_z_offset|enhancement_fix_z_offset 中
 *               出厂时使用
 * @author:     
 * @param {GCodeCommand} &gcode
 * @return {*}
 */
void Printer_para::cmd_M8823(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    // double cur_fix_z_offset = Printer::GetInstance()->m_unmodifiable_cfg->GetDouble("strain_gauge", "fix_z_offset", 0.17);
    double cur_fix_z_offset = Printer::GetInstance()->m_strain_gauge->m_cfg->m_fix_z_offset;
    double cur_z_offset = Printer::GetInstance()->m_pconfig->GetDouble("probe", "z_offset", DEFAULT_PROBE_Z_OFFSET);
    std::cout << "cur_fix_z_offset: " << cur_fix_z_offset << " cur_z_offset: " << cur_z_offset << std::endl;
    std::string platform_material = Printer::GetInstance()->m_bed_mesh->m_platform_material;
    if (platform_material == "standard")
    {
        Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("strain_gauge", "standard_fix_z_offset", -(cur_fix_z_offset - cur_z_offset));
        std::cout << "m_unmodifiable_cfg set standard_fix_z_offset: " << -(cur_fix_z_offset - cur_z_offset) << std::endl;
    }
    else if (platform_material == "enhancement")
    {
        Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("strain_gauge", "enhancement_fix_z_offset", -(cur_fix_z_offset - cur_z_offset));
        std::cout << "m_unmodifiable_cfg set enhancement_fix_z_offset: " << -(cur_fix_z_offset - cur_z_offset) << std::endl;
    }
    set_cfg("probe", "z_offset", 0);
    set_cfg("stepper_z", "position_endstop", Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop_extra);
    std::cout << "m_tool_head set position_endstop: " << Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop << std::endl;
    if (Printer::GetInstance()->m_bed_mesh->m_platform_material == "standard")
    {
        set_cfg("bed_mesh", "standard_z_offset", 0);
    }
    else
    {
        set_cfg("bed_mesh", "enhancement_z_offset", 0);
    }
    Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop = Printer::GetInstance()->m_tool_head->m_kin->m_rails[2]->m_position_endstop_extra;
    Printer::GetInstance()->m_probe->m_z_offset = 0;
    Printer::GetInstance()->m_bed_mesh_probe->m_z_offset = 0;
    Printer::GetInstance()->m_pconfig->WriteIni(CONFIG_PATH);
    Printer::GetInstance()->m_unmodifiable_cfg->WriteIni(UNMODIFIABLE_CFG_PATH);
    std::vector<string> keys;
    keys.push_back("z_offset");
    Printer::GetInstance()->m_pconfig->WriteI_specified_Ini(USER_CONFIG_PATH, "probe", keys);
    keys.clear();
    keys.push_back("position_endstop");
    keys.push_back("position_endstop_extra");
    // TODO: 清除 position_endstop = position_endstop_extra
    Printer::GetInstance()->m_pconfig->WriteI_specified_Ini(USER_CONFIG_PATH, "stepper_z", keys);
    Printer::GetInstance()->m_pconfig->WriteI_specified_Ini(USER_CONFIG_PATH, "bed_mesh", keys);
    // 应用配置
    if (Printer::GetInstance()->m_bed_mesh->m_platform_material == "standard")
    {
        Printer::GetInstance()->m_gcode_io->single_command("BED_MESH_SET_INDEX TYPE=standard INDEX=0");
    }
    else
    {
        Printer::GetInstance()->m_gcode_io->single_command("BED_MESH_SET_INDEX TYPE=enhancement INDEX=0");
    }
}
/**
 * @description: 把全部面的fix_z_offset恢复到默认值 Gcode
 * @author:  
 * @param {GCodeCommand} &gcode
 * @return {*}
 */
void Printer_para::cmd_M8824(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    func_M8824();
}

/**
 * @description: 把全部面的fix_z_offset恢复到默认值
 * @author:  
 * @param {GCodeCommand} &gcode
 * @return {*}
 */
void Printer_para::func_M8824(bool isUpdate)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    if (isUpdate) {
        Printer::GetInstance()->m_unmodifiable_cfg->SetInt("strain_gauge", "update", 7);
    }
    Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("strain_gauge", "standard_fix_z_offset", 0.15);
    Printer::GetInstance()->m_unmodifiable_cfg->SetDouble("strain_gauge", "enhancement_fix_z_offset", 0.17);
    Printer::GetInstance()->m_unmodifiable_cfg->WriteIni(UNMODIFIABLE_CFG_PATH);
}

/**
 * @description: 清空设备时长记录
 * @author:  
 * @param {GCodeCommand} &gcode
 * @return {*}
 */
void Printer_para::cmd_M8825(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    get_sysconf()->SetInt("system", "cumulative_time", 0);
}

void Printer_para::cmd_M8826(GCodeCommand &gcode)
{
    if (!is_root && Printer::GetInstance()->m_virtual_sdcard->is_cmd_from_sd())
    {
        return;
    }
    std::string filePath = "/mnt/exUDISK/chipid.txt";
    char b_id[64] = {0};
    hl_get_chipid(b_id, sizeof(b_id));
    std::string chipID(b_id);
    std::ofstream outFile(filePath, std::ios::app); // 以追加的方式打开文件
    if (outFile.is_open())
    {
        outFile << chipID << "\n"; // 写入芯片 ID 并换行
        outFile.close();
        LOG_I("chipid writen to file: %s\n", filePath.c_str());
    }
    else
    {
        LOG_E("Failed to open file: %s\n", filePath.c_str());
    }
    system("sync");
}