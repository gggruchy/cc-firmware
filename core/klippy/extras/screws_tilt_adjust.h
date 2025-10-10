#ifndef SCREWS_TILT_ADJUST_H
#define SCREWS_TILT_ADJUST_H

#include <string>
#include <vector>
#include <map>
#include <math.h>
#include "probe.h"

class ScrewsTiltAdjust{
    private:

    public:
        ScrewsTiltAdjust(std::string section_name);
        ~ScrewsTiltAdjust();

        std::vector<std::vector<double>> m_screws_coord;
        std::vector<std::string> m_screws_name;
        double m_max_diff;
        std::string m_cmd_SCREWS_TILT_CALCULATE_help;
        std::string m_direction;
        std::map<std::string, int> m_threads;
        int m_thread;
        ProbePointsHelper *m_probe_helper;

        void cmd_SCREWS_TILT_CALCULATE(GCodeCommand& gcmd);
        std::string probe_finalize(std::vector<double> offsets, std::vector<std::vector<double>> positions);
};

#endif