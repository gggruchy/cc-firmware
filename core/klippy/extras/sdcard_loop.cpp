#include "sdcard_loop.h"
#include "klippy.h"
#include "Define.h"

SDCardLoop::SDCardLoop(std::string section_name)
{
    m_cmd_SDCARD_LOOP_BEGIN_help = "Begins a looped section in the SD file.";
    m_cmd_SDCARD_LOOP_DESIST_help = "Stops iterating the current loop stack.";
    m_cmd_SDCARD_LOOP_END_help = "Ends a looped section in the SD file.";
    
    Printer::GetInstance()->m_gcode->register_command("SDCARD_LOOP_BEGIN", std::bind(&SDCardLoop::cmd_SDCARD_LOOP_BEGIN, this, std::placeholders::_1), false, m_cmd_SDCARD_LOOP_BEGIN_help);
    Printer::GetInstance()->m_gcode->register_command("SDCARD_LOOP_END", std::bind(&SDCardLoop::cmd_SDCARD_LOOP_END, this, std::placeholders::_1), false, m_cmd_SDCARD_LOOP_END_help);
    Printer::GetInstance()->m_gcode->register_command("SDCARD_LOOP_DESIST", std::bind(&SDCardLoop::cmd_SDCARD_LOOP_DESIST, this, std::placeholders::_1), false, m_cmd_SDCARD_LOOP_DESIST_help);
}

SDCardLoop::~SDCardLoop()
{
    
}
    
void SDCardLoop::cmd_SDCARD_LOOP_BEGIN(GCodeCommand& gcmd)
{
    int count = gcmd.get_int("COUNT", INT32_MIN, 0);
    if (!loop_begin(count))
    {
        printf("Only permitted in SD file.\n");
    }
}
        
    
void SDCardLoop::cmd_SDCARD_LOOP_END(GCodeCommand& gcmd)
{
    if(!loop_end())
    {
        printf("Only permitted in SD file.\n");
    }
}
        
    
void SDCardLoop::cmd_SDCARD_LOOP_DESIST(GCodeCommand& gcmd)
{
    if (!loop_desist())
    {
        printf("Only permitted outside of a SD file.\n");
    }
}
        
bool SDCardLoop::loop_begin(int count)
{
    // if (!m_sdcard.is_cmd_from_sd())
    // {
    //     // Can only run inside of an SD file
    //     return false;
    // }
    // m_loop_stack.push_back((count, m_sdcard.get_file_position()));
    // return true;
}
        
bool SDCardLoop::loop_end()
{
    // if (!m_sdcard.is_cmd_from_sd())
    // {
    //     //Can only run inside of an SD file
    //     return false;
    // }
        
    // // If the stack is empty, no need to skip back
    // if (m_loop_stack.size() == 0)
    //     return true;
    // // Get iteration count and return position
    // std::vector<int> ret_count_posi = m_loop_stack.pop_back();
    // if (ret_count_posi[0] == 0)// Infinite loop
    // {
    //     m_sdcard.set_file_position(ret_count_posi[1]);
    //     std::vector<int> temp = {0, ret_count_posi[1]};
    //     m_loop_stack.push_back(temp);
    // }
    // else if (ret_count_posi[0] == 1 )// Last repeat
    // {
    //     // Nothing to do
    // }
    // else
    // {
    //     //At the next opportunity, seek back to the start of the loop
    //     m_sdcard.set_file_position(ret_count_posi[1]);
    //     // Decrement the count by 1, and add the position back to the stack
    //     std::vector<int> temp = {ret_count_posi[0] - 1, ret_count_posi[1]};
    //     m_loop_stack.push_back(temp);
    // }
    // return true
}
        
bool SDCardLoop::loop_desist()
{
    // if (m_sdcard.is_cmd_from_sd())
    // {
    //     // Can only run outside of an SD file
    //     return false;
    // }
        
    // std::cout << "Desisting existing SD loops" << std::endl;
    // std::vector<std::vector<int>>().swap(m_loop_stack);
    // return true;
}
        
