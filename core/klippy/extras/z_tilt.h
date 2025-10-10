#ifndef Z_TILT_H
#define Z_TILT_H
#include "stepper.h"
#include "probe.h"
class ZAdjustHelper;
class ZAdjustStatus;
class RetryHelper;
class ZTilt;

class ZAdjustHelper{
    private:

    public:
        ZAdjustHelper(std::string section_name , int z_count);
        ~ZAdjustHelper();

        std::string m_name;
        int m_z_count;
        std::vector<MCU_stepper*> m_z_steppers;

        void handle_connect();
        void adjust_steppers(std::vector<double> adjustments, double speed);
};

class ZAdjustStatus{
    private:

    public:
        ZAdjustStatus();
        ~ZAdjustStatus();

        bool m_applied;

        std::string check_retry_result(std::string retry_result);
        std::string reset();    
        std::string get_status(double eventtime);
        void _motor_off(double print_time);

};

class RetryHelper{
    private:

    public:
        RetryHelper(std::string section_name, std::string error_msg_extra = "");
        ~RetryHelper();
        
        int m_default_max_retries;
        double m_default_retry_tolerance;
        std::string m_value_label;
        std::string m_error_msg_extra;
        int m_max_retries;
        double m_retry_tolerance;
        int m_current_retry;
        double m_previous;
        int m_increasing;
        std::string m_section_name;

        void start(GCodeCommand& gcmd);
        bool check_increase(double error);
        std::string check_retry(std::vector<double> z_positions);
};

class ZTilt{
    private:

    public:
        ZTilt(std::string section_name);
        ~ZTilt();

        std::vector<std::vector<double>> m_z_positions;
        RetryHelper *m_retry_helper;
        ProbePointsHelper *m_probe_helper;
        ZAdjustStatus *m_z_status;
        ZAdjustHelper *m_z_helper;
        std::string m_cmd_Z_TILT_ADJUST_help;

        void cmd_Z_TILT_ADJUST(GCodeCommand& gcmd);
        std::string probe_finalize(std::vector<double> offsets, std::vector<std::vector<double>> positions);
        std::string get_status(double eventtime);
};


#endif