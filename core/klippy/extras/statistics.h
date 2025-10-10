#ifndef STATISTICS_H
#define STATISTICS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include "reactor.h"

extern "C"
{
    #include "../chelper/pyhelper.h"
}


typedef struct sys_stats_tag{
    double total_process_time;
    double last_load_avg;
    int last_mem_avail;
}sys_stats_t;

typedef struct status_tag{
    sys_stats_t sys_stats;

}status_t;
class PrinterSysStats{
    private:
    
    public:
        PrinterSysStats(std::string section_name);
        ~PrinterSysStats();
        double m_last_process_time;
        double m_total_process_time;
        double m_last_load_avg;
        int m_last_mem_avail;
        int m_mem_file_fp;
        std::ifstream m_mem_file;

    public:

        void _disconnect();
        std::string stats(double eventtime);
        sys_stats_t get_status(double eventtime);
};

class PrinterStats{
    private:

    public:
        PrinterStats(std::string section_name);
        ~PrinterStats();
        ReactorTimerPtr m_stats_timer;
        std::vector<std::function<void(double)>> stats_cb;
        status_t m_stats;


    public:

        void handle_ready();
        double generate_stats(double eventtime);
};

#endif