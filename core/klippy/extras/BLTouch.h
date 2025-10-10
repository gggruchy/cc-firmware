#ifndef BLTOUCH_H
#define BLTOUCH_H
#include "mcu_io.h"
#include "gcode.h"
#include "homing.h"
#include "probe_endstop_wrapper_base.h"

class BLTouchEndstopWrapper : public ProbeEndstopWrapperBase{
    private:

    public:
        BLTouchEndstopWrapper(std::string section_name);
        ~BLTouchEndstopWrapper();
        std::map<std::string, double> Commands;
        bool m_probe_touch_mode;
        MCU_pwm* m_mcu_pwm;
        MCU *m_mcu ;
        double m_next_cmd_time;
        double m_action_end_time;
        std::vector<std::string> m_omodes;
        std::string m_output_mode;
        double m_next_test_time;
        bool m_pin_up_not_triggered;
        bool m_pin_up_touch_triggered;
        //Calculate pin move time
        double m_pin_move_time;
        std::string m_cmd_BLTOUCH_DEBUG_help;
        std::string m_cmd_BLTOUCH_STORE_help;

    public:
        void commands_init();
        void handle_mcu_identify();
        void handle_connect();
        void sync_mcu_print_time();
        void sync_print_time();
        void send_cmd(std::string cmd, double duration=BLTOUCH_MIN_CMD_TIME);
        int verify_state(bool triggered);
        void raise_probe();
        void verify_raise_probe();
        void lower_probe();
        void test_sensor();
        void multi_probe_begin();
        void multi_probe_end();
        void probe_prepare(HomingMove* hmove);
        void home_start(double print_time, double sample_time, int sample_count, double rest_time, bool triggered=true);
        void wait_for_trigger(double eventtime);
        void probe_finish(HomingMove* hmove);
        double get_position_endstop();   
        void set_output_mode(std::string mode);
        void store_output_mode(std::string mode);
        void cmd_BLTOUCH_DEBUG(GCodeCommand& gcmd);
        void cmd_BLTOUCH_STORE(GCodeCommand& gcmd);
};
#endif