#include "exclude_object.h"
#include "Define.h"
#include "klippy.h"

ExcludeObject::ExcludeObject(std::string section_name)
{
    Printer::GetInstance()->register_event_handler("klippy:connect:ExcludeObject", std::bind(&ExcludeObject::_handle_connect, this));
    Printer::GetInstance()->register_event_handler("virtual_sdcard:reset_file:ExcludeObject", std::bind(&ExcludeObject::_reset_file, this));

    m_last_position_extruded = {0., 0., 0., 0.};
    m_last_position_excluded = {0., 0., 0., 0.};
    m_cmd_EXCLUDE_OBJECT_START_help = "Marks the beginning the current object as labeled";
    m_cmd_EXCLUDE_OBJECT_END_help = "Marks the end the current object";
    m_cmd_EXCLUDE_OBJECT_help = "Cancel moves inside a specified objects";
    m_cmd_EXCLUDE_OBJECT_DEFINE_help = "Provides a summary of an object";
    _reset_state();
    Printer::GetInstance()->m_gcode->register_command("EXCLUDE_OBJECT_START", std::bind(&ExcludeObject::cmd_EXCLUDE_OBJECT_START, this, std::placeholders::_1), false, m_cmd_EXCLUDE_OBJECT_START_help);
    Printer::GetInstance()->m_gcode->register_command("EXCLUDE_OBJECT_END", std::bind(&ExcludeObject::cmd_EXCLUDE_OBJECT_END, this, std::placeholders::_1), false, m_cmd_EXCLUDE_OBJECT_END_help);
    Printer::GetInstance()->m_gcode->register_command("EXCLUDE_OBJECT", std::bind(&ExcludeObject::cmd_EXCLUDE_OBJECT, this, std::placeholders::_1), false, m_cmd_EXCLUDE_OBJECT_help);
    Printer::GetInstance()->m_gcode->register_command("EXCLUDE_OBJECT_DEFINE", std::bind(&ExcludeObject::cmd_EXCLUDE_OBJECT_DEFINE, this, std::placeholders::_1), false, m_cmd_EXCLUDE_OBJECT_DEFINE_help);
}

ExcludeObject::~ExcludeObject()
{

}
        
void ExcludeObject::_register_transform()
{
    if (m_next_move_transform == nullptr)
    {
        TuningTower *tuning_tower = Printer::GetInstance()->m_tuning_tower;
        if (tuning_tower->is_active())
        {
            printf("The ExcludeObject move transform is not being loaded due to Tuning tower being Active\n");
            return;
        }
        Printer::GetInstance()->m_gcode_move->set_move_transform(std::bind(&ExcludeObject::move, this, std::placeholders::_1, std::placeholders::_2), true);
        Printer::GetInstance()->m_gcode_move->set_get_position_transform(std::bind(&ExcludeObject::get_position, this), true);
        m_max_position_extruded = 0;
        m_extrusion_offsets = {};
        m_max_position_excluded = 0;
        int m_extruder_adj = 0;
        int m_initial_extrusion_moves = 5;
        std::vector<double> m_last_position = {0., 0., 0., 0.};

        get_position();
        std::vector<double> m_last_position_extruded = m_last_position;
        std::vector<double> m_last_position_excluded = m_last_position;
    }
        
}
        
void ExcludeObject::_handle_connect()
{
    // m_toolhead = m_printer.lookup_object("toolhead")  //---??---ExcludeObject
}
       

void ExcludeObject::_unregister_transform()
{
    if (m_next_move_transform != nullptr)
    {
        // tuning_tower = m_printer.lookup_object("tuning_tower") //---??---ExcludeObject
        TuningTower *tuning_tower;
        if (tuning_tower->is_active())
        {
            printf("The Exclude Object move transform was not unregistered because it is not at the head of the transform chain.\n");
            return;
        }
        Printer::GetInstance()->m_gcode_move->set_move_transform(std::bind(&ExcludeObject::move, this, std::placeholders::_1, std::placeholders::_2), true);
        Printer::GetInstance()->m_gcode_move->set_get_position_transform(std::bind(&ExcludeObject::get_position, this), true);
        m_next_move_transform = nullptr;
        m_next_get_position_transform = nullptr;
        Printer::GetInstance()->m_gcode_move->reset_last_position();
    }
}
        

void ExcludeObject::_reset_state()
{
    std::vector<std::string>().swap(m_objects);
    std::vector<std::string>().swap(m_excluded_objects);
    std::string m_current_object = "";
    m_in_excluded_region = false;
}
        

void ExcludeObject::_reset_file()
{
    _reset_state();
    _unregister_transform();
}
        

std::vector<double> ExcludeObject::_get_extrusion_offsets()
{
    // offset = m_extrusion_offsets.get(
    //     m_toolhead.get_extruder().get_name()) 
    std::vector<double> offset = m_extrusion_offsets[0];
    if (offset.size() == 0)
    {
        offset = {0., 0., 0., 0.};
        m_extrusion_offsets.push_back(offset);
    }
    return offset;
}
        

std::vector<double> ExcludeObject::get_position()
{
    std::vector<double> offset = _get_extrusion_offsets();
    std::vector<double> pos = m_next_get_position_transform();
    m_last_position[0] = pos[0] + offset[0];
    m_last_position[1] = pos[1] + offset[1];
    m_last_position[2] = pos[2] + offset[2];
    m_last_position[3] = pos[3] + offset[3];
    return m_last_position;
}
        
std::vector<double> ExcludeObject::_normal_move(std::vector<double> newpos, double speed)
{
    std::vector<double> offset = _get_extrusion_offsets();
    if (m_initial_extrusion_moves > 0 && m_last_position[3] != newpos[3])
    {
        // Since the transform is not loaded until there is a request to
        // exclude an object, the transform needs to track a few extrusions
        // to get the state of the extruder
        m_initial_extrusion_moves -= 1;
    }
    m_last_position = newpos;
    m_last_position_extruded = m_last_position;
    m_max_position_extruded = std::max(m_max_position_extruded, newpos[3]);

    // These next few conditionals handle the moves immediately after leaving
    // and excluded object.  The toolhead is at the end of the last printed
    // object and the gcode is at the end of the last excluded object.
    //
    // Ideally, there will be Z and E moves right away to adjust any offsets
    // before moving away from the last position.  Any remaining corrections
    // will be made on the firs XY move.
    if ((offset[0] != 0 || offset[1] != 0) && (newpos[0] != m_last_position_excluded[0] || newpos[1] != m_last_position_excluded[1]))
    {
        offset[0] = 0;
        offset[1] = 0;
        offset[2] = 0;
        offset[3] += m_extruder_adj;
        m_extruder_adj = 0;
    }
    if (offset[2] != 0 && newpos[2] != m_last_position_excluded[2])
    {
        offset[2] = 0;
    }
    if (m_extruder_adj != 0 && newpos[3] != m_last_position_excluded[3])
    {
        offset[3] += m_extruder_adj;
        m_extruder_adj = 0;
    }
    std::vector<double> tx_pos = newpos;
    tx_pos[0] = newpos[0] - offset[0];
    tx_pos[1] = newpos[1] - offset[1];
    tx_pos[2] = newpos[2] - offset[2];
    tx_pos[3] = newpos[3] - offset[3];
    m_next_move_transform(tx_pos, speed);
}
        
void ExcludeObject::_ignore_move(std::vector<double> newpos, double speed)
{
    std::vector<double> offset = _get_extrusion_offsets();
    offset[0] = newpos[0] - m_last_position_extruded[0];
    offset[1] = newpos[1] - m_last_position_extruded[1];
    offset[2] = newpos[2] - m_last_position_extruded[2];
    offset[3] = offset[3] + newpos[3] - m_last_position[3];

    m_last_position = newpos;
    m_last_position_excluded = m_last_position;
    m_max_position_excluded = std::max(m_max_position_excluded, newpos[3]);
}

void ExcludeObject::_move_into_excluded_region(std::vector<double> newpos, double speed)
{
    m_in_excluded_region = true;
    _ignore_move(newpos, speed);
}
        

void ExcludeObject::_move_from_excluded_region(std::vector<double> newpos, double speed)
{
    m_in_excluded_region = false;
    // This adjustment value is used to compensate for any retraction
    // differences between the last object printed and excluded one.
    m_extruder_adj = m_max_position_excluded - m_last_position_excluded[3] - (m_max_position_extruded - m_last_position_extruded[3]);
    _normal_move(newpos, speed);
}
        
bool ExcludeObject::_test_in_excluded_region()
{
    // Inside cancelled object
    bool find_obj = false;
    for (int i = 0; i < m_excluded_objects.size(); i++)
    {
        if (m_current_object == m_excluded_objects[i])
        {
            find_obj = true;
        }
    }
    return find_obj && m_initial_extrusion_moves == 0;
}
        

struct ExcludeObjectState ExcludeObject::get_status(double eventtime)
{
    struct ExcludeObjectState status = {m_objects, m_excluded_objects, m_current_object};
    return status;
}
        

bool ExcludeObject::move(std::vector<double> newpos, double speed)
{
    bool move_in_excluded_region = _test_in_excluded_region();
    m_last_speed = speed;

    if (move_in_excluded_region)
    {
        if (m_in_excluded_region)
            _ignore_move(newpos, speed);
        else
            _move_into_excluded_region(newpos, speed);
    }
    else {
        if (m_in_excluded_region)
            _move_from_excluded_region(newpos, speed);
        else
            _normal_move(newpos, speed);
    }
    return true;
}
    
void ExcludeObject::cmd_EXCLUDE_OBJECT_START(GCodeCommand& gcmd)
{
    std::string name = gcmd.get_string("NAME", "");
    for (int i = 0; i < m_objects.size(); i++)
    {
        if(name == m_objects[i])
        {
            _add_object_definition(name);
        }
    }
    m_current_object = name;
    m_was_excluded_at_start = _test_in_excluded_region();
}
        

void ExcludeObject::cmd_EXCLUDE_OBJECT_END(GCodeCommand& gcmd)
{
    if (m_current_object == "None" && m_next_move_transform != nullptr)
    {
        gcmd.m_respond_info("EXCLUDE_OBJECT_END called, but no object is currently active", true); //---??---ExcludeObject
        return;
    } 
    std::string name = gcmd.get_string("NAME", "");
    if (name != "None" && name != m_current_object)
    {
        gcmd.m_respond_info("EXCLUDE_OBJECT_END NAME=%s does not match the current object NAME=" + m_current_object, true);  //---??---ExcludeObject
    }
    m_current_object = "";
}
        

void ExcludeObject::cmd_EXCLUDE_OBJECT(GCodeCommand& gcmd)
{
    std::string reset = gcmd.get_string("RESET", "");
    std::string current = gcmd.get_string("CURRENT", "");
    std::string name = gcmd.get_string("NAME", "");
    if (reset != "")
    {
        if (name != "")
            _unexclude_object(name);
        else
            std::vector<std::string>().swap(m_excluded_objects);
    }
    else if (name != "")
    {
        bool obj_state = false;
        for (int i = 0; i < m_excluded_objects.size(); i++)
        {
            if (name == m_excluded_objects[i])
            {
                obj_state = true;
            }
        }
        if (!obj_state)
        {
            _exclude_object(name);
        }
    }
    else if (current != "")
    {
        if (m_current_object == "")
        {
            gcmd.m_respond_error("There is no current object to cancel", true); //---??---ExcludeObject
        }
        else
        {
            _exclude_object(m_current_object);
        }
    }
    else
        _list_excluded_objects(gcmd);
}

void ExcludeObject::cmd_EXCLUDE_OBJECT_DEFINE(GCodeCommand& gcmd)
{
    std::string reset = gcmd.get_string("RESET", "");
    std::string name = gcmd.get_string("NAME", "");
    if (reset != "")
        _reset_file();
    else if (name != "")
    {
        // parameters = gcmd.get_command_parameters().copy()   //---??---ExcludeObject
        // parameters.pop("NAME")
        // center = parameters.pop("CENTER", None)
        // polygon = parameters.pop("POLYGON", None)

        // obj = {"name": name.upper()}
        // obj.update(parameters)

        // if center != None:
        //     obj["center"] = json.loads("[%s]" % center)

        // if polygon != None:
        //     obj["polygon"] = json.loads(polygon)

        // _add_object_definition(obj)
    }
    else
        _list_objects(gcmd);
}
        

void ExcludeObject::_add_object_definition(std::string definition)
{
    m_objects.push_back(definition);
}
        

void ExcludeObject::_exclude_object(std::string name)
{
    _register_transform();
    // m_gcode.respond_info("Excluding object {}".format(name.upper()))  //---??---ExcludeObject
    bool obj_state = false;
    for (int i = 0; i < m_excluded_objects.size(); i++)
    {
        if (name == m_excluded_objects[i])
        {
            obj_state = true;
        }
    }
    if (!obj_state)
    {
        m_excluded_objects.push_back(name);
    }
}
        

void ExcludeObject::_unexclude_object(std::string name)
{
    // m_gcode.respond_info("Unexcluding object {}".format(name.upper()))
    for (int i = 0; i < m_excluded_objects.size(); i++)
    {
        if (name == m_excluded_objects[i])
        {
            m_excluded_objects.erase(m_excluded_objects.begin() + i);
        }

    }
}
        

void ExcludeObject::_list_objects(GCodeCommand& gcmd)
{
    // if gcmd.get("JSON", None) is not None:
    //     object_list = json.dumps(m_objects)
    // else:
    //     object_list = " ".join(obj["name"] for obj in m_objects)
    // gcmd.respond_info("Known objects: {}".format(object_list))  //---??---ExcludeObject
}
        

void ExcludeObject::_list_excluded_objects(GCodeCommand& gcmd)
{
    // object_list = " ".join(m_excluded_objects)
    // gcmd.respond_info("Excluded objects: {}".format(object_list))  //---??---ExcludeObject
}