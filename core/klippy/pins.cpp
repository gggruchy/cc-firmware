#include "pins.h"
#include "my_string.h"



PinResolver::PinResolver(bool validate_aliases)
{
    m_validate_aliases = validate_aliases;
}

PinResolver::~PinResolver()
{
}

void PinResolver::reserve_pin(std::string pin, std::string reserve_name)
{
    if(reserved.find(pin) != reserved.end() && reserved[pin] != reserve_name)
    {
        std::cout << "Pin " << pin << "reserved for " << reserved[pin] << "- can't reserve for " << reserve_name << std::endl; 
    }
    reserved[pin] = reserve_name;
}

void PinResolver::alias_pin(std::string alias, std::string pin)
{
    if(aliases.find(alias) != aliases.end() && aliases[alias] != pin)
    {
        std::cout << "Alias " << alias << "mapped to " << aliases[alias] << "- can't alias to " << pin << std::endl;
    }
    if(pin.find("^") != pin.npos || pin.find("!") != pin.npos || pin.find(":") != pin.npos || pin.find(" ") != pin.npos)
    {
        std::cout << "invalid pin alias" << pin << std::endl;
    }
    if(aliases.find(pin) != aliases.end())
    {
        pin = aliases[pin];
    }
    aliases[alias] = pin;
    std::map<std::string, std::string>::iterator iter;
    for(iter = aliases.begin(); iter != aliases.end(); ++iter)
    {
        if(iter->second == alias)
        {
            aliases[iter->first] = pin;
        }
    }
}
std::string PinResolver::update_command(std::string cmd)
{
    // re_pin = re.compile(r'(?P<prefix>[ _]pin=)(?P<name>[^ ]*)')
    std::regex re_pin ("?P<prefix>[ _]pin=)(?P<name>[^ ]*");
    //---??---

}

PrinterPins::PrinterPins()
{
}

PrinterPins::~PrinterPins()
{
}

pinParams PrinterPins::parse_pin(std::string pin_desc, bool can_invert, bool can_pullup)
{
    std::string desc = strip(pin_desc);
    std::string chip_name, pin;
    int pullup = 0;
    int invert = 0;
    if(can_pullup && (desc[0] == '^' || desc[0] == '~'))
    {
        pullup = 1;
        if(desc[0] == '~')
            pullup = -1;
        desc = strip(desc.substr(1));
    }
    if(can_invert && desc[0] == '!')
    {
        invert = 1;
        desc = strip(desc.substr(1));
    }
    pin = desc;


    if(desc.find(":") == std::string::npos)
    {
        chip_name = "mcu";
        pin = desc;
    }
    else
    {
        std::vector<std::string> desc_parts = split(desc, ":");
        chip_name = desc_parts[0];
        pin = desc_parts[1];
    }
    if(chips.find(chip_name) == chips.end())
    {
        std::cout << "unknown pin chip name " << chip_name << std::endl;
    }
    // if [c for c in '^~!: ' if c in pin]:
    //     format = ""
    //     if can_pullup:
    //         format += "[^~] "
    //     if can_invert:
    //         format += "[!] "
    //     raise error("Invalid pin description '%s'\n"
    //                 "Format is: %s[chip_name:] pin_name" % (
    //                     pin_desc, format))
    
    pinParams pin_params;
    pin_params.chip = chips[chip_name];
    pin_params.chip_name = chip_name;
    pin_params.pin = pin;           //pin_desc;
    pin_params.invert = invert;
    pin_params.pullup = pullup;
    pin_params.pclass = nullptr;
    return pin_params;
}

pinParams* PrinterPins::lookup_pin(std::string pin_desc, bool can_invert, bool can_pullup, std::string share_type)
{
    pinParams pin_params = parse_pin(pin_desc, can_invert, can_pullup);
    std::string pin = pin_params.pin;
    std::string share_name = pin_params.chip_name + ":" + pin;
    auto iter = active_pins.find(share_name);
    if(iter != active_pins.end())
    {
        pinParams* share_params = &(iter->second);
        if(allow_multi_use_pins.find(share_name) != allow_multi_use_pins.end())     //多用功能引脚
        {

        }
        else if (share_type == "" || share_type != share_params->share_type)
        {
            std::cout << "pin " << pin_desc << " used multiple times in config" << std::endl;
        }
        else if(pin_params.invert != share_params->invert || pin_params.pullup != share_params->pullup)
        {
            std::cout << "shared pin " << pin_desc << " must have same polarity" << std::endl;
        }
        return share_params; 
    }
    pin_params.share_type = share_type;
    active_pins[share_name] = pin_params;
    return &active_pins[share_name];
}

void* PrinterPins:: setup_pin(std::string pin_type, std::string pin_desc)
{
    bool can_invert = false;
    bool can_pullup = false;
    if(pin_type == "endstop" || pin_type == "digital_out" || pin_type == "pwm")
        can_invert = true;
    if(pin_type == "endstop")
        can_pullup = true;
    pinParams *pin_params = this->lookup_pin(pin_desc, can_invert, can_pullup);
    return pin_params->chip->setup_pin(pin_type, pin_params);
}

PinResolver* PrinterPins::get_pin_resolver(std::string chip_name)
{
    if(pin_resolvers.find(chip_name) == pin_resolvers.end())
    {
        std::cout << "Unknown chip name " << chip_name << std::endl;
    }
    return pin_resolvers.find(chip_name)->second;
    
}

void PrinterPins::register_chip(std::string chip_name, McuChip* chip)
{
    chip_name = strip(chip_name);
    if(chips.find(chip_name) != chips.end())
    {
        std::cout << "Duplicate chip name " << chip_name << std::endl;
    }
    chips[chip_name] = chip;
    pin_resolvers[chip_name] = new PinResolver();
}

void PrinterPins::allow_multi_use_pin(std::string pin_desc)     //duplicate_pin_override
{
    pinParams pin_params = this->parse_pin(pin_desc);
    std::string share_name = pin_params.chip_name + ":" + pin_params.pin;
    allow_multi_use_pins[share_name] = true;
}

void PrinterPins::reset_pin_sharing(pinParams *pin_params)
{

}

