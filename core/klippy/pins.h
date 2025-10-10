#ifndef PINS_H
#define PINS_H
#include <iostream>
#include <string>
#include <string.h>
#include <map>
#include <regex>
#include "mcu.h"

struct pinParams
{
    McuChip *chip;
    std::string chip_name;
    std::string pin;
    int invert;
    int pullup;
    std::string share_type;
    void *pclass;
};

class PinResolver
{
private:
    bool m_validate_aliases;
    std::map<std::string, std::string> reserved;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::string> active_pins;

public:
    PinResolver(bool validate_aliases = true);
    ~PinResolver();
    void reserve_pin(std::string pin, std::string reserve_name);
    void alias_pin(std::string alias, std::string pin);
    std::string update_command(std::string cmd);
};

class PrinterPins
{
private:
    std::map<std::string, McuChip *> chips;
    std::map<std::string, pinParams> active_pins;
    std::map<std::string, PinResolver *> pin_resolvers;
    std::map<std::string, bool> allow_multi_use_pins;

public:
    PrinterPins();
    ~PrinterPins();
    pinParams parse_pin(std::string pin_desc, bool can_invert = false, bool can_pullup = false);
    pinParams *lookup_pin(std::string pin_desc, bool can_invert = false, bool can_pullup = false, std::string share_type = "");
    // 通过返回的pinParams new一个mcu
    void *setup_pin(std::string pin_type, std::string pin_desc);
    void reset_pin_sharing(pinParams *pin_params);
    PinResolver *get_pin_resolver(std::string chip_name);
    void register_chip(std::string chip_name, McuChip *chip);
    void allow_multi_use_pin(std::string pin_desc);
};

#endif