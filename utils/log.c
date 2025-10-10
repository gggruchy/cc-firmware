
#include "common.h"
#include "utils.h"
#include "hl_ts_queue.h"
#include "log.h"
#include "hl_tpool.h"
#include "hl_common.h"

#define LOG_FILE_SIZE 8 * 1024 * 1024
#define LOG_BUF_SIZE 64 * 1024
#define LOG_LINE_SIZE 1 * 1024

static const char *log_level_identifier[LOG_LEVEL_NUM] = {
    "",
    "Assert",
    "Error",
    "Warn",
    "Info",
    "Debug",
};

typedef struct loger_tag
{
    hl_ts_queue_t queue[2];
    hl_ts_queue_t act_queue;
    hl_ts_queue_t backup_queue;
    hl_ts_queue_t full_queue;

    FILE *log_fp;
    pthread_t thread;
    pthread_attr_t thread_attr;
    pthread_mutex_t lock;

} loger_t;

static loger_t loger;

static void log_server(hl_tpool_thread_t thread, void *args);

static void log_swap_buf(void)
{
    pthread_mutex_lock(&loger.lock);
    // 交换缓冲区
    if (loger.act_queue == loger.queue[0])
    {
        loger.backup_queue = loger.queue[0];
        loger.act_queue = loger.queue[1];
    }
    else
    {
        loger.backup_queue = loger.queue[1];
        loger.act_queue = loger.queue[0];
    }
    pthread_mutex_unlock(&loger.lock);
}

static void log_over_handle(void)
{
    char link_path[PATH_MAX_LEN];
    // 解除原来软链接
    fclose(loger.log_fp);
    readlink(LOG_FILE_PATH, link_path, PATH_MAX_LEN - 1);
    unlink(LOG_FILE_PATH);
    if (strcmp(link_path, LOG_FILE_PATH "1") == 0)
    {
        symlink(LOG_FILE_PATH "2", LOG_FILE_PATH);
    }
    else if (strcmp(link_path, LOG_FILE_PATH "2") == 0)
    {
        symlink(LOG_FILE_PATH "1", LOG_FILE_PATH);
    }
    loger.log_fp = fopen(LOG_FILE_PATH, "wb+");
}

static void log_sync_to_file(void)
{
    char buf[LOG_BUF_SIZE];
    long seek = ftell(loger.log_fp);
    long len;
    long strip;
    while (hl_ts_queue_is_empty(loger.backup_queue) == 0)
    {
        len = hl_ts_queue_dequeue_try(loger.backup_queue, buf, LOG_BUF_SIZE);
        if (seek + len >= LOG_FILE_SIZE)
        {
            log_over_handle();
            strip = LOG_FILE_SIZE - seek;
            fwrite(buf, 1, strip, loger.log_fp);
            fseek(loger.log_fp, 0, SEEK_SET);
            len -= strip;
            if (len)
                fwrite(buf, 1, len, loger.log_fp);
        }
        else
        {
            fwrite(buf, 1, len, loger.log_fp);
        }
    }
    fflush(loger.log_fp);
    fsync(fileno(loger.log_fp));
}

void log_init(void)
{

    hl_ts_queue_create(&loger.queue[0], sizeof(char), LOG_BUF_SIZE);
    hl_ts_queue_create(&loger.queue[1], sizeof(char), LOG_BUF_SIZE);
    loger.act_queue = loger.queue[0];
    loger.backup_queue = loger.queue[1];
    loger.full_queue = NULL;
    if (access(LOG_FILE_PATH, F_OK) != 0)
    {
        // 打开日志失败说明不存在日志,此时新建轮换日志并建立软链接
        if (loger.log_fp == NULL)
        {
            loger.log_fp = fopen(LOG_FILE_PATH "1", "wb+");
            fclose(loger.log_fp);
            loger.log_fp = fopen(LOG_FILE_PATH "2", "wb+");
            fclose(loger.log_fp);
            symlink(LOG_FILE_PATH "1", LOG_FILE_PATH);
            loger.log_fp = fopen(LOG_FILE_PATH, "ab+");
        }
    }
    else
    {
        loger.log_fp = fopen(LOG_FILE_PATH, "ab+");
    }

    hl_tpool_thread_t log_thread;
    hl_tpool_create_thread(&log_thread, log_server, NULL, 0, 0, 0, 0);
}

void log_async_output(char *module, int level, char *file_name, int line_num, const char *fmt, ...)
{
    char line[LOG_LINE_SIZE];
    uint64_t tick = utils_get_current_tick();
    va_list ap;
    int len;
    int line_len;
#if LOG_OUTPUT_FILE_LINE
    snprintf(line, LOG_LINE_SIZE, "[%s][%s][%s][%d][%llu]:", module, log_level_identifier[level], file_name, line_num, tick);
#else
    snprintf(line, LOG_LINE_SIZE, "[%s][%d][%s][%llu]:", module, line_num, log_level_identifier[level], tick);
#endif
    line_len = strlen(line);
    va_start(ap, fmt);
    vsnprintf(line + line_len, LOG_LINE_SIZE - line_len, fmt, ap);
    va_end(ap);  
    line_len = strlen(line);
    if(line_len == LOG_LINE_SIZE - 1)
        line[LOG_LINE_SIZE - 2] = '\n';
    printf("%s", line);
    len = hl_ts_queue_enqueue(loger.act_queue, line, line_len);
    if (len != line_len) // 说明缓冲区满了
    {
        if (loger.full_queue)
        {
            printf("double buf full %d %d %d\n", len, line_len, hl_ts_queue_get_available_enqueue_length(loger.act_queue));
        }
        loger.full_queue = loger.act_queue;
        log_swap_buf();
        hl_ts_queue_enqueue(loger.act_queue, line + len, line_len - len);
    }
}
void aws_log_async_output(char *log_level_str, char *file_name, int line_num, const char *fmt, ...)
{
    char line[LOG_LINE_SIZE];
    uint64_t tick = utils_get_current_tick();
    va_list ap;
    int len;
    int line_len;
    if (strrchr(file_name, '/') && strlen(strrchr(file_name, '/') + 1) > 0)
        snprintf(line, LOG_LINE_SIZE, "[%s][%s][%d][%llu]:", log_level_str, strrchr(file_name, '/') + 1, line_num, tick);
    else
        snprintf(line, LOG_LINE_SIZE, "[%s][%s][%d][%llu]:", log_level_str, file_name, line_num, tick);
    line_len = strlen(line);
    va_start(ap, fmt);
    vsnprintf(line + line_len, LOG_LINE_SIZE - line_len - 1, fmt, ap);
    va_end(ap);
    strcat(line, "\n");
    // printf(line);
    line_len = strlen(line);
    len = hl_ts_queue_enqueue(loger.act_queue, line, line_len);
    if (len != line_len) // 说明缓冲区满了
    {
        if (loger.full_queue)
        {
            printf("double buf full %d %d %d\n", len, line_len, hl_ts_queue_get_available_enqueue_length(loger.act_queue));
        }
        loger.full_queue = loger.act_queue;
        log_swap_buf();
        hl_ts_queue_enqueue(loger.act_queue, line + len, line_len - len);
    }
}

static void log_server(hl_tpool_thread_t thread, void *args)
{
    while (1)
    {
        if (loger.full_queue)
        {
            log_sync_to_file();
            loger.full_queue = NULL;
        }

        if (hl_ts_queue_is_empty(loger.act_queue) == 0)
        {
            log_swap_buf();
            log_sync_to_file();
        }
        usleep(50000);
    }
}

void log_export_to_path(const char *path)
{
    char dst_name[512];
    fflush(loger.log_fp);
    fsync(fileno(loger.log_fp));
    snprintf(dst_name, sizeof(dst_name), "%s/log_1", path);
    utils_copy_file(dst_name, LOG_FILE_PATH "1");
    snprintf(dst_name, sizeof(dst_name), "%s/log_2", path);
    utils_copy_file(dst_name, LOG_FILE_PATH "2");
}