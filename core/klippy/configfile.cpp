#include "configfile.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "my_string.h"
// #include "printer_para.h"
#include "debug.h"
#include <unistd.h>
#define LOG_TAG "configfile"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

using std::cout;
using std::endl;
using std::fstream;
using std::ios;
using std::istringstream;
using std::stringstream;
using std::to_string;
#define OPTION_NAME_MAX_LEN 2048
#define OPTION_VALUE_MAX_LEN 8192
size_t find_first_bigger_then(std::string &src, char delim)
{
	size_t len = src.size();
	int pos = 0;
	for (; pos < len; ++pos)
	{
		if (src[pos] > delim)
		{
			return pos;
		}
	}
	return len;
}

size_t find_last_bigger_then(std::string &src, char delim)
{
	size_t len = src.size();
	int pos = len;
	--pos;
	for (; pos >= 0; --pos)
	{
		if (src[pos] > delim)
		{
			return pos;
		}
	}
	return 0;
}

ConfigParser::ConfigParser(string path, bool force) // 为分析断电等文件参数
{
	_ini = path;
	para_state = 0;
	_section_list_head = nullptr;
	if (access(path.c_str(), F_OK) != 0)
	{
	}
}

ConfigParser::ConfigParser(string path)
{
	_ini = path;
	para_state = 0;
	if (strcmp(path.c_str(), CONFIG_PATH) == 0)
	{
		if (access(path.c_str(), F_OK) != 0)
		{
			char cmd[1024];
			snprintf(cmd, sizeof(cmd), "cp /app/resources/configs/printer.cfg %s", path.c_str());
			int simple_retry = 0;
			int ret = 0;
			do
			{
				usleep(100000);
				ret = system(cmd);
				system("sync");
			} while (ret != 0 && simple_retry++ < 10);
		}
	}
	else if (strcmp(path.c_str(), USER_CONFIG_PATH) == 0)
	{
		if (access(path.c_str(), F_OK) != 0)
		{
			char cmd[1024];
			snprintf(cmd, sizeof(cmd), "cp /app/resources/configs/user_printer.cfg %s", path.c_str());
			int simple_retry = 0;
			int ret = 0;
			do
			{
				usleep(100000);
				ret = system(cmd);
				system("sync");
			} while (ret != 0 && simple_retry++ < 10);
		}
	}
	else if (strcmp(path.c_str(), SYSCONF_PATH) == 0)
	{
		if (access(path.c_str(), F_OK) != 0)
		{
			char cmd[1024];
			snprintf(cmd, sizeof(cmd), "cp /app/resources/configs/sysconf.cfg %s", path.c_str());
			int simple_retry = 0;
			int ret = 0;
			do
			{
				usleep(100000);
				ret = system(cmd);
				system("sync");
			} while (ret != 0 && simple_retry++ < 10);
		}
	}
	else if (strcmp(path.c_str(), UNMODIFIABLE_CFG_PATH) == 0)
	{
		if (access(path.c_str(), F_OK) != 0)
		{
			char cmd[1024];
			snprintf(cmd, sizeof(cmd), "cp /app/resources/configs/unmodifiable.cfg %s", path.c_str());
			int simple_retry = 0;
			int ret = 0;
			do
			{
				usleep(100000);
				ret = system(cmd);
				system("sync");
			} while (ret != 0 && simple_retry++ < 10);
		}
	}
	ReadIni();
}

ConfigParser::~ConfigParser()
{
	// for (size_t index = 0; index < section_count; index++)
	// {
	// 	Section* node = _section_list_head;
	// 	_section_list_head = _section_list_head->next;

	// 	Option* op_head = node->options;
	// 	size_t j = 0;
	// 	while (op_head != nullptr)
	// 	{
	// 		Option* child = op_head;
	// 		op_head = op_head->next;
	// 		delete child;
	// 	}
	// 	delete node;
	// }
}

Section *ConfigParser::InitSectionList()
{
	_section_list_head = new Section();
	_section_list_head->next = nullptr;
	// _section_list_head = nullptr;

	return _section_list_head;
}

bool ConfigParser::ReadIni()
{
	string str_line = "";
	string notes = "";
	size_t left_pos;
	try
	{
		InitSectionList();
		fstream config_name(_ini, ios::in | ios::out);
		if (!config_name.is_open())
		{
			LOG_E("Cannot open this file:%s \n", _ini.c_str());
			para_state = PARA_STATE_OPEN_ERR;
			return false;
		}
		int row = 0;
		string section_name = "";
		bool flag = false;
		while (getline(config_name, str_line))
		{
			row++;
			str_line = TrimTrialLineBreak(str_line); // 删除行尾注释
			bool result = IsNotes(str_line);
			if (result)
			{
				notes += str_line + line_break;
				continue;
			}
			str_line = TrimLeadTrailSpecStr(str_line); // 删除行空格
			str_line = TrimTrailSpecStr(str_line, "\n");
			str_line = TrimTrailSpecStr(str_line, "\r");
			result = IsSection(str_line);
			if (result)
			{
				str_line = str_line.erase(0, str_line.find_first_not_of("["));
				str_line = str_line.erase(str_line.find_last_not_of("]") + 1);
				flag = IsExistSection(str_line);
				if (!flag)
					AddSection(str_line, notes);
				section_name = str_line;
				notes = "";
				continue;
			}
			else
			{
				if (section_name.empty())
					continue;
				left_pos = str_line.find_first_of(":");
				size_t str_len = str_line.length();
				if (str_len == 1)
					continue;
				if (left_pos == 0 || left_pos > str_len)
				{
					LOG_E("Ini file format error on %d line str_line:%s str_len:%d \n", row, str_line.c_str(), str_len);
					para_state = PARA_STATE_PARA_ERR;
					return false;
				}
				char c_option_name[OPTION_NAME_MAX_LEN] = {0x00};
				char c_option_value[OPTION_VALUE_MAX_LEN] = {0x00};
				str_line.copy(c_option_name, left_pos, 0);
				str_line.copy(c_option_value, str_len - left_pos, left_pos + 1);
				string option_name(c_option_name);
				string option_value(c_option_value);
				option_name = TrimLeadTrailSpecStr(option_name);
				option_value = TrimLeadTrailSpecStr(option_value);
				flag = IsExistOption(section_name, option_name);
				if (!flag)
					AddOption(section_name, option_name, option_value, notes);
				notes = "";
			}
		}
		config_name.close();
	}
	catch (...)
	{
		LOG_E("-ERR----ReadIni  file:%s \n", _ini.c_str());
		para_state = PARA_STATE_READ_ERR;
		return false;
	}
	return true;
}

bool ConfigParser::IsNotes(string line) // 注释行
{
	string data = line.erase(0, find_first_bigger_then(line, ' ')); // 删除行首空格
	if (data.find_first_of(";") == 0 || data.find_first_of("#") == 0 || data.empty())
	{
		return true;
	}
	return false;
}

bool ConfigParser::IsSection(string line)
{
	string data = line.erase(find_last_bigger_then(line, ' ') + 1); // 删除行尾空格
	data = data.erase(0, find_first_bigger_then(data, ' '));		// 删除行首空格
	if (data.find_first_of("[") == 0 && data.find_last_of("]") == (data.length() - 1))
	{
		return true;
	}
	return false;
}

void ConfigParser::AddSection(string section, string note)
{
	Section *node = nullptr;
	// Add () to init node.
	node = new Section();
	node->comment = note;
	node->name = section;
	node->next = nullptr;
	if (_section_list_head == nullptr)
	{
		_section_list_head = node;
	}
	else
	{
		// add new node to the end of the list.
		Section *curr_pos = _section_list_head;
		while (curr_pos->next != nullptr)
		{
			curr_pos = curr_pos->next;
		}
		curr_pos->next = node;
	}

	section_count++;
}

void ConfigParser::AddOption(string section, string option, string value, string note)
{
	Option *elem = nullptr;
	if (_section_list_head == nullptr)
	{
		LOG_E("No any sections. \n");
		return;
	}
	Section *node = _section_list_head;
	while (node->next != nullptr)
	{
		if (node->name.compare(section) == 0)
			break;
		node = node->next;
	}

	elem = new Option();
	elem->name = option;
	elem->comment = note;
	elem->value = value;
	elem->next = nullptr;
	if (node->options == nullptr)
	{
		node->options = new Option();
		node->options = elem;
	}
	else
	{
		Option *option_pos = node->options;
		while (option_pos->next != nullptr)
		{
			while (option_pos->next != nullptr)
				option_pos = option_pos->next;
		}
		option_pos->next = elem;
	}
}

void ConfigParser::ViewSections()
{
	if (_section_list_head != nullptr)
	{
		Section *section = _section_list_head;
		while (section != nullptr)
		{
			GAM_DEBUG_printf("--[%s]--\n", section->name.c_str());
			Option *option_pos = section->options;
			while (option_pos != nullptr)
			{
				GAM_DEBUG_printf("%s = %s \n", option_pos->name.c_str(), option_pos->value.c_str());
				option_pos = option_pos->next;
			}
			section = section->next;
		}
	}
}

string ConfigParser::TrimLeadTrailSpecStr(string src, char keyword)
{
	// src = TrimLeadSpecStr(src, keyword);
	// src = TrimTrailSpecStr(src, keyword);
	src = src.erase(find_last_bigger_then(src, keyword) + 1); // 删除行尾空格
	src = src.erase(0, find_first_bigger_then(src, keyword)); // 删除行首空格
	return src;
}

string ConfigParser::TrimLeadSpecStr(string src, string keyword)
{
	size_t pos = src.find_first_not_of(keyword);
	size_t len = src.size();
	if (0 <= pos && pos <= len)
		src = src.erase(0, pos);
	return src;
}

string ConfigParser::TrimTrailSpecStr(string src, string keyword)
{
	size_t pos = src.find_last_not_of(keyword);
	size_t len = src.size();
	if (0 <= pos && pos <= len)
		src = src.erase(pos + 1);
	return src;
}

Section *ConfigParser::GetSection(const string &root)
{
	if (_section_list_head == nullptr)
	{
		LOG_E("%s file is empty.\n", _ini.c_str());
		return nullptr;
	}
	bool flag = false;
	auto node = _section_list_head;
	while (node != nullptr)
	{
		if (strcmp(node->name.c_str(), root.c_str()) != 0)
		{
			node = node->next;
			continue;
		}
		break;
	}
	return node;
}

string ConfigParser::GetString(const string &root, const string &key, const string &def)
{
	if (_section_list_head == nullptr)
	{
		LOG_E("%s file is empty.\n", _ini.c_str());
		return def;
	}

	bool flag = false;
	auto node = _section_list_head;
	string value = "";
	while (node != nullptr)
	{
		if (strcmp(node->name.c_str(), root.c_str()) != 0)
		{
			node = node->next;
			continue;
		}

		Option *opt = node->options;
		while (opt != nullptr)
		{
			if (strcmp(opt->name.c_str(), key.c_str()) == 0)
			{
				value = opt->value;
				flag = true;
				break;
			}
			opt = opt->next;
		}
		if (flag)
			break;
		node = node->next;
	}
	if (flag)
	{
		// GAM_DEBUG_printf("---%s--%s--%s--%s-----\n", root.c_str(), key.c_str(), def.c_str(),value.c_str());
		return value;
	}
	// GAM_DEBUG_printf("---%s--%s--%s------\n", root.c_str(), key.c_str(), def.c_str());
	return def;
}

double ConfigParser::GetDouble(const string &root, const string &key, double def, double minval, double maxval, double above, double below, bool note_valid)
{
	string value = GetString(root, key);
	if (value.empty())
	{
		// GAM_DEBUG_printf("---%s--%s--%f------\n", root.c_str(), key.c_str(), def);
		return def;
	}

	double res;
	try
	{
		istringstream is(value);
		is >> res;
	}
	catch (...)
	{
		res = def;
		// GAM_DEBUG_printf("---%s--%s--%f------\n", root.c_str(), key.c_str(), res);
	}

	if (minval != DBL_MIN && res < minval)
	{
		std::cout << "error on " << root << ": " << key << " must have minimum of " << minval << std::endl;
	}
	if (maxval != DBL_MAX && res > maxval)
	{
		std::cout << "error on " << root << ": " << key << " must have maximum of " << maxval << std::endl;
	}
	if (above != DBL_MIN && res <= above)
	{
		std::cout << "error on " << root << ": " << key << " must be above " << above << std::endl;
	}
	if (below != DBL_MAX && res >= below)
	{
		std::cout << "error on " << root << ": " << key << " must be below " << below << std::endl;
	}

	return res;
}

std::vector<double> ConfigParser::GetDoubleVector(const string& root, const string& key, double def, double minval, double maxval, double above, double below, bool note_valid)
{
	string value = GetString(root, key);
    std::vector<double> values;
    strip(value);
    std::istringstream iss(value); 
    std::string token_str;
    while (getline(iss, token_str, ','))
    {	double tmp = atof(token_str.c_str());
		if (minval != DBL_MIN && tmp < minval)
		{ 	//TODO: exception 
    		LOG_E("cfg value(%f) < minimum value(%f)\n",  tmp, DBL_MIN);
		}
		if (maxval != DBL_MAX  && tmp > maxval)
		{ 
    		LOG_E("cfg value(%f) > maximum value(%f)\n",  tmp, DBL_MAX); 
		}
        values.push_back(tmp);
    } 
    return values;
}

int ConfigParser::GetInt(const string &root, const string &key, int def, int minval, int maxval)
{
	string value = GetString(root, key);
	if (value.empty())
	{
		// GAM_DEBUG_printf("---%s--%s--%d-----\n", root.c_str(), key.c_str(), def);
		return def;
	}

	int res;
	try
	{
		istringstream is(value);
		is >> res;
	}
	catch (...)
	{
		res = def;
		// GAM_DEBUG_printf("---%s--%s--%d------\n", root.c_str(), key.c_str(), res);
	}
	if (minval != INT32_MIN && res < minval)
	{
		std::cout << "error on " << root << ": " << key << " must have minimum of " << minval << std::endl;
		res = minval;
	}
	if (maxval != INT32_MAX && res > maxval)
	{
		std::cout << "error on " << root << ": " << key << " must have maximum of " << maxval << std::endl;
		res = maxval;
	}
	return res;
}

bool ConfigParser::GetBool(const string &root, const string &key, bool def)
{
	string value = GetString(root, key);
	if (value.empty())
		return def;
	if (value == "1")
		return true;
	if (strcmp(value.c_str(), "true") == 0)
		return true;
	if (strcmp(value.c_str(), "TRUE") == 0)
		return true;
	if (strcmp(value.c_str(), "True") == 0)
		return true;
	return false;
}

bool ConfigParser::IsExistSection(string root)
{
	if (_section_list_head == nullptr)
		return false;

	bool flag = false;
	auto node = _section_list_head;
	while (node != nullptr)
	{
		if (strcmp(node->name.c_str(), root.c_str()) == 0)
		{
			flag = true;
			break;
		}
		node = node->next;
	}
	return flag;
}

bool ConfigParser::IsExistOption(string root, string option)
{
	if (_section_list_head == nullptr)
		return false;

	bool flag = false;
	auto node = _section_list_head;
	while (node != nullptr)
	{
		if (strcmp(node->name.c_str(), root.c_str()) == 0)
		{
			auto opt = node->options;
			while (opt != nullptr)
			{
				if (strcmp(opt->name.c_str(), option.c_str()) == 0)
				{
					flag = true;
					break;
				}
				opt = opt->next;
			}
			break;
		}
		node = node->next;
	}
	return flag;
}

void ConfigParser::SetValue(const string &root, const string &key, const string &value)
{
	if (_section_list_head == nullptr)
	{
		AddSection(root, "");
		AddOption(root, key, value, "");
		return;
	}
	else
	{
		bool flag = false;
		flag = IsExistSection(root);
		if (!flag)
		{
			AddSection(root, "");
			AddOption(root, key, value, "");
			return;
		}
		flag = IsExistOption(root, key);
		if (!flag)
		{
			AddOption(root, key, value, "");
			return;
		}
		auto node = _section_list_head;
		while (node != nullptr)
		{
			if (strcmp(node->name.c_str(), root.c_str()) == 0)
			{
				auto opt = node->options;
				while (opt != nullptr)
				{
					if (strcmp(opt->name.c_str(), key.c_str()) == 0)
					{
						opt->value = value;
						break;
					}
					opt = opt->next;
				}
				break;
			}
			node = node->next;
		}
	}
}

void ConfigParser::SetInt(const string &root, const string &key, int value)
{
	string tmp = to_string(value);
	SetValue(root, key, tmp);
}

void ConfigParser::SetDouble(const string &root, const string &key, double value)
{
	string tmp = to_string(value);
	SetValue(root, key, tmp);
}

void ConfigParser::SetBool(const string &root, const string &key, bool value)
{
	if (value)
		SetValue(root, key, "1");
	else
		SetValue(root, key, "0");
}

void ConfigParser::Setuservalue(Section *section)
{
	auto node = _section_list_head;
	bool cover_flag = false;
	while (node != nullptr)
	{
		// std::cout << "set section->name：" << section->name << " ，node->name：" << node->name << std::endl;
		if (strcmp(section->name.c_str() ,node->name.c_str()) == 0)
		{
			auto user_opt = section->options;
			while (user_opt != nullptr)
			{
				// std::cout << "user_opt->name：" << user_opt->name << std::endl;
				auto opt = node->options;
				while (opt != nullptr)
				{
					if(strcmp(user_opt->name.c_str() ,opt->name.c_str()) == 0)
					{
						// std::cout << "set value " << opt->name << " : " << opt->value << std::endl;
						// std::cout << "set user value " << user_opt->name << " : " << user_opt->value << std::endl;
						opt->value = user_opt->value;
						break;
					}
					opt = opt->next;
				}
				user_opt = user_opt->next;
			}
			cover_flag = true;
			break;
		}
		node = node->next;
	}
	if (!cover_flag)
	{
		AddSection(section->name, section->comment);
		auto user_opt = section->options;
		while (user_opt != nullptr)
		{
			AddOption(section->name, user_opt->name, user_opt->value, user_opt->comment);
			user_opt = user_opt->next;
		}
	}
}

bool ConfigParser::WriteIni(string path)
{
	fstream fp(path, ios::trunc | ios::out);
	if (!fp.is_open())
	{
		LOG_E("Cannot open this file: %s \n", path.c_str());
		return false;
	}

	auto node = _section_list_head;
	while (node != nullptr)
	{
		string section_node = "[" + node->name + "]";
		if (!node->comment.empty())
			fp << node->comment;
		fp << section_node << endl;
		auto opt = node->options;
		while (opt != nullptr)
		{
			string opt_node = opt->name + " : " + opt->value;
			if (!opt->comment.empty())
				fp << opt->comment;
			fp << opt_node << endl;
			opt = opt->next;
		}
		node = node->next;
	}
	fp.flush();
	fp.sync();
	fp.close();
	system("sync");
	std::cout << "write to  " << path << "  finish " << std::endl;
	return true;
}

/**
 * @brief 写入指定节点的指定值到用户配置文件中
 * 
 * @param path 用户配置文件路径
 * @param root 节点名称
 * @param keys 指定值名称容器
 * @return  
 */
bool ConfigParser::WriteI_specified_Ini(string path, const string &root, const std::vector<string> &keys)
{
    fstream fp(path, ios::in | ios::out); // 以读写模式打开文件
	std::string file_content;
    if (fp.is_open())
    {
		// 读取文件内容到内存
		std::stringstream buffer;
		buffer << fp.rdbuf();
		file_content = buffer.str();
		fp.seekp(0, ios::beg);
		fp.clear();
    }
	else
	{
		// 创建新文件
        std::ofstream outfile(path);
		fp.close();
		// 重新以读写模式打开文件
        fp.open(path, ios::in | ios::out);
        if (!fp.is_open()) {
            std::cout << "Failed to reopen the newly created file." << std::endl;
            return false;
        }
	}


    auto node = _section_list_head;
    while (node != nullptr)
    {
        auto opt = node->options;
        if (strcmp(node->name.c_str(), root.c_str()) == 0)
        {
            string section_node = "[" + node->name + "]";
            size_t section_pos = file_content.find(section_node);
            if (section_pos != std::string::npos)
            {
                // 找到同名节，删除同名内容
                size_t section_end = file_content.find("[", section_pos + 1);
				std::cout << "section_pos: " << section_pos << " section_end: " << section_end << std::endl;
                if (section_end == std::string::npos)
                    section_end = file_content.size();
                file_content.erase(section_pos, section_end - section_pos);
            }

            if (!node->comment.empty())
                fp << node->comment;
            fp << section_node << endl;
            if (!keys.empty())
            {
                while (opt != nullptr)
                {
                    for (const auto &key : keys)
                    {
                        if (opt->name == key)
                        {
                            string opt_node = opt->name + " : " + opt->value;
                            if (!opt->comment.empty())
                                fp << opt->comment;
                            fp << opt_node << endl;
                            break;
                        }
                    }
                    opt = opt->next;
                }
            }
            else
            {
                while (opt != nullptr)
                {
                    string opt_node = opt->name + " : " + opt->value;
                    if (!opt->comment.empty())
                        fp << opt->comment;
                    fp << opt_node << endl;
                    opt = opt->next;
                }
            }
        }
        node = node->next;
    }

    // 将修改后的内容写回文件
	if (!file_content.empty())
		fp << file_content;
    fp.flush();
    fp.sync();
    fp.close();
    system("sync");
    std::cout << "write to  " << path << "  finish " << std::endl;
    return true;
}

string ConfigParser::TrimTrialLineBreak(string src) // 删除行尾注释
{
	size_t len = src.size();
	for (int pos = 0; pos < len; ++pos)
	{
		if (src[pos] == '\0' || src[pos] == '\r' || src[pos] == '\n' || src[pos] == ';' || src[pos] == '#')
		{
			src = src.erase(pos);
			break;
		}
	}
	return src;
}

std::vector<string> ConfigParser::get_prefix_sections(std::string prefix)
{
	std::vector<string> sections;
	if (_section_list_head != nullptr)
	{
		Section *section = _section_list_head;
		while (section != nullptr)
		{

			if (startswith(section->name, prefix))
				sections.push_back(section->name);
			section = section->next;
		}
	}
	return sections;
}

std::vector<string> ConfigParser::get_prefix_options(std::string section, std::string prefix)
{
	std::vector<string> prefix_options;
	Section *_section = GetSection(section);
	while (_section->options != nullptr)
	{
		if (startswith(_section->options->name, prefix))
		{
			prefix_options.push_back(_section->options->name);
		}
	}
	return prefix_options;
}

std::vector<string> ConfigParser::get_all_options(std::string section)
{
	std::vector<string> options;
	Section *_section = GetSection(section);
	if (_section != nullptr)
	{
		auto opt = _section->options;
		while (opt != nullptr)
		{
			options.push_back(opt->name);
			opt = opt->next;
		}
	}
	return options;
}

void ConfigParser::DeleteSection(string section)
{
	Section *node = _section_list_head;
	Section *last = node;
	while (node->name != section)
	{
		last = node;
		node = node->next;
	}
	if (node != nullptr)
	{
		last->next = node->next;
		node->next = nullptr;
		Option *op_head = node->options;
		while (op_head != nullptr)
		{
			Option *child = op_head;
			op_head = op_head->next;
			delete child;
		}
		delete node;
	}
}

// printer_para_t *cbd_printer_para;
