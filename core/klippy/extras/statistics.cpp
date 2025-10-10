#include "statistics.h"
#include "klippy.h"
#include "debug.h"

PrinterSysStats::PrinterSysStats(std::string section_name)
{
    m_last_process_time = 0.;
    m_total_process_time = 0.;
    m_last_load_avg = 0.;
    m_last_mem_avail = 0;
    m_mem_file.open("/proc/meminfo", std::ios::binary);
    Printer::GetInstance()->register_event_handler("klippy:disconnect:PrinterSysStats", std::bind(&PrinterSysStats::_disconnect, this));
}

PrinterSysStats::~PrinterSysStats()
{

}
        
void PrinterSysStats::_disconnect()
{
    if (m_mem_file.fail())
    {
        m_mem_file.close();
    }
}
        
std::string PrinterSysStats::stats(double eventtime)
{
    // Get core usage stats
    double ptime = get_monotonic();
    double pdiff = ptime - m_last_process_time;
    m_last_process_time = ptime;
    if (pdiff > 0.)
    {
        m_total_process_time += pdiff;
    }
    getloadavg(&m_last_load_avg, 1);
    std::string msg = "sysload=" + std::to_string(m_last_load_avg) + "cputime=" + std::to_string(m_total_process_time);
    // Get available system memory
    if (!m_mem_file.fail())
    {
        m_mem_file.seekg(0, std::ios::end);
        while(1)
        {
            std::string line;
            getline(m_mem_file, line);
            if (line.find("MemAvailable:"))
            {
                msg += " memavail=" + std::to_string(m_last_mem_avail);
                break;
            }
        }
    }
    return msg;
}
        
sys_stats_t PrinterSysStats::get_status(double eventtime)
{
    sys_stats_t ret = {
        .total_process_time = m_total_process_time, 
        .last_load_avg = m_last_load_avg, 
        .last_mem_avail = m_last_mem_avail
    };
    return ret;
}
        

PrinterStats::PrinterStats(std::string section_name)
{
    m_stats_timer = Printer::GetInstance()->m_reactor->register_timer("stats_timer", std::bind(&PrinterStats::generate_stats, this, std::placeholders::_1));
    Printer::GetInstance()->register_event_handler("klippy:ready:PrinterStats", std::bind(&PrinterStats::handle_ready, this));
}

PrinterStats::~PrinterStats()
{

}
        
void PrinterStats::handle_ready()
{
        // stats_cb = [o.stats for n, o in self.printer.lookup_objects() if hasattr(o, 'stats')]
        // if self.printer.get_start_args().get('debugoutput') is None:
        //     reactor = self.printer.get_reactor()
        //     reactor.update_timer(self.stats_timer, reactor.NOW)
    // m_mcu = Printer::GetInstance()->m_mcu;
    // m_all_mcus.push_back(m_mcu);
    // for (auto mcu_map : Printer::GetInstance()->m_mcu_map)
    // {
    //     m_all_mcus.push_back(mcu_map.second);
    // }

    GAM_DEBUG_send_MOVE("2_H17-ra-\n");     //---G-G-2023-03-28----未实现所有对象状态定时回调处理---
    if (Printer::GetInstance()->get_start_args("debugoutput") == "")
    {
        Printer::GetInstance()->m_reactor->update_timer(m_stats_timer, Printer::GetInstance()->m_reactor->m_NOW);
    }
}
        
double PrinterStats::generate_stats(double eventtime)
{
    // stats = [cb(eventtime) for cb in self.stats_cb]
    // 生成所有模块的状态信息  status_t
    return eventtime + 1.;  
}