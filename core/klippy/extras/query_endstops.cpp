#include "query_endstops.h"
#include "klippy.h"

QueryEndstops::QueryEndstops(std::string section_name)
{
    // Register webhook if server is available
    Printer::GetInstance()->m_webhooks->register_endpoint("query_endstops/status", std::bind(&QueryEndstops::_handle_web_request, this, std::placeholders::_1));

    m_cmd_QUERY_ENDSTOPS_help = "Report on the status of each endstop";
    Printer::GetInstance()->m_gcode->register_command("QUERY_ENDSTOPS", std::bind(&QueryEndstops::cmd_QUERY_ENDSTOPS, this, std::placeholders::_1),false, m_cmd_QUERY_ENDSTOPS_help);
    Printer::GetInstance()->m_gcode->register_command("M119", std::bind(&QueryEndstops::cmd_QUERY_ENDSTOPS, this, std::placeholders::_1));
}

QueryEndstops::~QueryEndstops()
{

}


void QueryEndstops::register_endstop(MCU_endstop* mcu_endstop, std::string name)
{
    m_endstops_name.push_back(name);
    m_endstops.push_back(mcu_endstop);
}
        
void QueryEndstops::get_status(double eventtime)
{
    // return {'last_query': {name: value for name, value in self.last_state}}  //---??---QueryEndstops
}
        
void QueryEndstops::_handle_web_request(WebRequest* web_request)
{
    // gc_mutex = self.printer.lookup_object('gcode').get_mutex()
    // toolhead = self.printer.lookup_object('toolhead')
    // with gc_mutex:
    //     print_time = toolhead.get_last_move_time()
    //     self.last_state = [(name, mcu_endstop.query_endstop(print_time))
    //                         for mcu_endstop, name in self.endstops]
    // web_request.send({name: ["open", "TRIGGERED"][not not t]
    //                     for name, t in self.last_state})  //---??---QueryEndstops
}
        
    
void QueryEndstops::cmd_QUERY_ENDSTOPS(GCodeCommand& gcmd)
{
    // Query the endstops
    double print_time = Printer::GetInstance()->m_tool_head->get_last_move_time();
    for (int i = 0; i < m_endstops.size(); i++)
    {
        m_last_state[m_endstops_name[i]] = m_endstops[i]->query_endstop(print_time);
    }
    // Report results
    // msg = " ".join(["%s:%s" % (name, ["open", "TRIGGERED"][not not t]) //---??---QueryEndstops
    //                 for name, t in self.last_state])
    // gcmd.m_respond_raw(msg);
}
        