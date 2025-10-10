#include "utils_params.h"
#include "crc16.h"

int utils_params_read(void *params, uint32_t size, const char *path)
{
    FILE *fp = NULL;
    utils_params_header_t *header;
    uint16_t crc;
    int ret = 0;

    if (UTILS_CHECK(params != NULL))
        return -1;
    if (UTILS_CHECK(path != NULL))
        return -2;
    if (UTILS_CHECK(strlen(path) > 0))
        return -3;
    fp = fopen(path, "rb");
    if (UTILS_CHECK_ERRNO(fp != NULL))
        return -4;
    if (UTILS_CHECK_ERRNO(fread(params, 1, size, fp) == size))
        ret = -5;
    header = params;
    if (UTILS_CHECK(header->magic == PARAMS_FILE_MAGIC))
        ret = -6;
    crc = crc_16((uint8_t *)params + sizeof(utils_params_header_t), size - sizeof(utils_params_header_t));
    if (UTILS_CHECK(header->crc == crc))
        ret = -7;
    fclose(fp);
    return ret;
}

int utils_params_write(void *params, uint32_t size, const char *path)
{
    FILE *fp = NULL;
    utils_params_header_t *header;
    int ret = 0;
    if (UTILS_CHECK(params != NULL))
        return -1;
    if (UTILS_CHECK(path != NULL))
        return -2;
    if (UTILS_CHECK(strlen(path) > 0))
        return -3;
    fp = fopen(path, "wb+");
    if (UTILS_CHECK_ERRNO(fp != NULL))
        return -4;
    header = params;
    header->magic = PARAMS_FILE_MAGIC;
    header->size = size - sizeof(common_header_t);
    header->crc = crc_16((uint8_t *)params + sizeof(utils_params_header_t), size - sizeof(utils_params_header_t));
    if (UTILS_CHECK_ERRNO(fwrite(params, 1, size, fp) == size))
        ret = -5;
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
    sync();
    return ret;
}
