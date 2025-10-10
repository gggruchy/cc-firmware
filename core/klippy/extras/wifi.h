#ifndef WIFI_H
#define WIFI_H

#include "gcode.h"
extern "C"{
    // #include "wifid.h"
}

class WiFi{
    WiFi();
    ~WiFi();

    void cmd_M587(GCodeCommand& gcmd);
    void cmd_M588(GCodeCommand& gcmd);
};

#endif