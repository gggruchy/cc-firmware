#include "gcode_preview.h"
#include "extras/print_stats_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Base64.h"
#include "utils.h"
#include <regex.h>

#define LOG_TAG "gcode_preview"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define MAX_LINE_LENGTH 1024
typedef struct
{
    FILE *fp;
    int64_t reverse_read_pos;
    char line[MAX_LINE_LENGTH];
    int reverse;
} file_read_ctx_t;

static slice_type_t find_gcode_slicer(const char *file_path)
{
#define READ_LINE_NUM 30
    slice_type_t slicer = OTHERS_SLICER;
    FILE *fp = NULL;
    if ((fp = fopen(file_path, "r")) == NULL)
    {
        return slicer;
    }
    for (int i = 0; i < READ_LINE_NUM; i++)
    {
        char line[1024] = {0};
        if (fgets(line, 1024, fp) != NULL)
        {
            if (strstr(line, "PrusaSlicer") != NULL)
            {
                slicer = PRUSA_SLICER;
                break;
            }
            else if (strstr(line, "AnycubicSlicer") != NULL)
            {
                slicer = ANYCUBIC_SLICER;
                break;
            }
            else if (strstr(line, "Cura_SteamEngine") != NULL)
            {
                slicer = CURA_SLICER;
                break;
            }
            else if (strstr(line, "OrcaSlicer") != NULL)
            {
                slicer = ORCA_SLICER;
                break;
            }
            else if (strstr(line, "ElegooSlicer") != NULL)
            {
                slicer = ELEGOO_SLICER;
                break;
            }
        }
        else
            break;
    }
    fclose(fp);
    return slicer;
}

int file_read_init(file_read_ctx_t *ctx, const char *file_path, int reverse)
{

    if (ctx == NULL || file_path == NULL)
    {
        return -1;
    }
    ctx->fp = fopen(file_path, "r");
    if (ctx->fp == NULL)
    {
        return -1;
    }
    ctx->reverse_read_pos = -2L; // 从文件末尾开始向前读取
    ctx->reverse = reverse;
    memset(ctx->line, 0, sizeof(ctx->line));
    return 0;
}

int file_read_deinit(file_read_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return -1;
    }
    if (ctx->fp != NULL)
    {
        fclose(ctx->fp);
        ctx->fp = NULL;
    }
    return 0;
}

char *file_read_line(file_read_ctx_t *ctx)
{
#define TIMEOUT_LIMIT 2 // 设定超时限制为2秒

    int64_t pos;
    int ch;

    if (ctx == NULL || ctx->fp == NULL)
    {
        return NULL;
    }
    if (ctx->reverse == 0)
    {
        return fgets(ctx->line, sizeof(ctx->line), ctx->fp);
    }

    time_t start_time = time(NULL); // 获取当前时间
    // 从文件末尾开始向前读取
    for (pos = ctx->reverse_read_pos;; pos--)
    {
        // 检查超时  
        if (difftime(time(NULL), start_time) > TIMEOUT_LIMIT) {  
            return NULL;  
        }  

        if (fseek(ctx->fp, pos, SEEK_END) != 0)
        {
            return NULL; // 已经读取到文件的开头
        }

        ch = fgetc(ctx->fp);
        if (ch == '\n' || ch == EOF)
        {
            if (fgets(ctx->line, sizeof(ctx->line), ctx->fp) != NULL)
            {
                ctx->reverse_read_pos = pos - 1; // 更新已读取的位置
                return ctx->line;
            }
        }
    }
    return NULL;
}

static int get_key_value(char *line, char *key, char *delimiter, char *value)
{
    char *saveptr = NULL;
    char line_copy[MAX_LINE_LENGTH] = {0};
    strncpy(line_copy, line, sizeof(line_copy));
    char *token = strtok_r(line_copy, delimiter, &saveptr);
    int find_key = 0;
    while (token != NULL)
    {
        LOG_D("token %s\n", token);
        if (find_key == 1)
        {
            int start_pos = 0;
            for (int i = 0; i < strlen(token); i++)
            {
                if (token[i] == ' ') // 去掉前面空格
                    continue;
                else
                {
                    strncpy(value, token + i, strlen(token) - i - 1);
                    strcat(value, "\0");
                    LOG_D("Key: %s\n", key);
                    LOG_D("Value: %s\n", value);
                    return 0;
                }
            }
            return -1;
        }
        if (strstr(token, key) != NULL)
        {
            find_key = 1;
        }
        token = strtok_r(NULL, delimiter, &saveptr);
    }
    return -1;
}

static void removeSpaces(char *str)
{
    int count = 0;

    for (int i = 0; str[i]; i++)
    {
        if (str[i] != ' ')
        {
            str[count++] = str[i];
        }
    }
    str[count] = '\0';
}

int gcode_preview(const char *file_path, char *preview_path, int need_preview, slice_param_t *slice_param, char *file_name)
{
    DIR *dirp = NULL;
    file_read_ctx_t file_read_ctx = {0};
    int ret = 0;
    
    // 初始化和错误检查
    if (!file_path || !file_name) {
        return -1;
    }
    
    slice_type_t slicer = find_gcode_slicer(file_path);
    slice_param_t slice_param_tmp = {0};
    slice_param_tmp.slice_type = slicer;
    char img_path[PATH_MAX_LEN + 1] = {"0"};

    // 目录检查和创建
    if (access(THUMBNAIL_DIR, F_OK) != 0) {
        if (utils_vfork_system("mkdir -p %s", THUMBNAIL_DIR) != 0 || 
            access(THUMBNAIL_DIR, F_OK) != 0) {
            return -1;
        }
    }

    // 打开目录
    dirp = opendir(THUMBNAIL_DIR);
    if (!dirp) {
        LOG_I("opendir %s failed, errno: %s\n", THUMBNAIL_DIR, strerror(errno));
        return -1;
    }

    // 处理文件预览和参数解析
    if (slicer == PRUSA_SLICER || slicer == ANYCUBIC_SLICER || 
        slicer == ORCA_SLICER || slicer == ELEGOO_SLICER) {
        int thumbnail_base64_size = 0;
        if (file_read_init(&file_read_ctx, file_path, 0) != 0) {
            closedir(dirp);
            return -1;
        }
        while (file_read_line(&file_read_ctx) != NULL)
        {
            char value[MAX_LINE_LENGTH] = {0};
            if (strlen(file_read_ctx.line) < 5)
                continue;
            LOG_D("Line: %s\n", file_read_ctx.line);
            if (strchr("MGmg", file_read_ctx.line[0]) != NULL) // 首字符是gcode命令，就退出解析
            {
                if (file_read_ctx.reverse == 1)
                    break;
                file_read_ctx.reverse = 1;
                LOG_D("reverse read\n");
            }
            else if (strstr(file_read_ctx.line, "; thumbnail begin ") != NULL && need_preview) // 预览图解析
            {
                int count = 0;
                char *saveptr = NULL;
                char *token = strtok_r(file_read_ctx.line + strlen("; thumbnail begin "), " ", &saveptr);
                while (token != NULL)
                {
                    LOG_D("token %s\n", token);
                    if (count == 0)
                        sscanf(token, "%dx%d", &slice_param_tmp.thumbnail_width, &slice_param_tmp.thumbnail_heigh);
                    else if(count == 1)
                        thumbnail_base64_size = strtoul(token, NULL, 10);
                    count++;
                    token = strtok_r(NULL, " ", &saveptr);
                }
                LOG_D("thumbnail_width: %d, thumbnail_heigh: %d, thumbnail_base64_size: %d\n", slice_param_tmp.thumbnail_width, slice_param_tmp.thumbnail_heigh, thumbnail_base64_size);
                uint8_t *thumbnail_raw_data = (uint8_t *)calloc((thumbnail_base64_size + 4), sizeof(uint8_t));
                uint8_t *thumbnail_decode_data = (uint8_t *)calloc(3 + 3 * (thumbnail_base64_size + 4) / 4, sizeof(uint8_t));
                if (thumbnail_raw_data != NULL)
                {
                    if (thumbnail_decode_data == NULL)
                    {
                        free(thumbnail_raw_data);
                        continue;
                    }
                    while (file_read_line(&file_read_ctx) != NULL)
                    {
                        if (strstr(file_read_ctx.line, "; thumbnail end") != NULL)
                            break;
                        else
                        {
                            if (file_read_ctx.line[strlen(file_read_ctx.line) - 2] == '\r' || file_read_ctx.line[strlen(file_read_ctx.line) - 2] == '\0')
                            {
                                strncat((char *)thumbnail_raw_data, strlen(file_read_ctx.line) > 2 ? file_read_ctx.line + 2 : file_read_ctx.line, strlen(file_read_ctx.line) > 2 ? strlen(file_read_ctx.line) - 4 : strlen(file_read_ctx.line));
                            }
                            else
                            {
                                strncat((char *)thumbnail_raw_data, strlen(file_read_ctx.line) > 2 ? file_read_ctx.line + 2 : file_read_ctx.line, strlen(file_read_ctx.line) > 2 ? strlen(file_read_ctx.line) - 3 : strlen(file_read_ctx.line));
                            }
                        }
                    }
                    int decode_size = Base64_decode(thumbnail_decode_data, 3 + 3 * (thumbnail_base64_size) / 4, (char *)thumbnail_raw_data, thumbnail_base64_size);
                    if (decode_size > 0)
                    {
                        sprintf(img_path, "%s/%s.%s", THUMBNAIL_DIR, file_name, "png");
                        if (preview_path != NULL)
                            strcpy(preview_path, img_path);
                        // printf("img_path = %s\n", img_path);
                        FILE *thumbnail_fp = NULL;
                        thumbnail_fp = fopen(img_path, "wb");
                        if (thumbnail_fp != NULL)
                        {
                            fwrite(thumbnail_decode_data, 1, decode_size, thumbnail_fp);
                            fflush(thumbnail_fp);
                            fsync(fileno(thumbnail_fp));
                            fclose(thumbnail_fp);
                        }
                    }
                    free(thumbnail_raw_data);
                    free(thumbnail_decode_data);
                }
            }
            else if (get_key_value(file_read_ctx.line, "estimated printing time (normal mode)", "=", value) == 0)
            {
                removeSpaces(value);
                char pattern[] = "([0-9]{1,2}d)?([0-9]{1,2}h)?([0-9]{1,2}m)?([0-9]{1,2}s)?";
                regex_t regex;
                int reti;
                reti = regcomp(&regex, pattern, REG_EXTENDED);
                if (reti)
                {
                    continue;
                }
                regmatch_t pmatch[5];
                reti = regexec(&regex, value, 5, pmatch, 0);
                int time[4] = {0};
                // int hours = 0, minutes = 0, seconds = 0;
                if (!reti)
                {
                    for (int i = 1; i < 5; i++)
                    {
                        if (pmatch[i].rm_so != -1)
                        {
                            int len = pmatch[i].rm_eo - pmatch[i].rm_so;
                            char match_str[len + 1];
                            strncpy(match_str, value + pmatch[i].rm_so, len);
                            match_str[len] = '\0';
                            time[i - 1] = atoi(match_str);
                        }
                    }
                }
                // sscanf(value, "%dh%dm%d", &time[0], &time[1], &time[2]);
                // int hours = time[0] * 24 + time[1];
                sprintf(slice_param_tmp.estimeated_time_str, "%02dd%02dh%02dm%02ds", time[0], time[1], time[2], time[3]);
                // strcpy(slice_param_tmp.estimeated_time_str, token);
                slice_param_tmp.estimated_time = time[0] * 86400 + time[1] * 3600 + time[2] * 60 + time[3];
            }
            else if (get_key_value(file_read_ctx.line, "bed_temperature", "=", value) == 0) // 最好能用哈希表处理。
            {
                slice_param_tmp.bed_temperature = strtod(value, NULL);
            }
            else if (get_key_value(file_read_ctx.line, "temperature", "=", value) == 0)
            {
                slice_param_tmp.temperature = strtod(value, NULL);
            }
            else if (get_key_value(file_read_ctx.line, "filament_type", "=", value) == 0 || get_key_value(file_read_ctx.line, "initial_filament",":",  value) == 0)
            {
                strncpy(slice_param_tmp.filament_type, value, sizeof(slice_param_tmp.filament_type));
            }
            else if (get_key_value(file_read_ctx.line, "total layers count","=",  value) == 0 || get_key_value(file_read_ctx.line, "total_layers","=",  value) == 0)
            {
                slice_param_tmp.total_layers = strtod(value, NULL);
            }
            else if (get_key_value(file_read_ctx.line, "total filament used [g]","=",  value) == 0 || get_key_value(file_read_ctx.line, "filament used [g]","=",  value) == 0)
            {
                slice_param_tmp.est_filament_weight = strtod(value, NULL);
            }
        }
        LOG_D("gcode preview stop\n");
        file_read_deinit(&file_read_ctx);
    }
    else if (slicer == CURA_SLICER)
    {
#define READ_LINE_NUM 50
        if (file_read_init(&file_read_ctx, file_path, 0) != 0) {
            closedir(dirp);
            return -1;
        }
        for (uint8_t i = 0; i < READ_LINE_NUM; i++) // 读取前50行
        {
            /* code */
            if (file_read_line(&file_read_ctx) != NULL)
            {
                if (strlen(file_read_ctx.line) < 5)
                    continue;
                LOG_D("Line: %s\n", file_read_ctx.line);
                char value[MAX_LINE_LENGTH] = {0};
                int time[3] = {0};
                if (get_key_value(file_read_ctx.line, "TIME", ":", value) == 0 && 
                    strstr(file_read_ctx.line, "TIME_ELAPSED") == NULL)
                {
                    int remainder_seconds = 0;
                    int total_time = atoi(value);
                    time[0] = total_time / 3600;
                    remainder_seconds = total_time % 3600;
                    time[1] = remainder_seconds / 60;
                    remainder_seconds = remainder_seconds % 60;
                    sprintf(slice_param_tmp.estimeated_time_str, "%02dh%02dm", time[0], time[1]);
                    slice_param_tmp.estimated_time = total_time;
                }
                else if(get_key_value(file_read_ctx.line, "LAYER_COUNT", ":", value) == 0)
                {
                    int total_layers = atoi(value);
                    slice_param_tmp.total_layers = total_layers;
                    printf("total layers count: %d\n", slice_param_tmp.total_layers);
                }
            }
        }
        file_read_deinit(&file_read_ctx);
        
    }

    // 复制参数并清理
    if (slice_param) {
        memcpy(slice_param, &slice_param_tmp, sizeof(slice_param_t));
    }
    
    closedir(dirp);
    return ret;
}
