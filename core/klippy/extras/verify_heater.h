#ifndef VERIFY_HEATER_H
#define VERIFY_HEATER_H
#include "heaters.h"
#include "hl_queue.h"

class HeaterCheck{
    private:

    public:
        HeaterCheck(std::string section_name);
        ~HeaterCheck();

        std::string m_heater_name;
        Heater *m_heater;
        double m_hysteresis;
        double m_max_error;
        double m_heating_gain;
        double m_check_gain_time;
        bool m_approaching_target;
        bool m_starting_approach;
        double m_last_target;
        double m_goal_temp;
        double m_error;
        double m_goal_systime;
        ReactorTimerPtr m_check_timer;

        void handle_connect();
        void handle_shutdown();
        double check_event(double eventtime);
        double heater_fault();
};


typedef enum
{
    VERIFY_HEATER_STATE_HOT_BED_ERROR = 0,
    VERIFY_HEATER_STATE_EXTRUDER_ERROR = 1,
    VERIFY_HEATER_STATE_NTC_HOT_BED_ERROR = 2,
    VERIFY_HEATER_STATE_NTC_EXTRUDER_ERROR = 3,

} ui_event_verify_heater_state_id_t;
typedef void (*verify_heater_state_callback_t)(int state);
void check_heater_init(hl_queue_t *queue, verify_heater_state_callback_t state_callback);
int verify_heater_register_state_callback(verify_heater_state_callback_t state_callback);
int verify_heater_state_callback_call(int state);
bool is_verify_heater_arrayempty(void);

#endif