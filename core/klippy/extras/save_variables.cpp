#include "save_variables.h"
#include "Define.h"


SaveVariables::SaveVariables()
{
    // m_filename = SAVE_VARIABLES_FILE_NAME;

    // self.allVariables = {}
    // try:
    //     self.load_variables()
    // except self.printer.command_error as e:
    //     raise config.error(str(e))
    // gcode = self.printer.lookup_object('gcode')
    // m_cmd_SAVE_VARIABLE_help = "Save arbitrary variables to disk";
    // gcode.register_command("SAVE_VARIABLE", self.cmd_SAVE_VARIABLE, false, m_cmd_SAVE_VARIABLE_help); //---??---
}

SaveVariables::~SaveVariables()
{

}

       
void SaveVariables::load_variables()
{
    // allvars = {}
    // varfile = configparser.ConfigParser()
    // try:
    //     varfile.read(self.filename)
    //     if varfile.has_section('Variables'):
    //         for name, val in varfile.items('Variables'):
    //             allvars[name] = ast.literal_eval(val)
    // except:
    //     msg = "Unable to parse existing variable file"
    //     logging.exception(msg)
    //     raise self.printer.command_error(msg)
    // self.allVariables = allvars  //---??---save_variables
}
        
    
void SaveVariables::cmd_SAVE_VARIABLE(GCodeCommand& gcmd)
{
    // varname = gcmd.get('VARIABLE')
    // value = gcmd.get('VALUE')
    // try:
    //     value = ast.literal_eval(value)
    // except ValueError as e:
    //     raise gcmd.error("Unable to parse '%s' as a literal" % (value,))
    // newvars = dict(self.allVariables)
    // newvars[varname] = value
    // # Write file
    // varfile = configparser.ConfigParser()
    // varfile.add_section('Variables')
    // for name, val in sorted(newvars.items()):
    //     varfile.set('Variables', name, repr(val))
    // try:
    //     f = open(self.filename, "w")
    //     varfile.write(f)
    //     f.close()
    // except:
    //     msg = "Unable to save variable"
    //     logging.exception(msg)
    //     raise gcmd.error(msg)
    // gcmd.respond_info("Variable Saved")
    // self.load_variables() ////---??---save_variables
}
        
void SaveVariables::get_status(double eventtime)
{
    // return {'variables': self.allVariables} //---??---save_variables
}
        
