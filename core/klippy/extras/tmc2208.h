#ifndef TMC2208_H
#define TMC2208_H
#include "tmc2130.h"
#include <string>
#include <map>
// #include "tmc.h"

extern std::map<std::string, std::map<std::string, int>> Fields;
extern std::map<std::string, uint8_t> Registers;
extern std::vector<std::string> ReadRegisters;
extern std::vector<std::string> SignedFields;
extern std::map<std::string, std::function<std::string(int)>> FieldFormatters;

void init_Registers_2208();
void init_Fields_2208();
void init_FieldFormatters_2208();



#endif
