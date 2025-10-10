#include "canbus_ids.h"
#include "klippy.h"

#define NODEID_FIRST 4

PrinterCANBus::PrinterCANBus(std::string section_name)
{
}

PrinterCANBus::~PrinterCANBus()
{
}

uint32_t PrinterCANBus::add_uuid(std::string canbus_uuid, std::string canbus_iface)
{
    if(m_ids.find(canbus_uuid) != m_ids.end())
        std::cout << "Duplicate canbus_uuid" << std::endl;
    uint32_t new_id = m_ids.size() + NODEID_FIRST;
    m_ids[canbus_uuid] = new_id;
    return new_id;
}

uint32_t PrinterCANBus::get_nodeid(std::string canbus_uuid)
{
    if(m_ids.find(canbus_uuid) == m_ids.end())
        std::cout << "unknown canbus_uuid " << canbus_uuid << std::endl;
    return m_ids[canbus_uuid];
}

