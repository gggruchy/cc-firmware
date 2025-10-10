#include "motion_report.h"
#include "klippy.h"

APIDumpHelper::APIDumpHelper(std::function<API_data(void)> data_cb, std::function<void()> startstop_cb, double update_interval)
{
}

APIDumpHelper::~APIDumpHelper()
{
}

InternalDumpClient::InternalDumpClient()
{
    m_is_done = false;
}

InternalDumpClient::~InternalDumpClient()
{
}

std::vector<std::vector<uint8_t>> InternalDumpClient::get_message()
{
    return m_msgs;
}

void InternalDumpClient::finalize()
{
    m_is_done = true;
}

bool InternalDumpClient::is_closed()
{
    return m_is_done;
}

void InternalDumpClient::send(std::vector<uint8_t> msg)
{
    m_msgs.push_back(msg);
    if(msg.size() >= 10000)
    {
        // Avoid filling up memory with too many samples
        finalize();
    }
}

DumpStepper::DumpStepper(MCU_stepper* mcu_stepper)
{
    m_mcu_stepper = mcu_stepper;
    m_last_api_clock = 0;
    m_api_dump = new APIDumpHelper(std::bind(&DumpStepper::_api_update, this));
    // wh = self.printer.lookup_object('webhooks')
    //     wh.register_mux_endpoint("motion_report/dump_stepper", "name",
    //                              mcu_stepper.get_name(), self._add_api_client)
}

DumpStepper::~DumpStepper()
{
}

std::vector<struct pull_history_steps> DumpStepper::get_step_queue(uint64_t start_clock, uint64_t end_clock)
{
    MCU_stepper* mcu_stepper = m_mcu_stepper;
    std::vector<std::vector<struct pull_history_steps>> res;
    while(1)
    {
        std::vector<struct pull_history_steps> data = mcu_stepper->dump_steps(128, start_clock, end_clock);
        if(!data.size())
        {
            break;
        }
        res.push_back(data);
        // if count < len(data):
        //     break
        end_clock = data[data.size() - 1].first_clock;
    }
    std::reverse(res.begin(), res.end());
    std::vector<struct pull_history_steps> ret;
    for(auto d : res)
    {
        for(int i = d.size() - 1; i > 0; i--)
        {
            ret.push_back(d[i]);
        }
    }
    return ret;
}

void DumpStepper::log_steps(std::vector<struct pull_history_steps> data)
{
    if(data.size())
        return;
    std::vector<std::string> out;
    out.push_back("Dumping stepper '" + m_mcu_stepper->get_name() +"' (" + m_mcu_stepper->get_mcu()->get_name() + ") " + to_string(data.size()) + "queue_step:");
    for(int i = 0; i < data.size(); i++)
    {
        std::stringstream qs;
        qs << "queue_step " << i << ": t=" << data[i].first_clock << " p=" << data[i].start_position << " i=" << data[i].interval << " c=" << data[i].step_count << " a=" << data[i].add;
        out.push_back(qs.str());
    }
    // logging.info('\n'.join(out))
}

API_data DumpStepper::_api_update()
{
    std::vector<struct pull_history_steps> data = get_step_queue(m_last_api_clock, 1ull<<63);
    if(data.size())
    {
        return API_data{};
    }
    std::function<double(uint64_t)> clock_to_print_time = std::bind(&MCU::clock_to_print_time, m_mcu_stepper->get_mcu(), std::placeholders::_1); 
    struct pull_history_steps first = data[0];
    uint64_t first_clock = first.first_clock;
    double first_time = clock_to_print_time(first_clock);
    uint64_t last_clock = data[data.size() - 1].last_clock;
    m_last_api_clock = last_clock;
    int64_t mcu_pos = first.start_position;
    double start_position = m_mcu_stepper->mcu_to_command_position(mcu_pos);
    double step_dist = m_mcu_stepper->get_step_dist();
    // if(m_mcu_stepper.get_dir)
}


DumpTrapQ::DumpTrapQ(std::string name, trapq* dtrapq)
{
}

DumpTrapQ::~DumpTrapQ()
{
}

PrinterMotionReport::PrinterMotionReport()
{
    // get_status information
    m_next_status_time = 0.;
    m_last_status.live_velocity = 0;
    m_last_status.live_extruder_velocity = 0;

    // Register handlers
    Printer::GetInstance()->register_event_handler("klippy:connect", std::bind(&PrinterMotionReport::_connect, this));
    Printer::GetInstance()->register_event_handler("klippy:shutdown", std::bind(&PrinterMotionReport::_shutdown, this));
}

PrinterMotionReport::~PrinterMotionReport()
{
}

void PrinterMotionReport::register_stepper(MCU_stepper* mcu_stepper)
{
    DumpStepper* ds = new DumpStepper(mcu_stepper);
    m_steppers[mcu_stepper->get_name()] = ds;
}

void PrinterMotionReport::_connect()
{
    // Lookup toolhead trapq
    trapq* thtrapq = Printer::GetInstance()->m_tool_head->get_trapq();
    m_trapqs["toolhead"] = new DumpTrapQ("toolhead", thtrapq);
    // Lookup extruder trapq
    // for(auto i : range(0,99,1))
    // {
    //     std::string ename = "extruder" + to_string(i);
    //     if(ename == "extruder0")
    //         ename = "extruder";
        std::string ename = "extruder";
        PrinterExtruder* extruder = Printer::GetInstance()->m_tool_head->get_extruder();
    //     if(extruder == nullptr)
    //     {
    //         break;
    //     }
        trapq* etrapq = extruder->get_trapq();
        m_trapqs[ename] = new DumpTrapQ(ename, etrapq);
    // }
    // Populate "trapq" and "steppers" in get_status result
    m_last_status.steppers = m_steppers;
    m_last_status.trapq = m_trapqs;
    
}

void PrinterMotionReport::_dump_shutdown()
{
    // Log stepper queue_steps on mcu that started shutdown (if any)
    double shutdown_time = NEVER_TIME;
    for(auto dstepper : m_steppers)
    {
        // dstepper.second.m
    }
}

void PrinterMotionReport::_shutdown()
{

}

void PrinterMotionReport::get_status(double eventtime)
{

}