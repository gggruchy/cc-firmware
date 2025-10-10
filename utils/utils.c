#include "utils.h"
#include <sys/vfs.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include "Base64.h"
/**
 * @brief 获取文件大小
 *
 * @param filename
 * @return uint64_t
 */
uint64_t utils_get_size(const char *filepath)
{
    struct stat st;
    UTILS_CHECK_ERRNO(stat(filepath, &st) == 0);
    return st.st_size;
}

/**
 * @brief 获取目录可用大小
 *
 * @param dirpath
 * @return uint64_t
 */
uint64_t utils_get_dir_available_size(const char *dirpath)
{
    struct statfs st;
    UTILS_CHECK_ERRNO(statfs(dirpath, &st) == 0);
    return st.f_bsize * st.f_bavail;
}

/**
 * @brief 获取目录大小
 *
 * @param dirpath
 * @return uint64_t
 */
uint64_t utils_get_dir_size(const char *dirpath)
{
    struct statfs st;
    UTILS_CHECK_ERRNO(statfs(dirpath, &st) == 0);
    return st.f_bsize * st.f_blocks;
}

/**
 * @brief 文件复制
 *
 * @param dst_file
 * @param dst_offset
 * @param src_file
 * @param src_offset
 * @param cb
 * @return int
 */
int utils_copy_at(const char *dst_file, uint64_t dst_offset, const char *src_file, uint64_t src_offset, void *data, void (*cb)(void *data, uint64_t size, uint64_t offset, int end))
{
#define COPY_BUFFER_SIZE (4096)
    FILE *src = NULL;
    FILE *dst = NULL;
    uint64_t size = 0;
    uint64_t offset = 0;
    uint32_t rlen = 0;
    uint32_t wlen = 0;
    uint8_t copy_buf[COPY_BUFFER_SIZE];
    int ret = 0;

    if (UTILS_CHECK_ERRNO(src_file != NULL))
    {
        ret = -1;
        goto fail;
    }

    if (UTILS_CHECK_ERRNO(dst_file != NULL))
    {
        ret = -2;
        goto fail;
    }

    src = fopen(src_file, "rb");
    dst = fopen(dst_file, "wb+");

    if (src == NULL || dst == NULL)
    {
        ret = -3;
        goto fail_to_close;
    }

    if (UTILS_CHECK_ERRNO(fseek(src, src_offset, SEEK_SET) != -1))
    {
        ret = -4;
        goto fail_to_close;
    }

    if (UTILS_CHECK_ERRNO(fseek(dst, dst_offset, SEEK_SET) != -1))
    {
        ret = -5;
        goto fail_to_close;
    }

    size = utils_get_size(src_file) - src_offset;

    while (offset < size)
    {
        rlen = size - offset > COPY_BUFFER_SIZE ? COPY_BUFFER_SIZE : size - offset;
        rlen = fread(copy_buf, 1, rlen, src);
        if (rlen == 0)
            break;
        wlen = fwrite(copy_buf, 1, rlen, dst);
        if (wlen == 0)
            break;
        if (cb)
            cb(data, size, offset, 0);
        offset += wlen;
        if (UTILS_CHECK_ERRNO(fseek(src, wlen - rlen, SEEK_CUR) != -1))
        {
            ret = -6;
            goto fail_to_close;
        }
    }
    fflush(src);
    // fsync(fileno(src));
    fflush(dst);
    // fsync(fileno(dst));
    if (cb)
        cb(data, size, offset, 1);
    if (UTILS_CHECK(offset == size))
    {
        goto fail_to_close;
        return -7;
    }
    fclose(src);
    fclose(dst);
    return 0;
fail_to_close:
    if (src)
        fclose(src);
    if (dst)
        fclose(dst);
fail:
    return ret;
}

/**
 * @brief 获取文件头部信息,用于文件识别
 *
 * @param filename
 * @param header
 * @return int
 */
int utils_get_header(common_header_t *header, const char *filepath)
{
    FILE *fp = NULL;
    int ret = 0;
    if (UTILS_CHECK(filepath != NULL))
    {
        ret = -1;
        goto fail;
    }
    if (UTILS_CHECK(header != NULL))
    {
        ret = -2;
        goto fail;
    }
    if (UTILS_CHECK(strlen(filepath) > 0))
    {
        ret = -3;
        goto fail;
    }
    fp = fopen(filepath, "rb");
    if (UTILS_CHECK_ERRNO(fp != NULL))
    {
        ret = -4;
        goto fail;
    }
    if (UTILS_CHECK_ERRNO(fread(header, 1, sizeof(common_header_t), fp) >= 0))
    {
        ret = -5;
        goto fail_to_close;
    }

    fclose(fp);
    return 0;

fail_to_close:
    fclose(fp);
fail:
    return ret;
}

const char *utils_get_suffix(const char *filepath)
{
    if (UTILS_CHECK(filepath != NULL))
        return NULL;
    if (UTILS_CHECK(strlen(filepath) > 1))
        return "";
    const char *p = filepath + strlen(filepath);
    while (p != filepath)
    {
        p--;
        if (*p == '.')
            return p + 1;
    }
    return "";
}

const char *utils_get_file_name(const char *filepath)
{
    if (UTILS_CHECK(filepath != NULL))
        return NULL;
    if (UTILS_CHECK(strlen(filepath) > 1))
        return NULL;
    const char *p = filepath + strlen(filepath);
    while (p != filepath)
    {
        p--;
        if (*p == '/')
            return p + 1;
    }
    return NULL;
}

void utils_get_prefix(const char *filepath, char *prefix)
{
    memcpy(prefix, filepath, strlen(filepath) + 1);
    char *p = prefix + strlen(prefix);
    while (*(--p) != '.')
        ;
    *p = '\0';
}

void utils_get_file_dir(const char *filepath, char *dir)
{
    memcpy(dir, filepath, strlen(filepath) + 1);
    char *p = dir + strlen(dir);
    while (*(--p) != '/')
        ;
    p++;
    *p = '\0';
}

int utils_get_bmp_info(const char *filepath, uint32_t *w, uint32_t *h, uint16_t *biBitCount)
{
    FILE *fp = NULL;
    uint8_t headers[54];
    if (UTILS_CHECK(filepath != NULL))
        return -1;
    if (UTILS_CHECK(strlen(filepath) > 0))
        return -2;
    fp = fopen(filepath, "rb");
    if (UTILS_CHECK_ERRNO(fp != NULL))
        return -3;
    if (UTILS_CHECK_ERRNO(fread(headers, 1, sizeof(headers), fp) == sizeof(headers)))
    {
        fclose(fp);
        return -4;
    }
    memcpy(w, headers + 18, 4);
    memcpy(h, headers + 22, 4);
    memcpy(biBitCount, headers + 28, 2);
    fclose(fp);
    return 0;
}

uint64_t utils_get_current_tick(void)
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    return (tv.tv_sec * 1000 + tv.tv_nsec / 1000000);
}

uint64_t utils_get_current_tick_us(void)
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    return (tv.tv_sec * 1000000 + tv.tv_nsec / 1000);
}

void utils_second2time(uint64_t tsecond, uint32_t *hour, uint32_t *minute, uint32_t *second)
{
    *hour = tsecond / 3600;
    *minute = tsecond % 3600 / 60;
    *second = tsecond % 60;
}

void utils_second2timestr(uint64_t tsecond, char *str, uint32_t len)
{
    int hour = tsecond / 3600;
    int minute = tsecond % 3600 / 60;
    int second = tsecond % 60;
    snprintf(str, len, "%.2d:%.2d:%.2d", hour, minute, second);
}

extern int __libc_system(char *command);
int utils_system(const char *format, ...)
{
#define _CMD_LEN (256)
    char cmdBuff[_CMD_LEN];
    va_list vaList;
    va_start(vaList, format);
    vsnprintf((char *)cmdBuff, sizeof(cmdBuff), format, vaList);
    va_end(vaList);
    return __libc_system((char *)cmdBuff);
#undef _CMD_LEN
}

int utils_vfork_system(const char *format, ...)
{
#define _CMD_LEN (32 * 1024)
    char cmdBuff[_CMD_LEN];
    va_list vaList;
    va_start(vaList, format);
    vsnprintf((char *)cmdBuff, sizeof(cmdBuff), format, vaList);
    va_end(vaList);
#undef _CMD_LEN

    pid_t pid;
    int status;

    if (cmdBuff == NULL)
        return (1);

    if ((pid = vfork()) < 0)
        status = -1;

    else if (pid == 0)
    {
        execl("/bin/sh", "sh", "-c", cmdBuff, (char *)0);
        exit(127);
    }
    else
    {
        while (waitpid(pid, &status, 0) < 0)
        {
            if (errno != 4)
            {
                status = -1;
                break;
            }
        }
    }
    return status;
}

int utils_path_to_name(char *name, const char *path)
{
    if (UTILS_CHECK(path != NULL))
        return -1;
    if (UTILS_CHECK(name != NULL))
        return -2;
    const char *p = path + strlen(path);
    while (p != path)
    {
        if (*(--p) == '/')
            break;
    }
    if (UTILS_CHECK(p != path))
        return -3;
    p++;
    if (UTILS_CHECK(strlen(p) != 0))
        return -4;
    strcpy(name, p);
    return 0;
}

static char *utils_get_base_path(const char *filepath)
{
    static char path[PATH_MAX_LEN] = {0};
    snprintf(path, sizeof(path), "%s", filepath);
    int len = strlen(path);
    while (path[--len] != '/')
        ;
    path[++len] = '\0';
    return path;
}

// 同名处理: /mnt/emmc/1.c  --> /mnt/emmc/1(1).c
int utils_file_path_handle(char *new_path, const char *path)
{
    char buf_file_serial_number[32] = {0}; //(序号)
    char str_file_serial_number[32] = {0}; // 文件序号 最大序号:65536
    int int_file_serial_number;
    char file_name[NAME_MAX_LEN] = {0};
    char file_base_path[PATH_MAX_LEN] = {0};
    char old_path[PATH_MAX_LEN] = {0};
    if (access(path, F_OK) != 0)
        return -1;
    utils_path_to_name(file_name, path);
    const char *p = file_name + strlen(file_name);
    // 判断是否存在后缀
    if (strstr(file_name, ".") == NULL) // 文件无后缀
    {
        int file_name_index = strlen(file_name) - 1;
        if (file_name[file_name_index] == ')') // 括号处理
        {
            while (file_name[--file_name_index] != '(')
            {
                if (file_name[file_name_index] < 48 || file_name[file_name_index] > 57) // 括号里有不是数字的字符
                    return -2;
            }
            if (file_name[file_name_index] == '(' && file_name[file_name_index + 1] == ')')
                return -3;
            for (int i = file_name_index + 1, j = 0; file_name[i] != ')'; i++, j++)
                str_file_serial_number[j] = file_name[i];
            int_file_serial_number = atoi(str_file_serial_number);
            if (int_file_serial_number > 65536 || int_file_serial_number <= 0) // 文件序号处理
                return -4;
            snprintf(buf_file_serial_number, sizeof(buf_file_serial_number), "(%d)", ++int_file_serial_number); // 递增文件序号
            file_name_index--;
            for (int j = 0; buf_file_serial_number[j] != '\0'; j++) // 生成新的文件名
                file_name[++file_name_index] = buf_file_serial_number[j];
            file_name[++file_name_index] = '\0';
            snprintf(old_path, PATH_MAX_LEN, "%s%s", utils_get_base_path(path), file_name);
            if (access(old_path, F_OK) == 0)
                return utils_file_path_handle(new_path, old_path);
            else
            {
                strncpy(new_path, old_path, PATH_MAX_LEN);
                return 0;
            }
        }
        else // 生成第一个序号
        {
            file_name[++file_name_index] = '(';
            file_name[++file_name_index] = '1';
            file_name[++file_name_index] = ')';
            file_name[++file_name_index] = '\0';
            snprintf(old_path, PATH_MAX_LEN, "%s%s", utils_get_base_path(path), file_name);
            if (access(old_path, F_OK) == 0)
                return utils_file_path_handle(new_path, old_path);
            else
            {
                strncpy(new_path, old_path, PATH_MAX_LEN);
                return 0;
            }
        }
    }
    else // 文件有后缀
    {
        char file_name_prefix[NAME_MAX_LEN] = {0};
        const char *file_name_suffix = utils_get_suffix(path);
        utils_get_prefix(path, file_name_prefix);

        int file_name_index = strlen(file_name_prefix) - 1;
        if (file_name_prefix[file_name_index] == ')') // 括号处理
        {
            while (file_name_prefix[--file_name_index] != '(')
            {
                if (file_name_prefix[file_name_index] < 48 || file_name_prefix[file_name_index] > 57) // 括号里有不是数字的字符
                    return -2;
            }
            if (file_name_prefix[file_name_index] == '(' && file_name_prefix[file_name_index + 1] == ')')
                return -3;
            for (int i = file_name_index + 1, j = 0; file_name_prefix[i] != ')'; i++, j++)
                str_file_serial_number[j] = file_name_prefix[i];
            int_file_serial_number = atoi(str_file_serial_number);
            if (int_file_serial_number > 65536 || int_file_serial_number <= 0) // 文件序号处理
                return -4;
            snprintf(buf_file_serial_number, sizeof(buf_file_serial_number), "(%d)", ++int_file_serial_number); // 递增文件序号
            file_name_index--;
            for (int j = 0; buf_file_serial_number[j] != '\0'; j++) // 生成新的文件名
                file_name_prefix[++file_name_index] = buf_file_serial_number[j];
            file_name_prefix[++file_name_index] = '\0';

            snprintf(old_path, PATH_MAX_LEN, "%s.%s", file_name_prefix, file_name_suffix);
            if (access(old_path, F_OK) == 0)
                return utils_file_path_handle(new_path, old_path);
            else
            {
                strncpy(new_path, old_path, PATH_MAX_LEN);
                return 0;
            }
        }
        else // 生成第一个序号
        {
            file_name_prefix[++file_name_index] = '(';
            file_name_prefix[++file_name_index] = '1';
            file_name_prefix[++file_name_index] = ')';
            file_name_prefix[++file_name_index] = '\0';
            snprintf(old_path, PATH_MAX_LEN, "%s.%s", file_name_prefix, file_name_suffix);
            if (access(old_path, F_OK) == 0)
                return utils_file_path_handle(new_path, old_path);
            else
            {
                strncpy(new_path, old_path, PATH_MAX_LEN);
                return 0;
            }
        }
    }
    return 0;
}

int utils_generate_md5(const char *filepath, char *digest)
{
    char name[NAME_MAX + 1];
    struct stat st;
    if (filepath == NULL)
        return -1;
    stat(filepath, &st);
    // 使用最后修改时间+文件大小+文件名称进行MD5
    MD5_CTX ctx;
    MD5_Init(&ctx);
    memset(digest, 0, 16);
    uint64_t size = st.st_size;
    uint64_t createtimesec = st.st_ctim.tv_sec;
    uint64_t createimensec = st.st_ctim.tv_nsec;
    uint64_t modifytimesec = st.st_mtim.tv_sec;
    uint64_t modifytimensec = st.st_mtim.tv_nsec;
    utils_path_to_name(name, filepath);
    MD5_Update(&ctx, (uint8_t *)&size, sizeof(size));
    MD5_Update(&ctx, (uint8_t *)&createtimesec, sizeof(createtimesec));
    MD5_Update(&ctx, (uint8_t *)&createimensec, sizeof(createimensec));
    MD5_Update(&ctx, (uint8_t *)&modifytimesec, sizeof(modifytimesec));
    MD5_Update(&ctx, (uint8_t *)&modifytimensec, sizeof(modifytimensec));
    MD5_Update(&ctx, (uint8_t *)name, strlen(name));
    MD5_Final(digest, &ctx);
    return 0;
}

bool utils_ipv4_verify(char *ip)
{
    if (NULL == ip)
        return false;
    int a, b, c, d;
    char t;

    if (4 == sscanf(ip, "%d.%d.%d.%d%c", &a, &b, &c, &d, &t))
    {
        if (0 <= a && a <= 255 && 0 <= b && b <= 255 && 0 <= c && c <= 255 && 0 <= d && d <= 255)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

int eliminate_string_blank(char *name, const char *whole_mame)
{
    if (UTILS_CHECK(name != NULL))
        return -1;
    if (UTILS_CHECK(whole_mame != NULL))
        return -2;
    char str[256];
    int i = 0, j = 0;
    for (; i < strlen(whole_mame); i++)
    {
        if (whole_mame[i] == ' ')
            continue;
        str[j] = whole_mame[i];
        j++;
    }
    str[j] = '\0';
    if (strlen(str) > 15)
        str[15] = '\0';

    strcpy(name, str);
    return 0;
}

void utils_get_udisk_prefix(const char *filepath, char *prefix)
{
    const char *p = filepath;
    int len = strlen(filepath);

    while (p != filepath + len - 1)
    {
        *prefix++ = *p++;
    }
    *prefix = '\0';
}
// RSA加密，加密结果转换为base64编码
int rsa_encrypt_string_with_cert_str(const char *plaintext, const char *cert_str, char *ciphertext_base64, int *ciphertext_base64_len)
{
    BIO *bio = BIO_new_mem_buf(cert_str, -1);
    if (bio == NULL)
    {
        return -1;
    }
    X509 *cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (cert == NULL)
    {
        return -1;
    }

    // 获取公钥
    EVP_PKEY *pkey = X509_get_pubkey(cert);
    if (pkey == NULL)
    {
        X509_free(cert);
        return -1;
    }
    RSA *rsa = EVP_PKEY_get1_RSA(pkey);
    EVP_PKEY_free(pkey);
    if (rsa == NULL)
    {
        X509_free(cert);
        return -1;
    }

    // 加密字符串
    int plaintext_len = strlen(plaintext);
    int rsa_len = RSA_size(rsa);
    char ciphertext[rsa_len];
    int ciphertext_len = RSA_public_encrypt(plaintext_len, (unsigned char *)plaintext, ciphertext, rsa, RSA_PKCS1_PADDING);
    if (ciphertext_len == -1)
    {
        RSA_free(rsa);
        X509_free(cert);
        return -1;
    }
    // 将加密结果转换为 base64 编码
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_write(b64, ciphertext, ciphertext_len);
    BIO_flush(b64);
    char *ciphertext_base64_ptr;
    *ciphertext_base64_len = BIO_get_mem_data(mem, &ciphertext_base64_ptr);
    memcpy(ciphertext_base64, ciphertext_base64_ptr, *ciphertext_base64_len);
    ciphertext_base64[*ciphertext_base64_len] = '\0';
    printf("ciphertext_base64_len = %d\n", *ciphertext_base64_len);
    // 释放资源
    RSA_free(rsa);
    X509_free(cert);
    BIO_free_all(b64);
    return 0;
}

// RSA加密，加密结果转换为base64编码
int rsa_encrypt_string_with_cert_file(const char *plaintext, const char *cert_file, char *ciphertext_base64, int *ciphertext_base64_len)
{
    BIO *bio = BIO_new_file(cert_file, "rb");
    if (bio == NULL)
    {
        return -1;
    }
    char *cert_str = (char *)malloc(4096);
    int cert_str_len = BIO_read(bio, cert_str, 4096);
    BIO_free_all(bio);
    if (cert_str_len <= 0)
    {
        free(cert_str);
        return -1;
    }
    cert_str[cert_str_len] = '\0';

    BIO *bio_mem = BIO_new_mem_buf(cert_str, -1);
    if (bio_mem == NULL)
    {
        free(cert_str);
        return -1;
    }
    X509 *cert = PEM_read_bio_X509(bio_mem, NULL, NULL, NULL);
    BIO_free(bio_mem);
    free(cert_str);
    if (cert == NULL)
    {
        return -1;
    }

    // 获取公钥
    EVP_PKEY *pkey = X509_get_pubkey(cert);
    if (pkey == NULL)
    {
        X509_free(cert);
        return -1;
    }
    RSA *rsa = EVP_PKEY_get1_RSA(pkey);
    EVP_PKEY_free(pkey);
    if (rsa == NULL)
    {
        X509_free(cert);
        return -1;
    }

    // 加密字符串
    int plaintext_len = strlen(plaintext);
    int rsa_len = RSA_size(rsa);
    char ciphertext[rsa_len];
    int ciphertext_len = RSA_public_encrypt(plaintext_len, (unsigned char *)plaintext, ciphertext, rsa, RSA_PKCS1_PADDING);
    if (ciphertext_len == -1)
    {
        RSA_free(rsa);
        X509_free(cert);
        return -1;
    }
    // 将加密结果转换为 base64 编码
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_write(b64, ciphertext, ciphertext_len);
    BIO_flush(b64);
    char *ciphertext_base64_ptr;
    *ciphertext_base64_len = BIO_get_mem_data(mem, &ciphertext_base64_ptr);
    memcpy(ciphertext_base64, ciphertext_base64_ptr, *ciphertext_base64_len);
    ciphertext_base64[*ciphertext_base64_len] = '\0';
    // printf("ciphertext_base64_len = %d\n", *ciphertext_base64_len);
    // 释放资源
    RSA_free(rsa);
    X509_free(cert);
    BIO_free_all(b64);
    return 0;
}
/*
brief: 将16进制数转换为str
eg: uint8_t data[4] = {0x12,0x34,0x56,0x78} --> s: "12345678"
        utils_hex2str(data,&s,4)
*/
int utils_hex2str(uint8_t *data, char *s, int len)
{
    int i;
    char *tmp = data;
    s[2 * len] = 0;
    for (i = 0; i < len; i++)
    {
        if (((*tmp >> 4) & 0xf) <= 9)
            s[i * 2] = ((*tmp >> 4) & 0xf) + '0';
        else
            s[i * 2] = ((*tmp >> 4) & 0xf) + 'a' - 0xa;

        if ((*tmp & 0xf) <= 9)
            s[i * 2 + 1] = (*tmp & 0xf) + '0';
        else
            s[i * 2 + 1] = (*tmp & 0xf) + 'a' - 0xa;

        tmp++;
    }
    return 1;
}

void utils_string_toupper(const char *in, char *out, int outsize)
{
    int i = 0;
    int len = strlen(in);
    len = len > outsize ? outsize : len;
    for (i = 0; i < len; i++)
        out[i] = toupper(in[i]);
    out[i] = '\0';
}
