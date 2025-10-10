#ifndef BED_SCREWS_H
#define BED_SCREWS_H
#include <string>
#include <vector>
#include "gcode.h"

class BedScrews{
    private:

    public:
        BedScrews(std::string section_name);
        ~BedScrews();

        int m_state;
        int m_current_screw;
        bool m_adjust_again;
        // Read config
        std::vector<std::string> m_screws_index_name;
        std::vector<std::vector<double>> m_screws;
        std::vector<std::vector<double>> m_fine_adjust;

        enum{
            adjust, 
            fine
        };
        
        double m_speed;
        double m_lift_speed;
        double m_horizontal_move_z;
        double m_probe_z;
        // Register command
        std::string m_cmd_BED_SCREWS_ADJUST_help;   
        std::string m_cmd_ACCEPT_help;
        std::string m_cmd_ADJUSTED_help;
        std::string m_cmd_ABORT_help;
    public:
        bool move(std::vector<double> coord, double speed);
        void move_to_screw(int state, int screw);
        void unregister_commands();

        void cmd_BED_SCREWS_ADJUST(GCodeCommand& gcmd);
        void cmd_ACCEPT(GCodeCommand& gcmd);
        void cmd_ADJUSTED(GCodeCommand& gcmd);
        void cmd_ABORT(GCodeCommand& gcmd);
};
#endif