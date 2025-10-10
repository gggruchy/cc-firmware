#include "miniunz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include "hl_tpool.h"
#include "hl_assert.h"
#define LOG_TAG "miniunz"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (8192)
#define MAXFILENAME (256)
static int miniunz_do_extract(unzFile uf, int opt_extract_without_path, int opt_overwrite, const char *password, uint64_t *offset);
typedef struct
{
    hl_tpool_thread_t thread;
    unzFile uf;
    uint64_t offset;
    uint64_t size;
    hl_unz_state_t state;
    char *password;
} unz_ctx_t;
static void change_file_date(const char *filename, uLong dosdate, tm_unz tmu_date)
{
    struct utimbuf ut;
    struct tm newdate;
    newdate.tm_sec = tmu_date.tm_sec;
    newdate.tm_min = tmu_date.tm_min;
    newdate.tm_hour = tmu_date.tm_hour;
    newdate.tm_mday = tmu_date.tm_mday;
    newdate.tm_mon = tmu_date.tm_mon;
    if (tmu_date.tm_year > 1900)
        newdate.tm_year = tmu_date.tm_year - 1900;
    else
        newdate.tm_year = tmu_date.tm_year;
    newdate.tm_isdst = -1;

    ut.actime = ut.modtime = mktime(&newdate);
    utime(filename, &ut);
}
static int makedir(char *newdir)
{
    char *buffer;
    char *p;
    int len = (int)strlen(newdir);

    if (len <= 0)
        return 0;

    buffer = (char *)calloc(len + 1, sizeof(char));
    if (buffer == NULL)
    {
        LOG_I("Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }
    strcpy(buffer, newdir);

    if (buffer[len - 1] == '/')
    {
        buffer[len - 1] = '\0';
    }
    if (mkdir(buffer, 0755) == 0)
    {
        free(buffer);
        return 1;
    }

    p = buffer + 1;
    while (1)
    {
        char hold;

        while (*p && *p != '\\' && *p != '/')
            p++;
        hold = *p;
        *p = 0;
        if ((mkdir(buffer, 0755) == -1) && (errno == ENOENT))
        {
            LOG_I("couldn't create directory %s\n", buffer);
            free(buffer);
            return 0;
        }
        if (hold == 0)
            break;
        *p++ = hold;
    }
    free(buffer);
    return 1;
}
static int do_extract_currentfile(unzFile uf, const int *popt_extract_without_path, int *popt_overwrite, const char *password, uint64_t *offset)
{
    char filename_inzip[256];
    char *filename_withoutpath;
    char *p;
    int err = UNZ_OK;
    FILE *fout = NULL;
    void *buf;
    uInt size_buf;
    uint64_t offset_tmp = 0;

    unz_file_info64 file_info;
    uLong ratio = 0;
    err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);

    if (err != UNZ_OK)
    {
        LOG_I("error %d with zipfile in unzGetCurrentFileInfo\n", err);
        return err;
    }

    size_buf = WRITEBUFFERSIZE;
    buf = (void *)malloc(size_buf);
    if (buf == NULL)
    {
        LOG_I("Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }

    p = filename_withoutpath = filename_inzip;
    while ((*p) != '\0')
    {
        if (((*p) == '/') || ((*p) == '\\'))
            filename_withoutpath = p + 1;
        p++;
    }

    if ((*filename_withoutpath) == '\0')
    {
        if ((*popt_extract_without_path) == 0)
        {
            LOG_I("creating directory: %s\n", filename_inzip);
            mkdir(filename_inzip, 0755);
        }
    }
    else
    {
        char *write_filename;
        int skip = 0;

        if ((*popt_extract_without_path) == 0)
            write_filename = filename_inzip;
        else
            write_filename = filename_withoutpath;

        err = unzOpenCurrentFilePassword(uf, password);
        if (err != UNZ_OK)
        {
            LOG_I("error %d with zipfile in unzOpenCurrentFilePassword\n", err);
        }

        if (((*popt_overwrite) == 0) && (err == UNZ_OK))
        {
            char rep = 0;
            FILE *ftestexist;
            ftestexist = FOPEN_FUNC(write_filename, "rb");
            if (ftestexist != NULL)
            {
                fclose(ftestexist);
                do
                {
                    char answer[128];
                    int ret;

                    LOG_I("The file %s exists. Overwrite ? [y]es, [n]o, [A]ll: ", write_filename);
                    ret = scanf("%1s", answer);
                    if (ret != 1)
                    {
                        exit(EXIT_FAILURE);
                    }
                    rep = answer[0];
                    if ((rep >= 'a') && (rep <= 'z'))
                        rep -= 0x20;
                } while ((rep != 'Y') && (rep != 'N') && (rep != 'A'));
            }

            if (rep == 'N')
                skip = 1;

            if (rep == 'A')
                *popt_overwrite = 1;
        }

        if ((skip == 0) && (err == UNZ_OK))
        {
            fout = FOPEN_FUNC(write_filename, "wb");
            /* some zipfile don't contain directory alone before file */
            if ((fout == NULL) && ((*popt_extract_without_path) == 0) &&
                (filename_withoutpath != (char *)filename_inzip))
            {
                char c = *(filename_withoutpath - 1);
                *(filename_withoutpath - 1) = '\0';
                makedir(write_filename);
                *(filename_withoutpath - 1) = c;
                fout = FOPEN_FUNC(write_filename, "wb");
            }

            if (fout == NULL)
            {
                LOG_I("error opening %s\n", write_filename);
            }
        }

        if (fout != NULL)
        {
            LOG_I(" extracting: %s\n", write_filename);

            do
            {
                err = unzReadCurrentFile(uf, buf, size_buf);
                if (err < 0)
                {
                    LOG_I("error %d with zipfile in unzReadCurrentFile\n", err);
                    break;
                }
                if (err > 0)
                    if (fwrite(buf, err, 1, fout) != 1)
                    {
                        LOG_I("error in writing extracted file\n");
                        err = UNZ_ERRNO;
                        break;
                    }
                *offset += err;
            } while (err > 0);
            LOG_I("decompression offset %llu\n", *offset);
            if (fout)
                fclose(fout);

            if (err == 0)
                change_file_date(write_filename, file_info.dosDate,
                                 file_info.tmu_date);
        }

        if (err == UNZ_OK)
        {
            err = unzCloseCurrentFile(uf);
            if (err != UNZ_OK)
            {
                LOG_I("error %d with zipfile in unzCloseCurrentFile\n", err);
            }
        }
        else
            unzCloseCurrentFile(uf); /* don't lose the error */
    }

    free(buf);
    return err;
}
static int miniunz_update_info(const char *path, unz_ctx_t *ctx)
{
    if (path == NULL || ctx == NULL)
        return -1;
    unzFile uf;
    unz_global_info64 gi;
    int err;
    unz_file_info64 file_info;
    char filename_inzip[256];
    uf = unzOpen64(path);
    if (uf == NULL)
    {
        return -1;
    }
    err = unzGetGlobalInfo64(uf, &gi);
    if (err != UNZ_OK)
    {
        LOG_I("error %d with zipfile in unzGetGlobalInfo \n", err);
        unzClose(uf);
        return -1;
    }

    for (uLong i = 0; i < gi.number_entry; i++)
    {
        err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
        if (err != UNZ_OK)
        {
            LOG_I("error %d with zipfile in unzGetCurrentFileInfo\n", err);
            unzClose(uf);
            return -1;
        }
        ctx->size += file_info.uncompressed_size;
        if ((i + 1) < gi.number_entry)
        {
            err = unzGoToNextFile(uf);
            if (err != UNZ_OK)
            {
                LOG_I("error %d with zipfile in unzGoToNextFile\n", err);
                unzClose(uf);
                return -1;
            }
        }
    }
    LOG_I("miniunz_update_info success size %llu\n", ctx->size);
    unzClose(uf);
    return 0;
}
static int miniunz_do_extract(unzFile uf, int opt_extract_without_path, int opt_overwrite, const char *password, uint64_t *offset)
{
    uLong i;
    unz_global_info64 gi;
    int err;
    FILE *fout = NULL;
    err = unzGetGlobalInfo64(uf, &gi);
    if (err != UNZ_OK)
    {
        LOG_I("error %d with zipfile in unzGetGlobalInfo \n", err);
        return -1;
    }

    for (i = 0; i < gi.number_entry; i++)
    {
        if (do_extract_currentfile(uf, &opt_extract_without_path,
                                   &opt_overwrite,
                                   password, offset) != UNZ_OK)
            break;

        if ((i + 1) < gi.number_entry)
        {
            err = unzGoToNextFile(uf);
            if (err != UNZ_OK)
            {
                LOG_I("error %d with zipfile in unzGoToNextFile\n", err);
                return -1;
            }
        }
    }

    return 0;
}
hl_unz_state_t hl_unz_get_state(hl_unz_ctx_t ctx, uint64_t *offset, uint64_t *size)
{
    HL_ASSERT(ctx != NULL);
    unz_ctx_t *c = (unz_ctx_t *)ctx;
    *offset = c->offset;
    *size = c->size;
    return c->state;
}
static void unz_routine(hl_tpool_thread_t thread, void *args)
{
    unz_ctx_t *c = (unz_ctx_t *)args;
    c->state = HL_UNZ_STATE_RUNNING;
    do
    {
        if (hl_tpool_thread_test_cancel(thread))
        {
            LOG_I("unz_routine cancelled\n");
            break;
        }

        if (miniunz_do_extract(c->uf, 0, 1, c->password, &c->offset) != 0) // 阻塞操作
        {
            LOG_I("miniunz_do_extract failed\n");
            c->state = HL_UNZ_STATE_FAILED;
            break;
        }
        else
        {
            LOG_I("miniunz_do_extract success\n");
            c->state = HL_UNZ_STATE_COMPLETED;
            break;
        }
    } while (hl_tpool_thread_test_cancel(thread) == 0);
}
void hl_unz_task_destory(hl_unz_ctx_t *ctx)
{
    HL_ASSERT(ctx != NULL);
    HL_ASSERT(*ctx != NULL);
    LOG_I("hl_unz_task_destory\n");
    unz_ctx_t *c = (unz_ctx_t *)(*ctx);
    hl_tpool_cancel_thread(c->thread);
    hl_tpool_wait_completed(c->thread, 0);
    hl_tpool_destory_thread(&c->thread);
    if (c->password != NULL)
    {
        free(c->password);
    }
    unzClose(c->uf);
    free(c);
    *ctx = NULL;
}
int hl_unz_task_create(hl_unz_ctx_t *ctx, const char *path, const char *decompression_path, const char *password)
{
    if (ctx == NULL || path == NULL || decompression_path == NULL)
        return -1;
    unz_ctx_t *c = (unz_ctx_t *)calloc(1, sizeof(unz_ctx_t));
    if (c == NULL)
        return -1;
    c->uf = unzOpen64(path);
    if (c->uf == NULL)
    {
        LOG_I("unzOpen64 failed %s\n", path);
        free(c);
        return -1;
    }
    if (miniunz_update_info(path, c) == -1)
    {
        LOG_I("miniunz_update_info failed %s\n", path);
        free(c);
        return -1;
    }
    char *cwd = getcwd(NULL, 0); // 获取当前工作目录
    if (cwd == NULL)
    {
        LOG_I("getcwd failed");
        free(c);
        return -1;
    }
    if (strcmp(cwd, decompression_path) != 0) // 如果当前工作目录不是 decompression_path
    {
        if (chdir(decompression_path) != 0) // 切换到 decompression_path 目录
        {
            free(cwd);
            free(c);
            return -1;
        }
    }
    free(cwd);
    if (password != NULL)
    {
        c->password = (char *)calloc(1, strlen(password) + 1);
        strncpy(c->password, password, strlen(password));
    }
    else
    {
        c->password = NULL;
    }
    LOG_I("hl_unz_task_create\n");
    c->state = HL_UNZ_STATE_IDLE;

    if (hl_tpool_create_thread(&c->thread, unz_routine, c, 0, 0, 0, 0) != 0)
    {
        free(c);
        if (c->password != NULL)
        {
            free(c->password);
        }
        LOG_I("hl_tpool_create_thread failed\n");
        return -1;
    }
    hl_tpool_wait_started(c->thread, 0);
    *ctx = c;

    return 0;
}