#ifndef FIRMWARE_RETRACTION_H
#define FIRMWARE_RETRACTION
#include <string>
#include "gcode.h"

typedef struct FirmwareRetraction_status_tag
{
    double retract_length;
    double retract_speed;
    double unretract_extra_length;
    double unretract_speed;
}FirmwareRetraction_status_t;

class FirmwareRetraction
{
private:
    double m_retract_length;
    double m_retract_speed;
    double m_unretract_extra_length;
    double m_unretract_speed;
    double m_unretract_length;
    bool m_is_retracted = false;

public:
    FirmwareRetraction(std::string section_name);
    ~FirmwareRetraction();
    void cmd_SET_RETRACTION(GCodeCommand &gcmd);
    void cmd_GET_RETRACTION(GCodeCommand &gcmd);
    void cmd_G10(GCodeCommand &gcmd);
    void cmd_G11(GCodeCommand &gcmd);
    
};




#endif