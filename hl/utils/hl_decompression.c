#include "hl_decompression.h"
#include "hl_common.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <zlib.h>

#define LOG_TAG "hl_decompression"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

typedef struct
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} tar_header_t;

typedef struct
{
    gzFile gzfile;
    uint64_t size;
    uint64_t offset;
    hl_gunzip_state_t state;
    tar_header_t tar_header;
    uint64_t tar_offset;
    uint64_t tar_size;
    char dir[1024];
    hl_gunzip_callback_t cb;
    FILE *fp;
} gunzip_ctx_t;

int hl_gunzip_ctx_create(hl_gunzip_ctx_t *ctx, const char *filepath, const char *dir, hl_gunzip_callback_t callback)
{
    gunzip_ctx_t *d = (gunzip_ctx_t *)malloc(sizeof(gunzip_ctx_t));
    if (d == NULL)
        return -1;
    memset(d, 0, sizeof(*d));

    d->cb = callback;
    d->gzfile = gzopen(filepath, "r");
    if (d->gzfile == NULL)
        return -1;
    strncpy(d->dir, dir, sizeof(d->dir));

    if (access(d->dir, F_OK) != 0)
        mkdir(d->dir, 0);

    // 读取未压缩数据大小
    FILE *fp = fopen(filepath, "r");
    uint32_t isize = 0;
    if (fp == NULL)
    {
        gzclose(d->gzfile);
        return -1;
    }
    fseek(fp, -4, SEEK_END);
    fread(&isize, 1, sizeof(isize), fp);
    fclose(fp);

    d->size = isize;
    d->offset = 0;
    d->state = HL_GUNZIP_STATE_RUNNING;

    *ctx = d;
    return 0;
}

void hl_gunzip_ctx_destory(hl_gunzip_ctx_t *ctx)
{
    gunzip_ctx_t *d = (gunzip_ctx_t *)*ctx;
    gzclose(d->gzfile);
    if (d->fp)
        fclose(d->fp);
    free(d);
    *ctx = NULL;
}

void hl_gunzip(hl_gunzip_ctx_t ctx)
{
    gunzip_ctx_t *d = (gunzip_ctx_t *)ctx;
    char buf[512 * 16];
    char *pbuf = buf;
    char *pend;
    int length;
    uint32_t slen;
    int skip = 0;
    char filepath[1024];
    if (d->state == HL_GUNZIP_STATE_RUNNING)
    {
        length = gzread(d->gzfile, buf, sizeof(buf));
        if (length > 0)
        {
            // d->offset = gztell(d->gzfile);
            d->offset += length;
            d->state = HL_GUNZIP_STATE_RUNNING;
            pbuf = buf;
            pend = buf + length;

            // 读取到文件尾部跳过1024字节块
            if (d->size == d->offset)
            {
                pend -= 1024;
            }

            // LOG_I("size %llu offset %llu legnth %d\n", d->size, d->offset, length);
            // 解析压缩数据
            while (pbuf < pend)
            {
                if (d->tar_size == 0)
                {
                    memcpy(&d->tar_header, pbuf, sizeof(d->tar_header));
                    pbuf += sizeof(d->tar_header);
                    d->tar_offset = 0;
                    d->tar_size = strtoul(d->tar_header.size, NULL, 8);
                    if (d->cb(d->tar_header.name) == 0)
                        skip = 1;
                    else
                        skip = 0;
                    
                    LOG_I("tar name %s size %s skip %d--- %llu\n", d->tar_header.name, d->tar_header.size, skip, d->tar_size);

                    if (skip == 0)
                    {
                        snprintf(filepath, sizeof(filepath), "%s/%s", d->dir, d->tar_header.name);
                        d->fp = fopen(filepath, "wb+");
                        if (d->fp == NULL)
                        {
                            LOG_I("gunzip open file failed: %s\n", filepath);
                            skip = 1;
                        }
                    }
                }
                else
                {
                    slen = (d->tar_size - d->tar_offset) > (pend - pbuf) ? (pend - pbuf) : (d->tar_size - d->tar_offset);
                    d->tar_offset += slen;
                    if (skip == 0)
                    {
                        if (d->fp)
                            fwrite(pbuf, 1, slen, d->fp);
                    }
                    pbuf += slen;
                    // LOG_I("tar name %s size %llu offset %llu\n", d->tar_header.name, d->tar_size, d->tar_offset);
                }

                if (d->tar_offset == d->tar_size)
                {
                    if (d->tar_size % 512)
                        pbuf += (512 - d->tar_size % 512);
                    d->tar_size = 0;
                    if (skip == 0)
                    {
                        if (d->fp)
                            fclose(d->fp);
                        d->fp = NULL;
                    }
                }
            }
        }
        else if (length == 0)
        {
            LOG_I("tar done\n");
            if (gzeof(d->gzfile))
            {
                d->state = HL_GUNZIP_STATE_COMPLETED;
            }
            else
            {
                d->state = HL_GUNZIP_STATE_FAILED;
            }
        }
        else
        {
            d->state = HL_GUNZIP_STATE_FAILED;
        }
    }
}

hl_gunzip_state_t hl_gunzip_get_state(hl_gunzip_ctx_t ctx, uint64_t *offset, uint64_t *size)
{
    gunzip_ctx_t *d = (gunzip_ctx_t *)ctx;
    *offset = d->offset;
    *size = d->size;
    return d->state;
}

int hl_gunzip_indentify(const char *filepath)
{
    uint16_t magic;
    FILE *fp = fopen(filepath, "r");
    if (fp == NULL)
        return -1;
    fread(&magic, 1, sizeof(magic), fp);
    fclose(fp);
    if (magic == 0x8b1f)
        return 1;
    return 0;
}