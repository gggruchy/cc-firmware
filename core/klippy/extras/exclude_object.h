#ifndef EXCLUDE_OBJECT_H
#define EXCLIDE_OBJECT_H

#include "tuning_tower.h"
#include <float.h>

struct ExcludeObjectState{
    std::vector<std::string> objects;
    std::vector<std::string> excluded_objects;
    std::string current_object;
};

class ExcludeObject{
    private:

    public:
        ExcludeObject(std::string section_name);
        ~ExcludeObject();
        std::function<void(std::vector<double>&, double)> m_next_move_transform;
        std::function<std::vector<double>()> m_next_get_position_transform;
        std::vector<double> m_last_position_extruded;
        std::vector<double> m_last_position_excluded;
        std::string m_cmd_EXCLUDE_OBJECT_START_help;
        std::string m_cmd_EXCLUDE_OBJECT_END_help;
        std::string m_cmd_EXCLUDE_OBJECT_help;
        std::string m_cmd_EXCLUDE_OBJECT_DEFINE_help;
        
        std::vector<std::vector<double>> m_extrusion_offsets;
        double m_max_position_extruded;
        double m_max_position_excluded;
        int m_extruder_adj;
        int m_initial_extrusion_moves;
        std::vector<double> m_last_position;
        std::vector<std::string> m_objects;
        std::vector<std::string> m_excluded_objects;
        std::string m_current_object = "";
        bool m_in_excluded_region;
        double m_last_speed;
        bool m_was_excluded_at_start;
    public:

        void _register_transform();
        void _handle_connect();
        void _unregister_transform();
        void _reset_state();
        void _reset_file();
        std::vector<double> _get_extrusion_offsets();
        std::vector<double> get_position();
        std::vector<double> _normal_move(std::vector<double> newpos, double speed);
        void _ignore_move(std::vector<double> newpos, double speed);
        void _move_into_excluded_region(std::vector<double> newpos, double speed);
        void _move_from_excluded_region(std::vector<double> newpos, double speed);
        bool _test_in_excluded_region();
        struct ExcludeObjectState get_status(double eventtime = DBL_MIN);
        bool move(std::vector<double> newpos, double speed);
        void cmd_EXCLUDE_OBJECT_START(GCodeCommand& gcmd);
        void cmd_EXCLUDE_OBJECT_END(GCodeCommand& gcmd);
        void cmd_EXCLUDE_OBJECT(GCodeCommand& gcmd);
        void cmd_EXCLUDE_OBJECT_DEFINE(GCodeCommand& gcmd);
        void _add_object_definition(std::string definition);
        void _exclude_object(std::string name);
        void _unexclude_object(std::string name);
        void _list_objects(GCodeCommand& gcmd);
        void _list_excluded_objects(GCodeCommand& gcmd);
};

#endif