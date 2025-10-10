#include "net.h"
#include "klippy.h"

Net::Net()
{
    Printer::GetInstance()->m_gcode->register_command("M550", std::bind(&Net::cmd_M550, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M551", std::bind(&Net::cmd_M551, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M552", std::bind(&Net::cmd_M552, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M553", std::bind(&Net::cmd_M553, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("M554", std::bind(&Net::cmd_M554, this, std::placeholders::_1));
}

Net::~Net()
{

}

void Net::cmd_M550(GCodeCommand& gcmd)
{
    std::string computer_name = gcmd.get_string("P", "");

}

void Net::cmd_M551(GCodeCommand& gcmd)
{
    std::string computer_password = gcmd.get_string("P", "");

}

void Net::cmd_M552(GCodeCommand& gcmd)
{
    int net_id = gcmd.get_int("I", 0); //要管理的网络接口编号（默认为 0)
    std::string net_ip = gcmd.get_string("P", "0.0.0.0"); // 表示使用 DHCP 获取 IP 地址
    int net_enable = gcmd.get_int("S", 0); //-1 = 重置网络接口，0 = 禁用网络，1 = 启用网络作为客户端，2 = 启用网络作为接入点（仅限支持 WiFi 的电子设备）
    int net_socket = gcmd.get_int("R", 80);
    // hl_netif_set_enable((hl_net_interface_t)net_id, net_enable);
    hl_netif_set_ip_address((hl_net_interface_t)net_id, net_ip.c_str());
}
void Net::cmd_M553(GCodeCommand& gcmd)
{
    int net_id = gcmd.get_int("I", 0); //要管理的网络接口编号（默认为 0)
    std::string net_mask = gcmd.get_string("P", "0"); //掩码
    hl_netif_set_netmask((hl_net_interface_t)net_id, net_mask.c_str());
}

void Net::cmd_M554(GCodeCommand& gcmd)
{
    int net_id = gcmd.get_int("I", 0); //要管理的网络接口编号（默认为 0)
    std::string net_gateway = gcmd.get_string("P", "0.0.0.0"); // 网关
    std::string net_dns = gcmd.get_string("S", "0"); //DNS 服务器（仅受带有 DuetPi 系统配置插件的 DSF 3.3 支持）
    hl_netif_set_dns_address((hl_net_interface_t)net_id, net_dns.c_str(), net_dns.c_str());
}