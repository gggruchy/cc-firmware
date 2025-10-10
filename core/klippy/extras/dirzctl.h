#ifndef DIRZCTL_H
#define DIRZCTL_H
#include <vector>
#include <string>
#include "msgproto.h"
#include "gcode.h"
#include "mcu_io.h"

class DirZCtl
{
private:
public:
    DirZCtl(std::string section_name);
    ~DirZCtl();
    void _handle_mcu_identify();
    void _build_config(int para);
    bool _handle_shutdown();
    bool _handle_disconnect();
    void _handle_result_dirzctl(ParseResult params);
    std::vector<ParseResult> get_params();
    void check_and_run(int direct, uint32_t step_us, uint32_t step_cnt, bool wait_finish = true, bool is_ck_con = false);
    void cmd_DIRZCTL(GCodeCommand &gcmd);

    std::vector<MCU_stepper *> m_steppers;
    int m_oid;
    MCU *m_mcu;
    int mcu_freq;
    int32_t m_mcu_freq;
    double m_step_base;
    double m_last_send_heart;
    bool m_is_shutdown;
    bool m_is_timeout;
    std::vector<ParseResult> m_params;
    command_queue* m_cmd_queue;
    std::mutex m_mutex;
};

#endif
