#ifndef GCODE_ARCS_H
#define GCODE_ARCS_H

#include "string.h"
#include <vector>
#include "gcode.h"

class ArcSupport
{
private:
public:
    ArcSupport(std::string section_name);
    ~ArcSupport();
    void cmd_G2(GCodeCommand &gcmd);
    void cmd_G3(GCodeCommand &gcmd);
    void cmd_G17(GCodeCommand &gcmd);
    void cmd_G18(GCodeCommand &gcmd);
    void cmd_G19(GCodeCommand &gcmd);
    void cmd_inner(GCodeCommand &gcmd);
    std::vector<std::vector<double>> planArc(std::vector<double> currentPos, std::vector<double> targetPos, std::vector<double> offset, bool clockwise, std::vector<int> axes);

    double m_mm_per_arc_segment;

    int m_plane;
};



#endif