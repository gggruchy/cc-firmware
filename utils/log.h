#ifndef LOG_H
#define LOG_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "log.h"
    // #ifndef LOG_TAG
    //     #error must be specified LOG_TAG
    // #endif

    // #ifndef LOG_LEVEL
    //     #error must be specified LOG_LEVEL
    // #endif

    // // // 0.[A]：断言(Assert)
    // // // 1.[E]：错误(Error)
    // // // 2.[W]：警告(Warn)
    // // // 3.[I]：信息(Info)
    // // // 4.[D]：调试(Debug)

    //[模块名称][时间戳][日志等级]:
#define LOG_FILE_PATH "/board-resource/log"

#define LOG_OUTPUT_FILE_LINE 0

#define LOG_OFF 0
#define LOG_ASSERT 1
#define LOG_ERROR 2
#define LOG_WARN 3
#define LOG_INFO 4
#define LOG_DEBUG 5
#define LOG_LEVEL_NUM 6

    void log_init(void);
    void log_async_output(char *module, int level, char *file_name, int line_num, const char *fmt, ...);
    void log_export_to_path(const char *path);
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_OFF
#endif

#ifndef LOG_TAG
#define LOG_TAG ""
#endif

#if (LOG_LEVEL >= LOG_ASSERT)
#define LOG_A(...) log_async_output((char *)LOG_TAG, LOG_ASSERT, (char *)__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_A(...)
#endif

#if (LOG_LEVEL >= LOG_ERROR)
#define LOG_E(...) log_async_output((char *)LOG_TAG, LOG_ERROR, (char *)__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_E(...)
#endif

#if (LOG_LEVEL >= LOG_WARN)
#define LOG_W(...) log_async_output((char *)LOG_TAG, LOG_WARN, (char *)__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_W(...)
#endif

#if (LOG_LEVEL >= LOG_INFO)
#define LOG_I(...) log_async_output((char *)LOG_TAG, LOG_INFO, (char *)__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_I(...)
#endif

#if (LOG_LEVEL >= LOG_DEBUG)
#define LOG_D(...) log_async_output((char *)LOG_TAG, LOG_DEBUG, (char *)__FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_D(...)
#endif

#ifdef __cplusplus
}
#endif

#endif