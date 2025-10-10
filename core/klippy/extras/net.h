#ifndef NET_H
#define NET_H
#include "gcode.h"

class Net{
    Net();
    ~Net();
    void cmd_M550(GCodeCommand& gcmd);
    void cmd_M551(GCodeCommand& gcmd);
    void cmd_M552(GCodeCommand& gcmd);
    void cmd_M553(GCodeCommand& gcmd);
    void cmd_M554(GCodeCommand& gcmd);
};

#endif