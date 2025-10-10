#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "openssl/md5.h"

#include "hl_ringbuffer.h"
#include "hl_assert.h"

typedef struct
{
    uint32_t index;
    uint32_t available_length;
    uint32_t length;
    uint32_t size;
    void *buf;
} ringbuffer_t;

typedef struct
{
    uint32_t index;
    uint32_t available_length;
    uint32_t length;
    uint32_t size;
    uint8_t digest[16];
} ringbuffer_db_header_t;
typedef struct
{
    FILE *fp;
    ringbuffer_t *rb;
} ringbuffer_db_t;

static uint32_t ringbuffer_index_to_pos(ringbuffer_t *rb, uint32_t index);
static int ringbuffer_db_verify(const char *db_file, uint32_t size, uint32_t length);
static void ringbuffer_db_digest(ringbuffer_db_t *db, ringbuffer_db_header_t *header);
static int ringbuffer_db_init(ringbuffer_db_t *db);
static int ringbuffer_db_create(ringbuffer_db_t *db, uint32_t size, uint32_t length);

int hl_ringbuffer_create(hl_ringbuffer_t *ringbuffer, uint32_t size, uint32_t length)
{
    HL_ASSERT(ringbuffer != NULL);
    HL_ASSERT(size != 0);
    HL_ASSERT(length != 0);

    ringbuffer_t *rb = (ringbuffer_t *)malloc(sizeof(ringbuffer_t));
    if (rb == NULL)
        return -1;
    rb->available_length = 0;
    rb->index = 0;
    rb->length = length;
    rb->size = size;
    rb->buf = malloc(rb->size * rb->length);

    if (rb->buf == NULL)
    {
        free(rb);
        return -1;
    }

    *ringbuffer = rb;
    return 0;
}

void hl_ringbuffer_destory(hl_ringbuffer_t *ringbuffer)
{
    HL_ASSERT(ringbuffer != NULL);
    HL_ASSERT(*ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)(*ringbuffer);
    free(rb->buf);
    free(rb);
    *ringbuffer = NULL;
}

void hl_ringbuffer_reset(hl_ringbuffer_t ringbuffer)
{
    HL_ASSERT(ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    rb->index = 0;
    rb->available_length = 0;
}

int hl_ringbuffer_set(hl_ringbuffer_t ringbuffer, uint32_t index, const void *data)
{
    HL_ASSERT(ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    if (index >= rb->available_length)
        return -1;
    memcpy(rb->buf + ringbuffer_index_to_pos(rb, index) * rb->size, data, rb->size);
    return 0;
}

int hl_ringbuffer_get(hl_ringbuffer_t ringbuffer, uint32_t index, void *data)
{
    HL_ASSERT(ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    if (index >= rb->available_length)
        return -1;
    memcpy(data, rb->buf + ringbuffer_index_to_pos(rb, index) * rb->size, rb->size);
    return 0;
}

int hl_ringbuffer_del(hl_ringbuffer_t ringbuffer, uint32_t index)
{
    HL_ASSERT(ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    if (index >= rb->available_length)
        return -1;
    for (int i = index; i < rb->available_length - 1; i++)
        memcpy(rb->buf + ringbuffer_index_to_pos(rb, i) * rb->size, rb->buf + ringbuffer_index_to_pos(rb, i + 1) * rb->size, rb->size);
    rb->available_length--;
    rb->index--;
    return 0;
}

void hl_ringbuffer_push(hl_ringbuffer_t ringbuffer, const void *data)
{
    HL_ASSERT(ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    memcpy(rb->buf + (rb->index % rb->length) * rb->size, data, rb->size);
    rb->index++;
    rb->available_length = rb->available_length < rb->length ? rb->available_length + 1 : rb->length;
}

void hl_ringbuffer_foreach(hl_ringbuffer_t ringbuffer, int (*callback)(hl_ringbuffer_t ringbuffer, uint32_t index, void *data, void *args), void *args)
{
    HL_ASSERT(ringbuffer != NULL);
    HL_ASSERT(callback != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    for (int i = 0; i < rb->available_length; i++)
    {
        if (callback(ringbuffer, i, rb->buf + ringbuffer_index_to_pos(rb, i) * rb->size, args))
            break;
    }
}

void hl_ringbuffer_foreach_reverse(hl_ringbuffer_t ringbuffer, int (*callback)(hl_ringbuffer_t ringbuffer, uint32_t index, void *data, void *args), void *args)
{
    HL_ASSERT(ringbuffer != NULL);
    HL_ASSERT(callback != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    for (int i = rb->available_length - 1; i >= 0; i--)
    {
        if (callback(ringbuffer, i, rb->buf + ringbuffer_index_to_pos(rb, i) * rb->size, args))
            break;
    }
}

uint32_t hl_ringbuffer_get_available_length(hl_ringbuffer_t ringbuffer)
{
    HL_ASSERT(ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    return rb->available_length;
}

uint32_t hl_ringbuffer_get_length(hl_ringbuffer_t ringbuffer)
{
    HL_ASSERT(ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    return rb->length;
}

uint32_t hl_ringbuffer_get_size(hl_ringbuffer_t ringbuffer)
{
    HL_ASSERT(ringbuffer != NULL);
    ringbuffer_t *rb = (ringbuffer_t *)ringbuffer;
    return rb->size;
}

int hl_ringbuffer_db_open(const char *db_file, hl_ringbuffer_db_t *ringbuffer_db, uint32_t size, uint32_t length)
{
    HL_ASSERT(ringbuffer_db != NULL);
    ringbuffer_db_t *db;
    ringbuffer_t *rb;
    int ret = 0;
    if ((db = (ringbuffer_db_t *)malloc(sizeof(ringbuffer_db_t))) == NULL)
        return -1;

    if ((rb = (ringbuffer_t *)malloc(sizeof(ringbuffer_t))) == NULL)
    {
        free(db);
        return -1;
    }

    db->rb = rb;
    if (ringbuffer_db_verify(db_file, size, length) == 0 && (db->fp = fopen(db_file, "rb+")) != NULL)
        ret = ringbuffer_db_init(db);
    else if ((db->fp = fopen(db_file, "wb+")) != NULL)
        ret = ringbuffer_db_create(db, size, length);
    else
    {
        printf("fopen failed: %s\n", strerror(errno));
        free(rb);
        free(db);
        return -1;
    }

    if (ret != 0)
    {
        printf("hl_ringbuffer_db_open failed\n");
        fclose(db->fp);
        free(rb);
        free(db);
        return -1;
    }

    *ringbuffer_db = db;
    return 0;
}

void hl_ringbuffer_db_close(hl_ringbuffer_db_t *ringbuffer_db)
{
    HL_ASSERT(ringbuffer_db != NULL);
    HL_ASSERT(*ringbuffer_db != NULL);
    ringbuffer_db_t *db = (ringbuffer_db_t *)(*ringbuffer_db);
    fflush(db->fp);
    fsync(fileno(db->fp));
    fclose(db->fp);
    free(db->rb->buf);
    free(db->rb);
    free(db);
}

void hl_ringbuffer_db_sync(hl_ringbuffer_db_t ringbuffer_db)
{
    HL_ASSERT(ringbuffer_db != NULL);
    ringbuffer_db_t *db = (ringbuffer_db_t *)(ringbuffer_db);
    ringbuffer_t *rb = db->rb;
    ringbuffer_db_header_t header;
    header.index = rb->index;
    header.available_length = rb->available_length;
    header.length = rb->length;
    header.size = rb->size;
    ringbuffer_db_digest(db, &header);
    printf("digest:");
    for (int i = 0; i < 16; i++)
        printf("%x ", header.digest[i]);
    printf("\n");
    fseek(db->fp, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(header), db->fp);
    fwrite(rb->buf, 1, rb->size * rb->length, db->fp);
    fflush(db->fp);
    fsync(fileno(db->fp));
    ringbuffer_db_verify(WLAN_ENTRY_FILE_PATH, rb->size, rb->length);
}

hl_ringbuffer_t hl_ringbuffer_db_get(hl_ringbuffer_db_t ringbuffer_db)
{
    HL_ASSERT(ringbuffer_db != NULL);
    ringbuffer_db_t *db = (ringbuffer_db_t *)(ringbuffer_db);
    return db->rb;
}

static inline uint32_t ringbuffer_index_to_pos(ringbuffer_t *rb, uint32_t index)
{
    return (rb->index - rb->available_length + index) % rb->length;
}

static int ringbuffer_db_verify(const char *db_file, uint32_t size, uint32_t length)
{
    FILE *fp;
    MD5_CTX ctx;
    uint8_t digest[MD5_DIGEST_LENGTH];
    uint8_t buffer[1024];
    int len;
    ringbuffer_db_header_t header;

    fp = fopen(db_file, "rb+");
    if (fp == NULL)
        return -1;

    if (fread(&header, 1, sizeof(header), fp) != sizeof(header))
    {
        fclose(fp);
        return -1;
    }

    memcpy(digest, header.digest, sizeof(digest));
    printf("#1#verify digest:");
    for (int i = 0; i < 16; i++)
        printf("%x ", digest[i]);
    printf("\n");

    MD5_Init(&ctx);
    memset(header.digest, 0, sizeof(header.digest));
    MD5_Update(&ctx, (uint8_t *)&header, sizeof(header));
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        MD5_Update(&ctx, buffer, len);
    MD5_Final(header.digest, &ctx);
    fclose(fp);
    printf("#2#verify digest:");
    for (int i = 0; i < 16; i++)
        printf("%x ", header.digest[i]);
    printf("\n");
    if (header.size != size || header.length != length)
        return -1;

    return memcmp(header.digest, digest, sizeof(header.digest)) == 0 ? 0 : -1;
}

static void ringbuffer_db_digest(ringbuffer_db_t *db, ringbuffer_db_header_t *header)
{
    MD5_CTX ctx;
    MD5_Init(&ctx);
    memset(header->digest, 0, sizeof(header->digest));
    MD5_Update(&ctx, (uint8_t *)header, sizeof(*header));
    MD5_Update(&ctx, db->rb->buf, db->rb->length * db->rb->size);
    MD5_Final(header->digest, &ctx);
}

static int ringbuffer_db_init(ringbuffer_db_t *db)
{
    ringbuffer_t *rb = db->rb;
    ringbuffer_db_header_t header;
    if (fseek(db->fp, 0, SEEK_SET) == -1)
        return -1;
    if (fread(&header, 1, sizeof(header), db->fp) != sizeof(header))
        return -1;

    rb->index = header.index;
    rb->available_length = header.available_length;
    rb->length = header.length;
    rb->size = header.size;
    if ((rb->buf = malloc(rb->size * rb->length)) == NULL)
        return -1;

    if (fread(rb->buf, 1, rb->size * rb->length, db->fp) != rb->size * rb->length)
    {
        free(rb->buf);
        return -1;
    }

    return 0;
}

static int ringbuffer_db_create(ringbuffer_db_t *db, uint32_t size, uint32_t length)
{
    ringbuffer_t *rb = db->rb;
    ringbuffer_db_header_t header;
    if (fseek(db->fp, 0, SEEK_SET) == -1)
        return -1;
    header.index = rb->index = 0;
    header.available_length = rb->available_length = 0;
    header.length = rb->length = length;
    header.size = rb->size = size;
    if ((rb->buf = malloc(rb->size * rb->length)) == NULL)
        return -1;
    memset(rb->buf, 0, rb->size * rb->length);
    ringbuffer_db_digest(db, &header);

    fwrite(&header, 1, sizeof(header), db->fp);
    fwrite(rb->buf, 1, rb->size * rb->length, db->fp);

    fflush(db->fp);
    fsync(fileno(db->fp));
    return 0;
}
