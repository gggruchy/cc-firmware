#include "my_string.h"

int startswith(std::string s, std::string sub){
        return s.find(sub)== 0 ? 1 : 0;
}

int endswith(std::string s, std::string sub)
{
    return s.rfind(sub) == (s.length() - sub.length()) ? 1 : 0;
}

// void utils_string_toupper(const char *in, char *out, int outsize)
// {
//     int i = 0;
//     int len = strlen(in);
//     len = len > outsize ? outsize : len;
//     for (i = 0; i < len; i++)
//         out[i] = toupper(in[i]);
//     out[i] = '\0';
// }

std::vector<std::string> split(const std::string &str, const std::string &delim) {
	std::vector<std::string> res;
	if("" == str) return res;

	//先将要切割的字符串从string类型转换为char*类型     //内存泄漏
    // char * strs = new char[str.length() + 1] ;       //内存泄漏//不要忘了       //----------new---??-----
    // cbd_new_mem("------------------------------------------------new_mem test:char[]",0);
	char strs[(str.length() + 1)];    
	strcpy(strs, str.c_str()); 
 
	char d[(delim.length() + 1)]; 
	strcpy(d, delim.c_str());
 
	char *p = strtok(strs, d);
	while(p) {
		std::string s = p; //分割得到的字符串转换为string类型
		res.push_back(s); //存入结果数组
		p = strtok(NULL, d);
	}
	return res;
}

std::vector<std::string> regex_split(const std::string &input, const std::regex &regex)
{
    std::vector<std::string> out;
    const std::sregex_token_iterator end;
    std::sregex_token_iterator iter1(input.begin(), input.end(), regex, 0);
    std::sregex_token_iterator iter2(input.begin(), input.end(), regex, -1);
    iter2++;
    while (iter1 != end)
    {
        out.push_back(iter1->str());
        iter1++;
        if (iter2 != end)
        {
            out.push_back(iter2->str());
            iter2++;
        }
    }
    return out;
}

std::string do_strip(const std::string &str, int striptype, const std::string &chars)
{
    std::string::size_type strlen = str.size();
    std::string::size_type charslen = chars.size();
    std::string::size_type i, j;

    // 默认情况下，去除空白符
    if (0 == charslen)
    {
        i = 0;
        // 去掉左边空白字符
        if (striptype != RIGHTSTRIP)
        {
            while (i < strlen && ::isspace(str[i]))
            {
                i++;
            }
        }
        j = strlen;
        // 去掉右边空白字符
        if (striptype != LEFTSTRIP)
        {
            j--;
            while (j >= i && ::isspace(str[j]))
            {
                j--;
            }
            j++;
        }
    }
    else
    {
        // 把删除序列转为c字符串
        const char *sep = chars.c_str();
        i = 0;
        if (striptype != RIGHTSTRIP)
        {
            // memchr函数：从sep指向的内存区域的前charslen个字节查找str[i]
            while (i < strlen && memchr(sep, str[i], charslen))
            {
                i++;
            }
        }
        j = strlen;
        if (striptype != LEFTSTRIP)
        {
            j--;
            while (j >= i && memchr(sep, str[j], charslen))
            {
                j--;
            }
            j++;
        }
        // 如果无需要删除的字符
        if (0 == i && j == strlen)
        {
            return str;
        }
        else
        {
            return str.substr(i, j - i);
        }
    }
}

std::string strip(const std::string &str, const std::string &chars)
{
    return do_strip(str, BOTHSTRIP, chars);
}

std::string lstrip(const std::string &str, const std::string &chars)
{
    return do_strip(str, LEFTSTRIP, chars);
}

std::string rstrip(const std::string &str, const std::string &chars)
{
    return do_strip(str, RIGHTSTRIP, chars);
}
