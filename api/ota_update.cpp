#include "ota_update.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdarg.h>
#include "hl_tpool.h"
#include "progress_ipc.h"
#include "hl_common.h"
#include "hl_assert.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include "config.h"
#include "ota_update.h"
#define LOG_TAG "ota_update"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif
#define enum_string(x) [x] = #x
#define OTA_FIRMWARE_MAGIC "\x14\x17\x0B\x17"
typedef struct
{
    hl_tpool_thread_t thread;
    struct progress_msg msg;
} ota_update_ctx_t;
typedef struct
{
    hl_tpool_thread_t thread;
    hl_ota_dec_state_t state;
    char src_path[256];
    char out_path[256];
    uint8_t r_md5[16];
    uint32_t r_len;
    uint8_t r_info[4];
} ota_dec_ctx_t;
static const char *get_status_string(unsigned int status)
{
    const char *const str[] = {
        enum_string(IDLE),
        enum_string(START),
        enum_string(RUN),
        enum_string(SUCCESS),
        enum_string(FAILURE),
        enum_string(DOWNLOAD),
        enum_string(DONE),
        enum_string(SUBPROCESS)};

    if (status >= ARRAY_SIZE(str))
        return "UNKNOWN";

    return str[status];
}
static void get_boot_partition(char *boot_part, size_t len)
{
    FILE *fp = NULL;
    char line[2048] = {0};

    /* 执行 fw_printenv 命令，通过 grep 命令筛选出包含 "boot_partition" 的行 */
    fp = popen("fw_printenv | grep boot_partition", "r");
    if (fp == NULL)
    {
        printf("Failed to run command\n");
        exit(1);
    }

    /* 读取命令的输出 */
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char key[1024] = {0};
        char value[1024] = {0};

        /* 解析命令的输出 */
        if (sscanf(line, "%[^=]=%s", key, value) == 2)
        {
            /* 如果键是 boot_partition，那么将值复制到 boot_part */
            if (strcmp(key, "boot_partition") == 0)
            {
                strncpy(boot_part, value, len - 1);
            }
        }
    }
    /* 关闭文件 */
    pclose(fp);
}
int ota_update(const char *path)
{
    std::cout << "ota_update" << std::endl;
    if (path == NULL)
    {
        printf("path is NULL\n");
        return -1;
    }
    char boot_part[128] = {0};
    char cmd_tmp[2048] = {0};
    get_boot_partition(boot_part, sizeof(boot_part));
    if (strlen(boot_part) == 0)
    {
        printf("Failed to get boot partition\n");
        return -1;
    }
    if (strcmp(boot_part, "bootA") == 0)
    {
        snprintf(cmd_tmp, sizeof(cmd_tmp), "swupdate -i %s -e stable,now_A_next_B -k /etc/swupdate_public.pem -p reboot &", path); // -n 不升级，测试
        // snprintf(cmd_tmp, sizeof(cmd_tmp), "swupdate -i %s -e stable,now_A_next_B -k /etc/swupdate_public.pem -p reboot -n &", path);
    }
    else if (strcmp(boot_part, "bootB") == 0)
    {
        snprintf(cmd_tmp, sizeof(cmd_tmp), "swupdate -i %s -e stable,now_B_next_A -k /etc/swupdate_public.pem -p reboot &", path); // -n 不升级，测试
        // snprintf(cmd_tmp, sizeof(cmd_tmp), "swupdate -i %s -e stable,now_B_next_A -k /etc/swupdate_public.pem -p reboot -n &", path);
    }
    system(cmd_tmp);
    return 0;
}
int ota_update2(char *root_path)
{
    std::cout << "ota_update2" << std::endl;
    char cmd_str[100];
    char boot_part[128] = {0};
    char swu_path[256] = {0};
    memset(cmd_str, 0, sizeof(cmd_str));
    strcpy(cmd_str, "swupdate -i ");
    struct stat info;
    snprintf(swu_path, sizeof(swu_path), "%s/update/update.swu", root_path);
    if (access(swu_path, F_OK) != 0)
    {
        LOG_E("ota file %s not exist\n", swu_path);
        return -1;
    }
    strcat(cmd_str, swu_path);
    strcat(cmd_str, " -e stable,");
    get_boot_partition(boot_part, sizeof(boot_part));
    if (strlen(boot_part) == 0)
    {
        LOG_E("Failed to get boot partition\n");
        return -1;
    }
    if (strcmp(boot_part, "bootA") == 0)
    {
        LOG_I("now_A_next_B\n");
        strcat(cmd_str, "now_A_next_B");
    }
    else if (strcmp(boot_part, "bootB") == 0)
    {
        LOG_I("now_B_next_A\n");
        strcat(cmd_str, "now_B_next_A");
    }
    strcat(cmd_str, " -k /etc/swupdate_public.pem &");
    // strcat(cmd_str, " -k /etc/swupdate_public.pem -n &"); //-n 不执行升级，测试用
    LOG_I("system run: %s start: \n", cmd_str);
    int resB = hl_system(cmd_str);
    if (resB != 0)
    {
        LOG_I("system run: %s errer \n", cmd_str);
    }
    else
    {
        LOG_I("system run: %s success \n", cmd_str);
    }
    return 0;
}
static int _progress_ipc_connect(const char *socketpath, hl_tpool_thread_t thread)
{
    struct sockaddr_un servaddr;
    int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strncpy(servaddr.sun_path, socketpath, sizeof(servaddr.sun_path));

    LOG_I("Trying to connect to SWUpdate...Path:%s\n", socketpath);

    do
    {
        if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
        {
            break;
        }

        usleep(10000);
    } while (hl_tpool_thread_test_cancel(thread) == 0);

    if (fd == -1)
    {
        LOG_E("cannot communicate with SWUpdate via %s\n", socketpath);
        return -1;
    }
    LOG_I("Connected to SWUpdate via %s\n", socketpath);
    return fd;
}

static void ota_routine(hl_tpool_thread_t thread, void *args)
{
    int fd = -1;
    RECOVERY_STATUS status = IDLE;
    unsigned int step = 0;
    unsigned int percent = 0;
    char str[512] = {0};
    int ret = -1;
    ota_update_ctx_t *ctx = (ota_update_ctx_t *)args;
    struct progress_msg ota_msg;
    // fd = progress_ipc_connect(true);
    fd = _progress_ipc_connect(get_prog_socket(), thread);
    do
    {
        if (fd != -1)
        {
            ret = progress_ipc_receive(&fd, &ota_msg);
            if (ret != sizeof(ctx->msg))
            {
                if (ota_msg.status != DONE)
                {
                    ctx->msg.status = FAILURE;
                }
                LOG_I("progress ipc stop %d , %s\n", ret, get_status_string(ota_msg.status));
            }
            else
            {
                // if (ota_msg.status != status || ota_msg.status == FAILURE)
                // {
                //     status = ota_msg.status;

                //     snprintf(str, sizeof(str),
                //              "{\r\n"
                //              "\t\"type\": \"status\",\r\n"
                //              "\t\"status\": \"%s\"\r\n"
                //              "}\r\n",
                //              get_status_string(ota_msg.status));
                //     printf("%s", str);
                // }
                memcpy(&ctx->msg, &ota_msg, sizeof(ctx->msg));
            }
        }
        else if (ota_msg.status != DONE)
        {
            LOG_E("progress ipc connect fail\n");
            ctx->msg.status = FAILURE;
            usleep(100000);
        }
        usleep(1000);
    } while (hl_tpool_thread_test_cancel(thread) == 0 && fd != -1);
    return;
}
int ota_progress_thread_create(hl_ota_update_ctx_t *ctx)
{
    HL_ASSERT(ctx != NULL);
    ota_update_ctx_t *ota_ctx = (ota_update_ctx_t *)malloc(sizeof(ota_update_ctx_t));
    if (ota_ctx == NULL)
        return -1;
    memset(ota_ctx, 0, sizeof(ota_update_ctx_t));
    if (hl_tpool_create_thread(&ota_ctx->thread, ota_routine, ota_ctx, 0, 0, 0, 0) != 0)
    {
        free(ota_ctx);
        return -1;
    }
    hl_tpool_wait_started(ota_ctx->thread, 0);
    *ctx = ota_ctx;
    return 0;
}
void ota_progress_thread_destory(hl_ota_update_ctx_t *ctx)
{
    HL_ASSERT(ctx != NULL);
    HL_ASSERT(*ctx != NULL);
    ota_update_ctx_t *c = (ota_update_ctx_t *)(*ctx);
    hl_tpool_cancel_thread(c->thread);
    hl_tpool_wait_completed(c->thread, 0);
    hl_tpool_destory_thread(&c->thread);

    LOG_I("ota_progress_thread_destory ready clean\n");

    free(c);
    *ctx = NULL;
}
struct progress_msg *get_ota_progress_msg(hl_ota_update_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }
    ota_update_ctx_t *ota_ctx = (ota_update_ctx_t *)*ctx;
    return &ota_ctx->msg;
}
int update_firmware_check(const char *input_file, uint8_t *md5, uint32_t *len, uint8_t *info)
{
    FILE *fp = fopen(input_file, "rb");
    if (fp == NULL)
    {
        return -1;
    }
    uint8_t magic[4] = {0};
    uint8_t firmware_info[4] = {0};
    uint8_t custom_info[4] = {0};
    uint8_t firmware_len[4] = {0};
    uint8_t firmware_md5[16] = {0};

    if (fread(magic, 1, sizeof(magic), fp) != sizeof(magic) ||
        fread(firmware_info, 1, sizeof(firmware_info), fp) != sizeof(firmware_info) ||
        fread(custom_info, 1, sizeof(custom_info), fp) != sizeof(custom_info) ||
        fread(firmware_len, 1, sizeof(firmware_len), fp) != sizeof(firmware_len) ||
        fread(firmware_md5, 1, sizeof(firmware_md5), fp) != sizeof(firmware_md5))
    {
        LOG_E("read firmware file fail\n");
        fclose(fp);
        return -1;
    }
    if (memcmp(magic, OTA_FIRMWARE_MAGIC, 4) != 0) // OTA_FIRMWARE_MAGIC
    {
        LOG_E("firmware file magic error\n");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    memcpy(md5, firmware_md5, 16);
    memcpy(info, firmware_info, 4);
    *len = (uint32_t)((firmware_len[3] << 24) | (firmware_len[2] << 16) | (firmware_len[1] << 8) | firmware_len[0]);
    return 0;
}
static void ota_dec_routine(hl_tpool_thread_t thread, void *args)
{
    ota_dec_ctx_t *ctx = (ota_dec_ctx_t *)args;
    uint8_t firmware_calc_md5[16] = {0};
    do
    {
        hl_md5_seek(ctx->src_path, firmware_calc_md5, 32);
        if (memcmp(ctx->r_md5, firmware_calc_md5, 16) != 0) //
        {
            LOG_E("firmware file md5 error\n");
            ctx->state = HL_OTA_DEC_FAILED;
            return;
        }
        break;
    } while (hl_tpool_thread_test_cancel(thread) == 0);
    LOG_I("firmware file decrypt success\n");
    ctx->state = HL_OTA_DEC_COMPLETED;
    return;
}
int ota_dec_thread_create(hl_ota_dec_ctx_t *ctx, char *input_file, char *output_file)
{
    HL_ASSERT(ctx != NULL);
    ota_dec_ctx_t *dec_ctx = (ota_dec_ctx_t *)malloc(sizeof(ota_dec_ctx_t));
    if (dec_ctx == NULL || input_file == NULL)
        return -1;
    memset(dec_ctx, 0, sizeof(ota_dec_ctx_t));
    if (update_firmware_check(input_file, dec_ctx->r_md5, &dec_ctx->r_len, dec_ctx->r_info) != 0) // 固件校验失败
    {
        free(dec_ctx);
        return -1;
    }
    if (dec_ctx->r_info[3] != CONFIG_BOARD) // 固件不匹配
    {
        LOG_E("error cur: %d target: %d\n", CONFIG_BOARD, dec_ctx->r_info[3]);
        free(dec_ctx);
        return -1;
    }
    strncpy(dec_ctx->src_path, input_file, sizeof(dec_ctx->src_path));
    strncpy(dec_ctx->out_path, output_file, sizeof(dec_ctx->out_path));
    if (hl_tpool_create_thread(&dec_ctx->thread, ota_dec_routine, dec_ctx, 0, 0, 0, 0) != 0)
    {
        LOG_E("ota_dec_routine create faild\n");
        free(dec_ctx);
        return -1;
    }
    dec_ctx->state = HL_OTA_DEC_RUNNING;
    hl_tpool_wait_started(dec_ctx->thread, 0);
    *ctx = dec_ctx;
    return 0;
}
void ota_dec_thread_destory(hl_ota_dec_ctx_t *ctx)
{
    HL_ASSERT(ctx != NULL);
    HL_ASSERT(*ctx != NULL);
    ota_dec_ctx_t *c = (ota_dec_ctx_t *)(*ctx);
    hl_tpool_cancel_thread(c->thread);
    hl_tpool_wait_completed(c->thread, 0);
    hl_tpool_destory_thread(&c->thread);

    LOG_I("ota_dec_thread_destory ready clean\n");

    free(c);
    *ctx = NULL;
}
hl_ota_dec_state_t ota_dec_get_state(hl_ota_dec_ctx_t *ctx)
{
    HL_ASSERT(ctx != NULL);
    HL_ASSERT(*ctx != NULL);
    ota_dec_ctx_t *ota_ctx = (ota_dec_ctx_t *)(*ctx);
    return ota_ctx->state;
}
