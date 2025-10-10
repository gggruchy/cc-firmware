#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include "hl_common.h"
#include "hl_assert.h"
#include "openssl/md5.h"
#include "Base64.h"
#include "sys/time.h"
#include <sys/statfs.h>
#include <openssl/aes.h>
#include <dirent.h>  
#include <stdio.h>  
typedef struct
{
    pthread_cond_t cond;
    pthread_mutex_t cond_mutex;
    pthread_condattr_t cond_attr;
} easy_cond_t;

int hl_easy_cond_create(hl_easy_cond_t *cond)
{
    HL_ASSERT(cond != NULL);
    easy_cond_t *c = (easy_cond_t *)malloc(sizeof(easy_cond_t));
    if (c == NULL)
        return -1;

    if (pthread_mutex_init(&c->cond_mutex, NULL) != 0)
    {
        free(c);
        return -1;
    }

    if (pthread_condattr_init(&c->cond_attr) != 0)
    {
        pthread_mutex_destroy(&c->cond_mutex);
        free(c);
        return -1;
    }

    if (pthread_condattr_setclock(&c->cond_attr, CLOCK_MONOTONIC) != 0)
    {
        pthread_condattr_destroy(&c->cond_attr);
        pthread_mutex_destroy(&c->cond_mutex);
        free(c);
        return -1;
    }

    if (pthread_cond_init(&c->cond, &c->cond_attr) != 0)
    {
        pthread_condattr_destroy(&c->cond_attr);
        pthread_mutex_destroy(&c->cond_mutex);
        free(c);
        return -1;
    }
    *cond = c;
    return 0;
}

void hl_easy_cond_destory(hl_easy_cond_t *cond)
{
    HL_ASSERT(cond != NULL);
    HL_ASSERT(*cond != NULL);
    easy_cond_t *c = (easy_cond_t *)(*cond);
    pthread_cond_destroy(&c->cond);
    pthread_condattr_destroy(&c->cond_attr);
    pthread_mutex_destroy(&c->cond_mutex);
    free(c);
    *cond = NULL;
}

int hl_easy_cond_lock(hl_easy_cond_t cond)
{
    HL_ASSERT(cond != NULL);
    easy_cond_t *c = (easy_cond_t *)cond;
    return pthread_mutex_lock(&c->cond_mutex);
}

int hl_easy_cond_trylock(hl_easy_cond_t cond)
{
    HL_ASSERT(cond != NULL);
    easy_cond_t *c = (easy_cond_t *)cond;
    return pthread_mutex_trylock(&c->cond_mutex);
}

int hl_easy_cond_unlock(hl_easy_cond_t cond)
{
    HL_ASSERT(cond != NULL);
    easy_cond_t *c = (easy_cond_t *)cond;
    return pthread_mutex_unlock(&c->cond_mutex);
}

int hl_easy_cond_wait(hl_easy_cond_t cond)
{
    HL_ASSERT(cond != NULL);
    easy_cond_t *c = (easy_cond_t *)cond;
    return pthread_cond_wait(&c->cond, &c->cond_mutex);
}

int hl_easy_cond_timewait(hl_easy_cond_t cond, const struct timespec *timeout)
{
    HL_ASSERT(cond != NULL);
    easy_cond_t *c = (easy_cond_t *)cond;
    return pthread_cond_timedwait(&c->cond, &c->cond_mutex, timeout);
}

int hl_easy_cond_signal(hl_easy_cond_t cond)
{
    HL_ASSERT(cond != NULL);
    easy_cond_t *c = (easy_cond_t *)cond;
    return pthread_cond_signal(&c->cond);
}

int hl_easy_cond_boardcast(hl_easy_cond_t cond)
{
    HL_ASSERT(cond != NULL);
    easy_cond_t *c = (easy_cond_t *)cond;
    return pthread_cond_broadcast(&c->cond);
}

struct timespec hl_calculate_timeout(uint64_t millisecond)
{
    struct timespec now;
    struct timespec timeout;
    clock_gettime(CLOCK_MONOTONIC, &now);
    timeout.tv_sec = now.tv_sec + millisecond / 1000ULL;
    timeout.tv_nsec = now.tv_nsec + (millisecond - (millisecond / 1000ULL) * 1000ULL) * 1000000ULL;
    if (timeout.tv_nsec > 1000000000ULL)
    {
        timeout.tv_sec += 1ULL;
        timeout.tv_nsec -= 1000000000ULL;
    }
    return timeout;
}

int __system__(const char *command)
{
    sigset_t blockMask, origMask;
    struct sigaction saIgnore, saOrigQuit, saOrigInt, saDefault;
    pid_t childPid;
    int status, savedErrno;

    if (command == NULL) /* Is a shell available? */
        return system(":") == 0;

    /* The parent process (the caller of system()) blocks SIGCHLD
       and ignore SIGINT and SIGQUIT while the child is executing.
       We must change the signal settings prior to forking, to avoid
       possible race conditions. This means that we must undo the
       effects of the following in the child after fork(). */

    sigemptyset(&blockMask); /* Block SIGCHLD */
    sigaddset(&blockMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &blockMask, &origMask);

    saIgnore.sa_handler = SIG_IGN; /* Ignore SIGINT and SIGQUIT */
    saIgnore.sa_flags = 0;
    sigemptyset(&saIgnore.sa_mask);
    sigaction(SIGINT, &saIgnore, &saOrigInt);
    sigaction(SIGQUIT, &saIgnore, &saOrigQuit);

    switch (childPid = fork())
    {
    case -1: /* fork() failed */
        status = -1;
        break; /* Carry on to reset signal attributes */

    case 0: /* Child: exec command */

        /* We ignore possible error returns because the only specified error
           is for a failed exec(), and because errors in these calls can't
           affect the caller of system() (which is a separate process) */
        for (int i = 3; i < sysconf(_SC_OPEN_MAX); i++)
            close(i);
        saDefault.sa_handler = SIG_DFL;
        saDefault.sa_flags = 0;
        sigemptyset(&saDefault.sa_mask);

        if (saOrigInt.sa_handler != SIG_IGN)
            sigaction(SIGINT, &saDefault, NULL);
        if (saOrigQuit.sa_handler != SIG_IGN)
            sigaction(SIGQUIT, &saDefault, NULL);

        sigprocmask(SIG_SETMASK, &origMask, NULL);

        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127); /* We could not exec the shell */

    default: /* Parent: wait for our child to terminate */

        /* We must use waitpid() for this task; using wait() could inadvertently
           collect the status of one of the caller's other children */

        while (waitpid(childPid, &status, 0) == -1)
        {
            if (errno != EINTR)
            { /* Error other than EINTR */
                status = -1;
                break; /* So exit loop */
            }
        }
        break;
    }

    /* Unblock SIGCHLD, restore dispositions of SIGINT and SIGQUIT */

    savedErrno = errno; /* The following may change 'errno' */

    sigprocmask(SIG_SETMASK, &origMask, NULL);
    sigaction(SIGINT, &saOrigInt, NULL);
    sigaction(SIGQUIT, &saOrigQuit, NULL);

    errno = savedErrno;

    return status;
}

int hl_system(const char *fmt, ...)
{
    FILE *fp;
    va_list ap;
    char cmdbuf[1024];
    int status;
    va_start(ap, fmt);
    vsnprintf(cmdbuf, sizeof(cmdbuf), fmt, ap);
    va_end(ap);
    status = __system__(cmdbuf);
    if (status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return 0;
    return -1;
}

int hl_echo(const char *path, const char *buf, uint32_t bufsize)
{
    int fd;
    if ((fd = open(path, O_WRONLY | O_SYNC)) == -1)
        return -1;

    if (write(fd, buf, bufsize) != bufsize)
    {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

uint64_t hl_get_tick_ns(void)
{
    struct timespec now;
    uint64_t tick;
    clock_gettime(CLOCK_MONOTONIC, &now);
    tick = now.tv_sec * 1000000000ULL + now.tv_nsec;
    return tick;
}

uint64_t hl_get_tick_us(void)
{
    return hl_get_tick_ns() / 1000ULL;
}

uint64_t hl_get_tick_ms(void)
{
    return hl_get_tick_ns() / 1000000ULL;
}

int hl_tick_is_overtime(uint64_t start_tick, uint64_t current_tick, uint64_t timeout_tick)
{
    uint64_t diff_tick = (uint64_t)(current_tick - start_tick);
    return diff_tick > timeout_tick;
}

uint64_t hl_get_utc_second(void)
{
    return time(NULL);
}

struct tm hl_get_time_from_utc_second(uint64_t utc_second)
{
    time_t time = utc_second;
    struct tm tm;
    gmtime_r(&time, &tm);
    return tm;
}
struct tm hl_get_localtime_time_from_utc_second(uint64_t utc_second)
{
    time_t time = utc_second;
    struct tm tm;
    localtime_r(&time, &tm);
    mktime(&tm);
    return tm;
}
uint64_t hl_get_utc_ms(void)
{
    static uint64_t last_now_ms = 0;
    struct timeval tv_now;
    uint64_t now_ms;
    gettimeofday(&tv_now, NULL);
    now_ms = (tv_now.tv_sec * 1000ULL) + (tv_now.tv_usec / 1000ULL) + 2000;
    if (now_ms <= last_now_ms) // AC...
    {
        now_ms = last_now_ms + 1;
    }
    last_now_ms = now_ms;
    return now_ms;
}
int hl_parse_usb_id(const char *product, uint16_t *vid, uint16_t *pid)
{
    char *saveptr;
    char *token;
    char tmp[128];
    strncpy(tmp, product, sizeof(tmp));
    token = strtok_r(tmp, "/", &saveptr);
    if (token != NULL)
    {
        *vid = strtol(token, NULL, 16);
        token = strtok_r(NULL, "/", &saveptr);
        if (token != NULL)
        {
            *pid = strtol(token, NULL, 16);
        }
        else
            return -1;
    }
    else
        return -1;
    return 0;
}

int hl_parse_usb_id2(const char *product, uint16_t *vid, uint16_t *pid)
{
    char *saveptr;
    char *token;
    char tmp[128];
    strncpy(tmp, product, sizeof(tmp));
    token = strtok_r(tmp, ":", &saveptr);
    if (token != NULL)
    {
        *vid = strtol(token, NULL, 16);
        token = strtok_r(NULL, ":", &saveptr);
        if (token != NULL)
        {
            *pid = strtol(token, NULL, 16);
        }
        else
            return -1;
    }
    else
        return -1;
    return 0;
}

int hl_get_line(char *buf, uint32_t bufsize, char **saveptr)
{
    char *p = *saveptr;
    uint32_t len = 0;
    if (*p == '\0')
        return 0;
    while (*p && *p != '\n' && len < bufsize - 1)
        buf[len++] = *p++;
    buf[len] = '\0';
    *saveptr = ++p;
    return 1;
}

typedef struct
{
    int fd_src;
    int fd_dest;
    uint64_t size;
    uint64_t offset;
    uint64_t segment;
} copy_ctx_t;

int hl_copy_create(void **ctx, const char *src, const char *dest)
{
    copy_ctx_t *cp_ctx;
    struct stat64 st;
    cp_ctx = (copy_ctx_t *)malloc(sizeof(copy_ctx_t));
    if (cp_ctx == NULL)
        return -1;
    memset(cp_ctx, 0, sizeof(copy_ctx_t));

    if ((cp_ctx->fd_src = open(src, O_RDONLY)) == -1)
        goto failed_free;
    if ((cp_ctx->fd_dest = open(dest, O_WRONLY | O_CREAT, 0777)) == -1)
        goto failed_close;
    if (fstat64(cp_ctx->fd_src, &st) == -1)
        goto failed_close;
    cp_ctx->size = st.st_size;
    cp_ctx->segment = st.st_size < 4 * 1024 ? st.st_size : st.st_size >> 7;
    *ctx = cp_ctx;
    return 0;
failed_close:
    if (cp_ctx->fd_src)
        close(cp_ctx->fd_src);
    if (cp_ctx->fd_dest)
        close(cp_ctx->fd_dest);
failed_free:
    free(cp_ctx);
    return -1;
}

void hl_copy_destory(void **ctx)
{
    copy_ctx_t *cp_ctx = (copy_ctx_t *)*ctx;
    if (cp_ctx->fd_src)
        close(cp_ctx->fd_src);
    if (cp_ctx->fd_dest)
        close(cp_ctx->fd_dest);
    free(cp_ctx);
}

int hl_copy(void *ctx, uint64_t *size, uint64_t *offset)
{
    copy_ctx_t *cp_ctx = (copy_ctx_t *)ctx;
    uint64_t length = cp_ctx->size - cp_ctx->offset > cp_ctx->segment ? cp_ctx->segment : cp_ctx->size - cp_ctx->offset;
    if (sendfile64(cp_ctx->fd_dest, cp_ctx->fd_src, &cp_ctx->offset, length) != -1)
    {
        if (size)
            *size = cp_ctx->size;
        if (offset)
            *offset = cp_ctx->offset;
        return 0;
    }
    return -1;
}

void hl_convert_version_string_to_version(const char *version_string, char *version, uint32_t size)
{
    char *token;
    char *saveptr;
    char tmp[256];
    int count = 0;
    int version_num[3] = {0};
    strncpy(tmp, version_string, sizeof(tmp));

    token = strtok_r(tmp, ".", &saveptr);
    while (token != NULL)
    {
        while (*token != '\0' && isdigit(*token) == 0)
            token++;
        version_num[count] = strtoul(token, NULL, 10);
        token = strtok_r(NULL, ".", &saveptr);
        count++;
        if (count == 3)
            break;
    }
    snprintf(version, size, "%d.%d.%d", version_num[0], version_num[1], version_num[2]);
}

int hl_version_compare(const char *v1, const char *v2)
{
    uint32_t v1_number[3];
    uint32_t v2_number[3];
    if (sscanf(v1, "%d.%d.%d", &v1_number[0], &v1_number[1], &v1_number[2]) == 3 && sscanf(v2, "%d.%d.%d", &v2_number[0], &v2_number[1], &v2_number[2]))
    {
        if (v1_number[0] > v2_number[0])
            return 1;
        else if (v1_number[0] < v2_number[0])
            return -1;

        if (v1_number[1] > v2_number[1])
            return 1;
        else if (v1_number[1] < v2_number[1])
            return -1;

        if (v1_number[2] > v2_number[2])
            return 1;
        else if (v1_number[2] < v2_number[2])
            return -1;
    }

    return 0;
}

const char *hl_get_name_from_path(const char *path)
{
    const char *p = path + strlen(path);
    while (p != path)
    {
        p--;
        if (*p == '/')
            return p + 1;
    }
    return NULL;
}

int hl_md5(const char *filepath, uint8_t *digest)
{
    FILE *fp = fopen(filepath, "rb+");
    MD5_CTX ctx;
    char buf[4096];
    ssize_t len;
    if (fp == NULL)
        return -1;
    MD5_Init(&ctx);
    memset(digest, 0, 16);
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0)
        MD5_Update(&ctx, buf, len);
    MD5_Final(digest, &ctx);
    fclose(fp);
    return 0;
}
int hl_md5_seek(const char *filepath, uint8_t *digest, uint32_t file_seek)
{
    FILE *fp = fopen(filepath, "rb+");
    MD5_CTX ctx;
    char buf[4096];
    ssize_t len;
    if (fp == NULL)
        return -1;
    if (file_seek > 0)
    {
        fseek(fp, file_seek, SEEK_SET);
    }
    MD5_Init(&ctx);
    memset(digest, 0, 16);
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0)
        MD5_Update(&ctx, buf, len);
    MD5_Final(digest, &ctx);
    fclose(fp);
    return 0;
}
int hl_str2hex(char *str, int len, uint8_t *digest)
{
    if (str == NULL || digest == NULL || len <= 0)
    {
        return -1;
    }
    for (int i = 0; i < len; i++)
    {
        sscanf(str + i * 2, "%02x", digest + i);
    }
    return 0;
}
int hl_hex2str(uint8_t *digest, int len, char *str)
{
    if (str == NULL || digest == NULL || len <= 0)
    {
        return -1;
    }
    for (int i = 0; i < len; i++)
    {
        sprintf(str + i * 2, "%02x", digest[i]);
    }
    return 0;
}
// int is_process_running(const char *process_nae)
// {
//     int bytes = 0;
//     char buf[256] = {0};
//     char cmd[256] = {0};
//     FILE *strea;
//     sprintf(cmd, "ps | grep %s | grep -v grep", process_nae);
//     strea = popen(cmd, "r");
//     if (!strea)
//         return -1;
//     bytes = fread(buf, sizeof(char), sizeof(buf), strea);
//     pclose(strea);
//     if (bytes > 0)
//     {
//         return 1;
//     }
//     else
//     {
//         return -1;
//     }
//     return 0;
// }
int is_process_running(const char *process_name) {  
    DIR *dir;  
    struct dirent *entry;  
    char path[256];  
    FILE *fp;  
    char cmdline[256];  

    // 打开 /proc 目录  
    dir = opendir("/proc");  
    if (!dir) {  
        perror("opendir");  
        return -1;  
    }  

    // 遍历目录中的每个条目  
    while ((entry = readdir(dir)) != NULL) {  
        // 检查目录名称是否为数字，代表进程ID  
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {  
            // 构建进程命令行路径  
            snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);  

            // 打开命令行文件  
            fp = fopen(path, "r");  
            if (fp) {  
                // 读取命令行  
                fgets(cmdline, sizeof(cmdline), fp);  
                fclose(fp);  

                // 检查命令行是否包含指定的进程名称  
                if (strstr(cmdline, process_name) != NULL) {  
                    closedir(dir);  
                    return 1; // 找到进程  
                }  
            }  
        }  
    }  

    closedir(dir);  
    return -1; // 未找到进程  
}  

/**
 * file_size: 文件大小(byte)
 */
int is_capacity_enough(const char *usb_path, __fsblkcnt64_t file_size)
{
    struct statfs s;
    __fsblkcnt64_t free_space = 0;
    memset(&s, 0, sizeof(struct statfs));
    if (0 != statfs(usb_path, &s))
    {
        return -1;
    }
    free_space = (__fsblkcnt64_t)(s.f_bavail * s.f_bsize);//可用字节数
    // printf("free_space = %lld, required_space = %lld\n", free_space, file_size);
    if (free_space >= file_size)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

#define AES_BLOCK_SIZE 16
void hex_string_to_bin(const char *hex_string, unsigned char *bin, int bin_length)
{
    for (int i = 0; i < bin_length; ++i)
    {
        sscanf(hex_string + 2 * i, "%2hhx", &bin[i]);
    }
}
/**
 * 使用AES加密文件
 * @param input_file 源文件路径
 * @param output_file 目标文件路径
 * @param hex_key 用于加密的密钥，长度必须为32字节
 */
int file_encrypt_aes(const char *input_file, const char *output_file, const char *hex_key, const char *hex_iv)
{
    unsigned char key[32] = {0};
    hex_string_to_bin(hex_key, key, sizeof(key));

    // 打开源文件
    FILE *ifp = fopen(input_file, "rb");
    if (!ifp)
    {
        printf("Input file opening failed\n");
        return -1;
    }

    unsigned char iv[AES_BLOCK_SIZE];
    hex_string_to_bin(hex_iv, iv, sizeof(iv));

    // 打开目标文件
    FILE *ofp = fopen(output_file, "wb");
    if (!ofp)
    {
        printf("Output file opening failed\n");
        fclose(ifp); // 如果目标文件打开失败，需要关闭已经打开的源文件
        return -1;
    }

    // 初始化AES密钥
    AES_KEY enc_key;
    AES_set_encrypt_key(key, 256, &enc_key);

    unsigned char read_buf[AES_BLOCK_SIZE];
    unsigned char cipher_buf[AES_BLOCK_SIZE];
    int read_bytes;
    // 读取源文件并加密
    while ((read_bytes = fread(read_buf, 1, AES_BLOCK_SIZE, ifp)) > 0)
    {
        // 如果这是最后一个数据块，且它的大小小于 AES_BLOCK_SIZE，那么添加 PKCS#7 填充
        if (read_bytes < AES_BLOCK_SIZE)
        {
            int padding = AES_BLOCK_SIZE - read_bytes;
            memset(read_buf + read_bytes, padding, padding);
            read_bytes = AES_BLOCK_SIZE;
        }

        // 使用AES加密
        AES_cbc_encrypt(read_buf, cipher_buf, read_bytes, &enc_key, iv, AES_ENCRYPT);

        // 将加密后的数据写入目标文件
        fwrite(cipher_buf, 1, read_bytes, ofp);
    }

    fflush(ifp);
    fsync(fileno(ifp));
    fflush(ofp);
    fsync(fileno(ofp));
    // 关闭源文件和目标文件
    fclose(ifp);
    fclose(ofp);
    return 0;
}
/**
 * 使用AES解密文件
 * @param input_file 源文件路径
 * @param output_file 目标文件路径
 * @param key 用于解密的密钥，长度必须为32字节
 */
int file_decrypt_aes(const char *input_file, const char *output_file, const char *hex_key, const char *hex_iv, uint32_t file_seek)
{
    unsigned char key[32] = {0};
    int padding = 0;
    hex_string_to_bin(hex_key, key, sizeof(key));

    // 打开源文件
    FILE *ifp = fopen(input_file, "rb");
    if (!ifp)
    {
        printf("Input file opening failed\n");
        return -1;
    }

    if (file_seek > 0)
    {
        fseek(ifp, file_seek, SEEK_SET);
    }
    unsigned char iv[AES_BLOCK_SIZE];
    hex_string_to_bin(hex_iv, iv, sizeof(iv));

    // 打开目标文件
    FILE *ofp = fopen(output_file, "wb");
    if (!ofp)
    {
        printf("Output file opening failed\n");
        fclose(ifp); // 如果目标文件打开失败，需要关闭已经打开的源文件
        return -1;
    }

    // 初始化AES密钥
    AES_KEY dec_key;
    AES_set_decrypt_key(key, 256, &dec_key);

    unsigned char read_buf[AES_BLOCK_SIZE];
    unsigned char plain_buf[AES_BLOCK_SIZE];
    int read_bytes;
    // 读取源文件并解密
    while ((read_bytes = fread(read_buf, 1, AES_BLOCK_SIZE, ifp)) > 0)
    {
        // 使用AES解密
        AES_cbc_encrypt(read_buf, plain_buf, read_bytes, &dec_key, iv, AES_DECRYPT);

        // 将解密后的数据写入目标文件
        fwrite(plain_buf, 1, read_bytes, ofp);
    }

    // 移除PKCS#7填充
    fseek(ofp, -1, SEEK_END);
    fread(&padding, 1, 1, ofp);
    printf("padding = %d\n", padding);
    if (padding > 0 && padding <= AES_BLOCK_SIZE)
    {
        int is_padding_true = 0;
        // 检查填充是否正确
        int padding_buf[AES_BLOCK_SIZE] = {0};
        fseek(ofp, -padding, SEEK_END);
        long end_pos = ftell(ofp); // 保存填充开始的位置
        fread(padding_buf, 1, padding, ofp);
        for (int i = 0; i < padding; i++)
        {
            if (padding_buf[i] != padding)
            {
                printf("padding error\n");
                is_padding_true = -1;
                break;
            }
        }
        if (is_padding_true == 0)
        {
            printf("remove padding\n");
            ftruncate(fileno(ofp), end_pos);
        }
    }
    fflush(ifp);
    fsync(fileno(ifp));
    fflush(ofp);
    fsync(fileno(ofp));
    // 关闭源文件和目标文件
    fclose(ifp);
    fclose(ofp);
    return 0;
}
int file_is_zip(const char *file_path, uint32_t file_seek)
{
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL)
    {
        return -1;
    }
    if (file_seek > 0)
    {
        fseek(fp, file_seek, SEEK_SET);
    }
    uint8_t file_head[4] = {0};
    fread(file_head, 1, 4, fp);
    fclose(fp);
    if (memcmp(file_head, "\x50\x4B\x03\x04", 4) != 0)
    {
        return -1;
    }
    return 0;
}