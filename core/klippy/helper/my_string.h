#ifndef MY_STRING_H
#define MY_DTRING_H

#include "string.h"
#include "vector"
#include <string>
#include <regex>
#include <iostream>

#define LEFTSTRIP 0
#define RIGHTSTRIP 1
#define BOTHSTRIP 2

int startswith(std::string s, std::string sub);
int endswith(std::string s, std::string sub);
// void utils_string_toupper(const char *in, char *out, int outsize);
std::vector<std::string> split(const std::string &str, const std::string &delim);
std::vector<std::string> regex_split(const std::string& input, const std::regex& regex);
std::string do_strip(const std::string &str, int striptype, const std::string&chars);
std::string strip( const std::string & str, const std::string & chars=" " );
std::string lstrip( const std::string & str, const std::string & chars=" " );
std::string rstrip( const std::string & str, const std::string & chars=" " );

#endif
