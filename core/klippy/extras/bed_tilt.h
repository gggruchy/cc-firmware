#ifndef BED_TILT_H
#define BED_TILT_H

#include <string>
#include <vector>
#include "toolhead.h"
#include "probe.h"

class BedTilt{
    private:

    public:
        BedTilt(std::string section_name);
        ~BedTilt();

        double m_x_adjust;
        double m_y_adjust;
        double m_z_adjust;
        ToolHead* m_toolhead;

        void handle_connect();
        std::vector<double> get_position();
        bool move(std::vector<double>& newpos, double speed);
        void update_adjust(double x_adjust, double y_adjust, double z_adjust);
};

class BedTiltCalibrate{
    private:

    public:
        BedTiltCalibrate(std::string section_name, BedTilt* bedtilt);
        ~BedTiltCalibrate();

        BedTilt* m_bedtilt;
        ProbePointsHelper * m_probe_helper;
        std::string m_cmd_BED_TILT_CALIBRATE_help;

        void cmd_BED_TILT_CALIBRATE(GCodeCommand& gcmd);
        std::string probe_finalize(std::vector<double> offsets, std::vector<std::vector<double>> positions);

        
};

#endif